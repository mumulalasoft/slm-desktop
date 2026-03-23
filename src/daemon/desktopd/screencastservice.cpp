#include "screencastservice.h"
#include "screencaststreambackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <algorithm>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Screencast";
constexpr const char kPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kApiVersion[] = "1.0";
}

ScreencastService::ScreencastService(QObject *parent)
    : QObject(parent)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    registerDbusService();
    attachStreamBackend();
}

ScreencastService::~ScreencastService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool ScreencastService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString ScreencastService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap ScreencastService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("stream_backend_requested"), m_streamBackendRequestedMode},
        {QStringLiteral("stream_backend"), m_streamBackendMode},
        {QStringLiteral("stream_backend_fallback_reason"), m_streamBackendFallbackReason},
        {QStringLiteral("pipewire_build_available"), ScreencastStreamBackend::isPipeWireBuildAvailable()},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), QVariantMap{}},
    };
}

QVariantMap ScreencastService::GetState() const
{
    QStringList sessions = m_sessionStreams.keys();
    std::sort(sessions.begin(), sessions.end());
    QVariantList items;
    for (const QString &session : sessions) {
        items.push_back(QVariantMap{
            {QStringLiteral("session"), session},
            {QStringLiteral("stream_count"), m_sessionStreams.value(session).size()},
        });
    }
    return success({
        {QStringLiteral("active"), !sessions.isEmpty()},
        {QStringLiteral("active_count"), sessions.size()},
        {QStringLiteral("active_sessions"), sessions},
        {QStringLiteral("active_session_items"), items},
        {QStringLiteral("stream_backend_requested"), m_streamBackendRequestedMode},
        {QStringLiteral("stream_backend"), m_streamBackendMode},
        {QStringLiteral("stream_backend_fallback_reason"), m_streamBackendFallbackReason},
        {QStringLiteral("pipewire_build_available"), ScreencastStreamBackend::isPipeWireBuildAvailable()},
        {QStringLiteral("stream_backend_ready"), m_streamBackend != nullptr},
    });
}

void ScreencastService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for ScreencastService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Screencast"), QStringLiteral("GetSessionStreams"), Slm::Permissions::Capability::ScreencastStart);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Screencast"), QStringLiteral("UpdateSessionStreams"), Slm::Permissions::Capability::ScreencastSelectSources);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Screencast"), QStringLiteral("EndSession"), Slm::Permissions::Capability::ScreencastStop);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Screencast"), QStringLiteral("RevokeSession"), Slm::Permissions::Capability::ScreencastStop);
}

bool ScreencastService::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
{
    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("High"));

    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (!d.isAllowed()) {
        sendErrorReply(QStringLiteral("org.slm.PermissionDenied"), d.reason);
        return false;
    }
    return true;
}

QVariantMap ScreencastService::GetSessionStreams(const QString &sessionPath) const
{
    if (!const_cast<ScreencastService*>(this)->checkPermission(Slm::Permissions::Capability::ScreencastStart, QStringLiteral("GetSessionStreams"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    return success({
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("streams"), m_sessionStreams.value(session)},
    });
}

QVariantMap ScreencastService::UpdateSessionStreams(const QString &sessionPath, const QVariantList &streams)
{
    if (!checkPermission(Slm::Permissions::Capability::ScreencastSelectSources, QStringLiteral("UpdateSessionStreams"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    const QVariantList normalized = normalizeStreams(streams);
    if (normalized.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-streams"));
    }

    m_sessionStreams.insert(session, normalized);
    emit SessionStreamsChanged(session, normalized);
    return success({
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("stream_count"), normalized.size()},
        {QStringLiteral("streams"), normalized},
    });
}

QVariantMap ScreencastService::EndSession(const QString &sessionPath)
{
    if (!checkPermission(Slm::Permissions::Capability::ScreencastStop, QStringLiteral("EndSession"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    const bool existed = m_sessionStreams.remove(session) > 0;
    emit SessionEnded(session);
    return success({
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("ended"), true},
        {QStringLiteral("had_streams"), existed},
    });
}

QVariantMap ScreencastService::RevokeSession(const QString &sessionPath, const QString &reason)
{
    if (!checkPermission(Slm::Permissions::Capability::ScreencastStop, QStringLiteral("RevokeSession"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    const QString normalizedReason = reason.trimmed().isEmpty() ? QStringLiteral("revoked")
                                                                : reason.trimmed();
    const bool existed = m_sessionStreams.remove(session) > 0;
    emit SessionRevoked(session, normalizedReason);
    return success({
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("revoked"), true},
        {QStringLiteral("had_streams"), existed},
        {QStringLiteral("reason"), normalizedReason},
    });
}

void ScreencastService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots
                                           | QDBusConnection::ExportAllSignals
                                           | QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

void ScreencastService::attachStreamBackend()
{
    m_streamBackend = ScreencastStreamBackend::createFromEnvironment(this, &m_streamBackendRequestedMode);
    m_streamBackendMode = m_streamBackendRequestedMode;
    m_streamBackendFallbackReason.clear();
    if (!m_streamBackend) {
        m_streamBackendFallbackReason = QStringLiteral("backend-instance-create-failed");
        m_streamBackendMode = QStringLiteral("portal-mirror");
        m_streamBackend = std::make_unique<ScreencastStreamBackend>(
            ScreencastStreamBackend::Mode::PortalMirror,
            this);
    }
    connect(m_streamBackend.get(),
            &ScreencastStreamBackend::sessionStreamsChanged,
            this,
            &ScreencastService::onBackendSessionStreamsChanged);
    connect(m_streamBackend.get(),
            &ScreencastStreamBackend::sessionEnded,
            this,
            &ScreencastService::onBackendSessionEnded);
    connect(m_streamBackend.get(),
            &ScreencastStreamBackend::sessionRevoked,
            this,
            &ScreencastService::onBackendSessionRevoked);
    const bool started = m_streamBackend->start();
    if (!started && m_streamBackend->mode() != ScreencastStreamBackend::Mode::PortalMirror) {
        // Fail-open to portal-mirror to keep runtime stable while native backend is developed.
        m_streamBackendFallbackReason = m_streamBackend->lastError().trimmed();
        if (m_streamBackendFallbackReason.isEmpty()) {
            m_streamBackendFallbackReason = QStringLiteral("backend-start-failed");
        }
        m_streamBackendMode = QStringLiteral("portal-mirror");
        m_streamBackend = std::make_unique<ScreencastStreamBackend>(
            ScreencastStreamBackend::Mode::PortalMirror,
            this);
        connect(m_streamBackend.get(),
                &ScreencastStreamBackend::sessionStreamsChanged,
                this,
                &ScreencastService::onBackendSessionStreamsChanged);
        connect(m_streamBackend.get(),
                &ScreencastStreamBackend::sessionEnded,
                this,
                &ScreencastService::onBackendSessionEnded);
        connect(m_streamBackend.get(),
                &ScreencastStreamBackend::sessionRevoked,
                this,
                &ScreencastService::onBackendSessionRevoked);
        if (!m_streamBackend->start() && m_streamBackendFallbackReason.isEmpty()) {
            m_streamBackendFallbackReason = QStringLiteral("portal-mirror-start-failed");
        }
    } else if (!started && m_streamBackend->mode() == ScreencastStreamBackend::Mode::PortalMirror) {
        m_streamBackendFallbackReason = m_streamBackend->lastError().trimmed();
    }
}

void ScreencastService::onBackendSessionStreamsChanged(const QString &sessionPath,
                                                       const QVariantList &streams)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return;
    }
    const QVariantList normalized = normalizeStreams(streams);
    if (normalized.isEmpty()) {
        return;
    }
    m_sessionStreams.insert(session, normalized);
    emit SessionStreamsChanged(session, normalized);
}

void ScreencastService::onBackendSessionEnded(const QString &sessionPath)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return;
    }
    if (m_sessionStreams.remove(session) > 0) {
        emit SessionEnded(session);
    }
}

void ScreencastService::onBackendSessionRevoked(const QString &sessionPath, const QString &reason)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return;
    }
    const QString normalizedReason = reason.trimmed().isEmpty() ? QStringLiteral("revoked")
                                                                : reason.trimmed();
    m_sessionStreams.remove(session);
    emit SessionRevoked(session, normalizedReason);
}

QVariantMap ScreencastService::invalidArgument(const QString &reason)
{
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("invalid-argument")},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("response"), 3u},
        {QStringLiteral("results"), QVariantMap{{QStringLiteral("reason"), reason}}},
    };
}

QVariantMap ScreencastService::success(const QVariantMap &results)
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), results},
    };
}

QVariantList ScreencastService::normalizeStreams(const QVariantList &streams)
{
    QVariantList out;
    for (const QVariant &value : streams) {
        QVariantMap stream = value.toMap();
        if (stream.isEmpty()) {
            const QVariantList tuple = value.toList();
            if (tuple.size() >= 2) {
                const QVariant first = tuple.constFirst();
                const QVariantMap payload = tuple.at(1).toMap();
                stream = payload;
                if (!stream.contains(QStringLiteral("node_id")) && first.isValid()) {
                    stream.insert(QStringLiteral("node_id"), first);
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
