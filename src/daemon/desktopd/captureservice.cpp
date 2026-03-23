#include "captureservice.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Capture";
constexpr const char kPath[] = "/org/slm/Desktop/Capture";
constexpr const char kApiVersion[] = "1.0";
constexpr const char kPortalService[] = "org.slm.Desktop.Portal";
constexpr const char kPortalPath[] = "/org/slm/Desktop/Portal";
constexpr const char kPortalIface[] = "org.slm.Desktop.Portal";
constexpr const char kScreencastService[] = "org.slm.Desktop.Screencast";
constexpr const char kScreencastPath[] = "/org/slm/Desktop/Screencast";
constexpr const char kScreencastIface[] = "org.slm.Desktop.Screencast";
constexpr const char kImplPortalService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kImplPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kImplPortalIface[] = "org.freedesktop.impl.portal.desktop.slm";
}

CaptureService::CaptureService(QObject *parent)
    : QObject(parent)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    registerDbusService();
    connectPortalSignals();
}

CaptureService::~CaptureService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool CaptureService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString CaptureService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap CaptureService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), QVariantMap{}},
    };
}

QVariantMap CaptureService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("screencast_streams"),
             QStringLiteral("session_stream_upsert"),
             QStringLiteral("session_clear"),
             QStringLiteral("session_revoke"),
        }},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), QVariantMap{}},
    };
}

void CaptureService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for CaptureService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Capture"), QStringLiteral("GetScreencastStreams"), Slm::Permissions::Capability::ScreencastStart);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Capture"), QStringLiteral("SetScreencastSessionStreams"), Slm::Permissions::Capability::ScreencastSelectSources);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Capture"), QStringLiteral("ClearScreencastSession"), Slm::Permissions::Capability::ScreencastStop);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.Capture"), QStringLiteral("RevokeScreencastSession"), Slm::Permissions::Capability::ScreencastStop);
}

bool CaptureService::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
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

QVariantMap CaptureService::GetScreencastStreams(const QString &sessionPath,
                                                 const QString &appId,
                                                 const QVariantMap &options)
{
    if (!checkPermission(Slm::Permissions::Capability::ScreencastStart, QStringLiteral("GetScreencastStreams"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }

    QVariantList streams = normalizeStreamsFromOptions(options);
    QString provider = QStringLiteral("options");

    if (!streams.isEmpty()) {
        m_sessionStreams.insert(session, streams);
    } else if (m_sessionStreams.contains(session)) {
        streams = m_sessionStreams.value(session);
        provider = QStringLiteral("cache");
    } else if (const QVariantList fromScreencast = queryStreamsFromScreencastService(session);
               !fromScreencast.isEmpty()) {
        streams = fromScreencast;
        provider = QStringLiteral("screencast-session");
        m_sessionStreams.insert(session, streams);
    } else if (const QVariantList fromPortal = queryStreamsFromPortalService(session); !fromPortal.isEmpty()) {
        streams = fromPortal;
        provider = QStringLiteral("portal-session");
        m_sessionStreams.insert(session, streams);
    } else {
        provider = QStringLiteral("none");
    }

    QVariantMap results{
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("streams"), streams},
        {QStringLiteral("stream_provider"), provider},
    };
    return success(results);
}

QVariantMap CaptureService::SetScreencastSessionStreams(const QString &sessionPath,
                                                        const QVariantList &streams,
                                                        const QVariantMap &options)
{
    if (!checkPermission(Slm::Permissions::Capability::ScreencastSelectSources, QStringLiteral("SetScreencastSessionStreams"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }

    QVariantMap normalizeOptions = options;
    normalizeOptions.insert(QStringLiteral("streams"), streams);
    const QVariantList normalized = normalizeStreamsFromOptions(normalizeOptions);
    if (normalized.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-streams"));
    }

    m_sessionStreams.insert(session, normalized);
    emit ScreencastSessionStreamsChanged(session, normalized.size());
    return success({
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("stream_provider"), QStringLiteral("session-upsert")},
        {QStringLiteral("streams"), normalized},
        {QStringLiteral("stream_count"), normalized.size()},
    });
}

QVariantMap CaptureService::ClearScreencastSession(const QString &sessionPath)
{
    if (!checkPermission(Slm::Permissions::Capability::ScreencastStop, QStringLiteral("ClearScreencastSession"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    const bool existed = m_sessionStreams.remove(session) > 0;
    emit ScreencastSessionCleared(session, false, QStringLiteral("cleared"));
    return success({
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("cleared"), true},
        {QStringLiteral("had_streams"), existed},
    });
}

QVariantMap CaptureService::RevokeScreencastSession(const QString &sessionPath,
                                                    const QString &reason)
{
    if (!checkPermission(Slm::Permissions::Capability::ScreencastStop, QStringLiteral("RevokeScreencastSession"))) {
        return {};
    }
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    const QString normalizedReason = reason.trimmed().isEmpty()
                                         ? QStringLiteral("revoked")
                                         : reason.trimmed();
    const bool existed = m_sessionStreams.remove(session) > 0;
    emit ScreencastSessionCleared(session, true, normalizedReason);
    return success({
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("revoked"), true},
        {QStringLiteral("had_streams"), existed},
        {QStringLiteral("reason"), normalizedReason},
    });
}

void CaptureService::registerDbusService()
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

void CaptureService::connectPortalSignals()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    bus.connect(QString::fromLatin1(kImplPortalService),
                QString::fromLatin1(kImplPortalPath),
                QString::fromLatin1(kImplPortalIface),
                QStringLiteral("ScreencastSessionStateChanged"),
                this,
                SLOT(onPortalSessionStateChanged(QString,QString,bool,int)));
    bus.connect(QString::fromLatin1(kImplPortalService),
                QString::fromLatin1(kImplPortalPath),
                QString::fromLatin1(kImplPortalIface),
                QStringLiteral("ScreencastSessionRevoked"),
                this,
                SLOT(onPortalSessionRevoked(QString,QString,QString,int)));
}

QVariantMap CaptureService::invalidArgument(const QString &reason)
{
    QVariantMap results{
        {QStringLiteral("reason"), reason},
        {QStringLiteral("streams"), QVariantList{}},
    };
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("invalid-argument")},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("response"), 3u},
        {QStringLiteral("results"), results},
    };
}

QVariantMap CaptureService::success(const QVariantMap &results)
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), results},
    };
}

QVariantList CaptureService::normalizeStreamsFromOptions(const QVariantMap &options)
{
    QVariantList out;
    QVariantList streams = options.value(QStringLiteral("streams")).toList();

    if (streams.isEmpty()) {
        QVariantMap stream;
        if (options.contains(QStringLiteral("stream_id"))) {
            stream.insert(QStringLiteral("stream_id"), options.value(QStringLiteral("stream_id")));
        }
        if (options.contains(QStringLiteral("node_id"))) {
            stream.insert(QStringLiteral("node_id"), options.value(QStringLiteral("node_id")));
        } else if (options.contains(QStringLiteral("pipewire_node_id"))) {
            stream.insert(QStringLiteral("node_id"), options.value(QStringLiteral("pipewire_node_id")));
        }
        if (options.contains(QStringLiteral("source_type"))) {
            stream.insert(QStringLiteral("source_type"), options.value(QStringLiteral("source_type")));
        }
        if (options.contains(QStringLiteral("cursor_mode"))) {
            stream.insert(QStringLiteral("cursor_mode"), options.value(QStringLiteral("cursor_mode")));
        }
        if (!stream.isEmpty()) {
            streams.push_back(stream);
        }
    }

    for (const QVariant &value : streams) {
        QVariantMap stream = value.toMap();
        if (stream.isEmpty()) {
            // PipeWire/portal tuple style: [node_id, {dict}]
            const QVariantList tuple = value.toList();
            if (tuple.size() >= 2) {
                const QVariant first = tuple.constFirst();
                const QVariantMap payload = tuple.at(1).toMap();
                stream = payload;
                if (!stream.contains(QStringLiteral("node_id")) && first.isValid()) {
                    stream.insert(QStringLiteral("node_id"), first);
                }
                if (!stream.contains(QStringLiteral("pipewire_node_id")) && first.isValid()) {
                    stream.insert(QStringLiteral("pipewire_node_id"), first);
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

QVariantList CaptureService::queryStreamsFromPortalService(const QString &sessionPath)
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
    QDBusReply<QVariantMap> reply =
        iface.call(QStringLiteral("GetScreencastSessionStreams"), session);
    if (!reply.isValid()) {
        return {};
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok")).toBool()) {
        return {};
    }
    const QVariantList streams = out.value(QStringLiteral("results")).toMap()
                                     .value(QStringLiteral("streams")).toList();
    QVariantMap opts;
    opts.insert(QStringLiteral("streams"), streams);
    return normalizeStreamsFromOptions(opts);
}

QVariantList CaptureService::queryStreamsFromScreencastService(const QString &sessionPath)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return {};
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return {};
    }
    QDBusInterface iface(QString::fromLatin1(kScreencastService),
                         QString::fromLatin1(kScreencastPath),
                         QString::fromLatin1(kScreencastIface),
                         bus);
    if (!iface.isValid()) {
        return {};
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetSessionStreams"), session);
    if (!reply.isValid()) {
        return {};
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok")).toBool()) {
        return {};
    }
    QVariantMap opts;
    opts.insert(QStringLiteral("streams"),
                out.value(QStringLiteral("results")).toMap().value(QStringLiteral("streams")).toList());
    return normalizeStreamsFromOptions(opts);
}

void CaptureService::onPortalSessionStateChanged(const QString &sessionHandle,
                                                 const QString &,
                                                 bool active,
                                                 int)
{
    const QString session = sessionHandle.trimmed();
    if (session.isEmpty()) {
        return;
    }
    if (!active) {
        m_sessionStreams.remove(session);
    }
}

void CaptureService::onPortalSessionRevoked(const QString &sessionHandle,
                                            const QString &,
                                            const QString &,
                                            int)
{
    const QString session = sessionHandle.trimmed();
    if (session.isEmpty()) {
        return;
    }
    m_sessionStreams.remove(session);
}
