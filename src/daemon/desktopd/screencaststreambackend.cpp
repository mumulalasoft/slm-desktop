#include "screencaststreambackend.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QHash>
#include <algorithm>

#ifdef SLM_HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <pipewire/keys.h>
#include <spa/utils/dict.h>
#include <spa/utils/hook.h>
#endif

namespace {
constexpr const char kPortalService[] = "org.slm.Desktop.Portal";
constexpr const char kPortalPath[] = "/org/slm/Desktop/Portal";
constexpr const char kPortalIface[] = "org.slm.Desktop.Portal";
constexpr const char kImplPortalService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kImplPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kImplPortalIface[] = "org.freedesktop.impl.portal.desktop.slm";
}

#ifdef SLM_HAVE_PIPEWIRE
struct ScreencastStreamBackend::PipeWireState
{
    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_registry *registry = nullptr;
    spa_hook registryListener{};
    QHash<uint32_t, QVariantMap> nodes;
};

static bool isLikelyScreencastNode(const QVariantMap &node)
{
    const QString mediaClass = node.value(QStringLiteral("media_class")).toString();
    const QString nodeName = node.value(QStringLiteral("node_name")).toString().toLower();
    const QString appName = node.value(QStringLiteral("app_name")).toString().toLower();
    if (mediaClass.startsWith(QStringLiteral("Stream/Output/Video"))) {
        return true;
    }
    if (mediaClass.contains(QStringLiteral("Video"))) {
        return true;
    }
    return nodeName.contains(QStringLiteral("xdp"))
        || nodeName.contains(QStringLiteral("portal"))
        || nodeName.contains(QStringLiteral("screencast"))
        || appName.contains(QStringLiteral("xdg-desktop-portal"))
        || appName.contains(QStringLiteral("slm"));
}

static void onRegistryGlobal(void *data,
                             uint32_t id,
                             uint32_t,
                             const char *type,
                             uint32_t,
                             const struct spa_dict *props)
{
    auto *self = static_cast<ScreencastStreamBackend *>(data);
    if (!self || !type || QString::fromLatin1(type) != QString::fromLatin1(PW_TYPE_INTERFACE_Node)) {
        return;
    }
    QVariantMap node;
    node.insert(QStringLiteral("node_id"), static_cast<quint32>(id));
    if (props) {
        if (const char *mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)) {
            node.insert(QStringLiteral("media_class"), QString::fromUtf8(mediaClass));
        }
        if (const char *nodeName = spa_dict_lookup(props, PW_KEY_NODE_NAME)) {
            node.insert(QStringLiteral("node_name"), QString::fromUtf8(nodeName));
        }
        if (const char *appName = spa_dict_lookup(props, PW_KEY_APP_NAME)) {
            node.insert(QStringLiteral("app_name"), QString::fromUtf8(appName));
        }
    }
    self->upsertPipeWireNode(id, node);
}

static void onRegistryGlobalRemove(void *data, uint32_t id)
{
    auto *self = static_cast<ScreencastStreamBackend *>(data);
    if (!self) {
        return;
    }
    self->removePipeWireNode(id);
}

static const pw_registry_events kRegistryEvents = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = onRegistryGlobal,
    .global_remove = onRegistryGlobalRemove,
};
#endif

bool ScreencastStreamBackend::isPipeWireBuildAvailable()
{
#ifdef SLM_HAVE_PIPEWIRE
    return true;
#else
    return false;
#endif
}

ScreencastStreamBackend::ScreencastStreamBackend(Mode mode, QObject *parent)
    : QObject(parent)
    , m_mode(mode)
{
}

ScreencastStreamBackend::~ScreencastStreamBackend()
{
#ifdef SLM_HAVE_PIPEWIRE
    shutdownPipeWire();
#endif
}

bool ScreencastStreamBackend::start()
{
    if (m_mode == Mode::PipeWire) {
#ifndef SLM_HAVE_PIPEWIRE
        m_lastError = QStringLiteral("pipewire-backend-not-built");
        qWarning().noquote() << "[screencast] backend=pipewire requested but PipeWire build support is unavailable";
        return false;
#else
        if (!initPipeWire()) {
            return false;
        }
#endif
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_lastError = QStringLiteral("session-bus-unavailable");
        return false;
    }
    const bool stateOk = bus.connect(QString::fromLatin1(kImplPortalService),
                                     QString::fromLatin1(kImplPortalPath),
                                     QString::fromLatin1(kImplPortalIface),
                                     QStringLiteral("ScreencastSessionStateChanged"),
                                     this,
                                     SLOT(onPortalSessionStateChanged(QString,QString,bool,int)));
    const bool revokeOk = bus.connect(QString::fromLatin1(kImplPortalService),
                                      QString::fromLatin1(kImplPortalPath),
                                      QString::fromLatin1(kImplPortalIface),
                                      QStringLiteral("ScreencastSessionRevoked"),
                                      this,
                                      SLOT(onPortalSessionRevoked(QString,QString,QString,int)));
    if (!(stateOk && revokeOk)) {
        m_lastError = QStringLiteral("impl-portal-signal-connect-failed");
        return false;
    }
    m_lastError.clear();
    return true;
}

QString ScreencastStreamBackend::modeName() const
{
    return m_mode == Mode::PipeWire ? QStringLiteral("pipewire")
                                    : QStringLiteral("portal-mirror");
}

std::unique_ptr<ScreencastStreamBackend> ScreencastStreamBackend::createFromEnvironment(
    QObject *parent, QString *selectedMode)
{
    const QString requested =
        qEnvironmentVariable("SLM_SCREENCAST_STREAM_BACKEND").trimmed().toLower();
    if (requested == QStringLiteral("pipewire")) {
        if (selectedMode) {
            *selectedMode = QStringLiteral("pipewire");
        }
        return std::make_unique<ScreencastStreamBackend>(Mode::PipeWire, parent);
    }
    if (selectedMode) {
        *selectedMode = QStringLiteral("portal-mirror");
    }
    return std::make_unique<ScreencastStreamBackend>(Mode::PortalMirror, parent);
}

QVariantList ScreencastStreamBackend::normalizeStreams(const QVariantList &streams)
{
    QVariantList out;
    for (const QVariant &value : streams) {
        QVariantMap stream = value.toMap();
        if (stream.isEmpty()) {
            const QVariantList tuple = value.toList();
            if (tuple.size() >= 2) {
                stream = tuple.at(1).toMap();
                if (!stream.contains(QStringLiteral("node_id")) && tuple.constFirst().isValid()) {
                    stream.insert(QStringLiteral("node_id"), tuple.constFirst());
                }
            }
        }
        if (stream.isEmpty()) {
            continue;
        }
        if (!stream.contains(QStringLiteral("node_id"))
            && stream.contains(QStringLiteral("pipewire_node_id"))) {
            stream.insert(QStringLiteral("node_id"), stream.value(QStringLiteral("pipewire_node_id")));
        }
        if (!stream.contains(QStringLiteral("source_type"))) {
            stream.insert(QStringLiteral("source_type"), QStringLiteral("monitor"));
        }
        if (!stream.contains(QStringLiteral("cursor_mode"))) {
            stream.insert(QStringLiteral("cursor_mode"), QStringLiteral("embedded"));
        }
        out.push_back(stream);
    }
    return out;
}

QVariantList ScreencastStreamBackend::queryPortalSessionStreams(const QString &sessionPath)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return {};
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return {};
    }
    QDBusInterface iface(QString::fromLatin1(kPortalService),
                         QString::fromLatin1(kPortalPath),
                         QString::fromLatin1(kPortalIface),
                         bus);
    if (!iface.isValid()) {
        return {};
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetScreencastSessionStreams"), session);
    if (!reply.isValid()) {
        return {};
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok")).toBool()) {
        return {};
    }
    return normalizeStreams(
        out.value(QStringLiteral("results")).toMap().value(QStringLiteral("streams")).toList());
}

QString ScreencastStreamBackend::normalizedHintText(const QVariant &value)
{
    return value.toString().trimmed().toLower();
}

ScreencastStreamBackend::SessionHints
ScreencastStreamBackend::extractSessionHints(const QVariantList &streams)
{
    SessionHints hints;
    for (const QVariant &value : streams) {
        const QVariantMap stream = value.toMap();
        if (stream.isEmpty()) {
            continue;
        }
        bool ok = false;
        const uint fromNode = stream.value(QStringLiteral("node_id")).toUInt(&ok);
        if (ok && fromNode > 0u) {
            hints.nodeIds.insert(fromNode);
        } else {
            const uint fromPwNode = stream.value(QStringLiteral("pipewire_node_id")).toUInt(&ok);
            if (ok && fromPwNode > 0u) {
                hints.nodeIds.insert(fromPwNode);
            }
        }
        const QString nodeName = normalizedHintText(stream.value(QStringLiteral("node_name")));
        if (!nodeName.isEmpty()) {
            hints.nodeNames.insert(nodeName);
        }
        const QString appName = normalizedHintText(stream.value(QStringLiteral("app_name")));
        if (!appName.isEmpty()) {
            hints.appNames.insert(appName);
        }
        const QString sourceType = normalizedHintText(stream.value(QStringLiteral("source_type")));
        if (!sourceType.isEmpty()) {
            hints.sourceTypes.insert(sourceType);
        }
    }
    return hints;
}

int ScreencastStreamBackend::hintMatchScore(const QVariantMap &stream, const SessionHints &hints)
{
    if (!hints.nodeIds.isEmpty()) {
        bool ok = false;
        const uint nodeId = stream.value(QStringLiteral("node_id")).toUInt(&ok);
        if (ok && nodeId > 0u && hints.nodeIds.contains(nodeId)) {
            return 1000;
        }
        const uint pwNodeId = stream.value(QStringLiteral("pipewire_node_id")).toUInt(&ok);
        return (ok && pwNodeId > 0u && hints.nodeIds.contains(pwNodeId)) ? 1000 : 0;
    }

    int score = 0;
    if (!hints.nodeNames.isEmpty()) {
        const QString nodeName = normalizedHintText(stream.value(QStringLiteral("node_name")));
        for (const QString &hint : hints.nodeNames) {
            if (nodeName.isEmpty()) {
                continue;
            }
            if (nodeName == hint) {
                score = std::max(score, 140);
            } else if (nodeName.contains(hint) || hint.contains(nodeName)) {
                score = std::max(score, 90);
            }
        }
    }
    if (!hints.appNames.isEmpty()) {
        const QString appName = normalizedHintText(stream.value(QStringLiteral("app_name")));
        for (const QString &hint : hints.appNames) {
            if (appName.isEmpty()) {
                continue;
            }
            if (appName == hint) {
                score = std::max(score, 120);
            } else if (appName.contains(hint) || hint.contains(appName)) {
                score = std::max(score, 70);
            }
        }
    }
    if (!hints.sourceTypes.isEmpty()) {
        const QString sourceType = normalizedHintText(stream.value(QStringLiteral("source_type")));
        if (hints.sourceTypes.contains(sourceType)) {
            score += 30;
        } else {
            // Source type mismatch penalized hard to avoid cross-session picks.
            score -= 40;
        }
    }

    // Keep minimum confidence to reduce false positives in dense multi-stream environments.
    return score >= 80 ? score : 0;
}

void ScreencastStreamBackend::onPortalSessionStateChanged(const QString &sessionHandle,
                                                          const QString &,
                                                          bool active,
                                                          int)
{
    const QString session = sessionHandle.trimmed();
    if (session.isEmpty()) {
        return;
    }
    if (!active) {
        m_sessionNodeHints.remove(session);
        emit sessionEnded(session);
        return;
    }

    const QVariantList portalStreams = queryPortalSessionStreams(session);
    m_sessionNodeHints.insert(session, extractSessionHints(portalStreams));

    QVariantList streams;
    if (m_mode == Mode::PipeWire) {
        streams = correlateSessionPipeWireStreams(session, portalStreams);
    }
    if (streams.isEmpty()) {
        streams = portalStreams;
    }
    if (streams.isEmpty()) {
        return;
    }
    emit sessionStreamsChanged(session, streams);
}

void ScreencastStreamBackend::onPortalSessionRevoked(const QString &sessionHandle,
                                                     const QString &,
                                                     const QString &reason,
                                                     int)
{
    const QString session = sessionHandle.trimmed();
    if (session.isEmpty()) {
        return;
    }
    const QString normalizedReason = reason.trimmed().isEmpty() ? QStringLiteral("revoked")
                                                                : reason.trimmed();
    m_sessionNodeHints.remove(session);
    emit sessionRevoked(session, normalizedReason);
}

QVariantList ScreencastStreamBackend::currentPipeWireStreams() const
{
#ifdef SLM_HAVE_PIPEWIRE
    QVariantList streams;
    if (!m_pw) {
        return streams;
    }
    for (const QVariantMap &node : m_pw->nodes) {
        if (!isLikelyScreencastNode(node)) {
            continue;
        }
        QVariantMap stream;
        stream.insert(QStringLiteral("node_id"), node.value(QStringLiteral("node_id")));
        stream.insert(QStringLiteral("pipewire_node_id"), node.value(QStringLiteral("node_id")));
        stream.insert(QStringLiteral("source_type"), QStringLiteral("monitor"));
        stream.insert(QStringLiteral("cursor_mode"), QStringLiteral("embedded"));
        if (node.contains(QStringLiteral("node_name"))) {
            stream.insert(QStringLiteral("node_name"), node.value(QStringLiteral("node_name")));
        }
        if (node.contains(QStringLiteral("app_name"))) {
            stream.insert(QStringLiteral("app_name"), node.value(QStringLiteral("app_name")));
        }
        stream.insert(QStringLiteral("provider"), QStringLiteral("pipewire-registry"));
        streams.push_back(stream);
    }
    return streams;
#else
    return {};
#endif
}

QVariantList ScreencastStreamBackend::correlateSessionPipeWireStreams(
    const QString &sessionPath, const QVariantList &portalStreams) const
{
    const QVariantList pipewireStreams = currentPipeWireStreams();
    if (pipewireStreams.isEmpty()) {
        return {};
    }

    SessionHints hints = extractSessionHints(portalStreams);
    if (hints.isEmpty()) {
        hints = m_sessionNodeHints.value(sessionPath);
    }
    if (hints.isEmpty()) {
        // No session-scoped hint available: let caller fall back to portal metadata instead
        // of returning global PipeWire candidates.
        return {};
    }

    QList<QPair<int, QVariantMap>> ranked;
    int bestScore = 0;
    for (const QVariant &value : pipewireStreams) {
        const QVariantMap stream = value.toMap();
        if (stream.isEmpty()) {
            continue;
        }
        const int score = hintMatchScore(stream, hints);
        if (score <= 0) {
            continue;
        }
        bestScore = std::max(bestScore, score);
        ranked.push_back(qMakePair(score, stream));
    }
    if (!ranked.isEmpty()) {
        // Return only top band to reduce noisy matches while still allowing multi-output sessions.
        std::stable_sort(ranked.begin(), ranked.end(),
                         [](const auto &a, const auto &b) { return a.first > b.first; });
        QVariantList matched;
        const int cutoff = std::max(100, bestScore - 20);
        for (const auto &pair : ranked) {
            if (pair.first < cutoff) {
                continue;
            }
            QVariantMap stream = pair.second;
            stream.insert(QStringLiteral("match_score"), pair.first);
            matched.push_back(stream);
        }
        return matched;
    }
    // Session hints exist but none match live PipeWire nodes; avoid global fallback.
    return {};
}

#ifdef SLM_HAVE_PIPEWIRE
bool ScreencastStreamBackend::initPipeWire()
{
    if (m_pipewireInitialized) {
        return true;
    }
    pw_init(nullptr, nullptr);
    m_pipewireInitialized = true;

    m_pw = std::make_unique<PipeWireState>();
    m_pw->loop = pw_thread_loop_new("slm-screencast-pw", nullptr);
    if (!m_pw->loop) {
        m_lastError = QStringLiteral("pw-thread-loop-create-failed");
        shutdownPipeWire();
        return false;
    }
    if (pw_thread_loop_start(m_pw->loop) < 0) {
        m_lastError = QStringLiteral("pw-thread-loop-start-failed");
        shutdownPipeWire();
        return false;
    }
    pw_thread_loop_lock(m_pw->loop);
    m_pw->context = pw_context_new(pw_thread_loop_get_loop(m_pw->loop), nullptr, 0);
    if (!m_pw->context) {
        pw_thread_loop_unlock(m_pw->loop);
        m_lastError = QStringLiteral("pw-context-create-failed");
        shutdownPipeWire();
        return false;
    }
    m_pw->core = pw_context_connect(m_pw->context, nullptr, 0);
    if (!m_pw->core) {
        pw_thread_loop_unlock(m_pw->loop);
        m_lastError = QStringLiteral("pw-core-connect-failed");
        shutdownPipeWire();
        return false;
    }
    m_pw->registry =
        static_cast<pw_registry *>(pw_core_get_registry(m_pw->core, PW_VERSION_REGISTRY, 0));
    if (!m_pw->registry) {
        pw_thread_loop_unlock(m_pw->loop);
        m_lastError = QStringLiteral("pw-registry-get-failed");
        shutdownPipeWire();
        return false;
    }
    pw_registry_add_listener(m_pw->registry, &m_pw->registryListener, &kRegistryEvents, this);
    pw_thread_loop_unlock(m_pw->loop);
    m_lastError.clear();
    return true;
}

void ScreencastStreamBackend::shutdownPipeWire()
{
    if (!m_pw) {
        if (m_pipewireInitialized) {
            pw_deinit();
            m_pipewireInitialized = false;
        }
        return;
    }
    if (m_pw->loop) {
        pw_thread_loop_lock(m_pw->loop);
    }
    spa_hook_remove(&m_pw->registryListener);
    if (m_pw->registry) {
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_pw->registry));
        m_pw->registry = nullptr;
    }
    if (m_pw->core) {
        pw_core_disconnect(m_pw->core);
        m_pw->core = nullptr;
    }
    if (m_pw->context) {
        pw_context_destroy(m_pw->context);
        m_pw->context = nullptr;
    }
    if (m_pw->loop) {
        pw_thread_loop_unlock(m_pw->loop);
        pw_thread_loop_stop(m_pw->loop);
        pw_thread_loop_destroy(m_pw->loop);
        m_pw->loop = nullptr;
    }
    m_pw.reset();
    if (m_pipewireInitialized) {
        pw_deinit();
        m_pipewireInitialized = false;
    }
}

void ScreencastStreamBackend::upsertPipeWireNode(uint32_t id, const QVariantMap &node)
{
    if (!m_pw) {
        return;
    }
    m_pw->nodes.insert(id, node);
}

void ScreencastStreamBackend::removePipeWireNode(uint32_t id)
{
    if (!m_pw) {
        return;
    }
    m_pw->nodes.remove(id);
}
#endif
