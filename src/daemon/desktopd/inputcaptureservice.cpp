#include "inputcaptureservice.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDateTime>

namespace {
constexpr const char kService[] = "org.slm.Desktop.InputCapture";
constexpr const char kPath[] = "/org/slm/Desktop/InputCapture";
constexpr const char kApiVersion[] = "1.0";
}

InputCaptureService::InputCaptureService(QObject *parent)
    : QObject(parent)
    , m_backend(InputCaptureBackend::createDefault())
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    registerDbusService();
}

InputCaptureService::~InputCaptureService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool InputCaptureService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString InputCaptureService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap InputCaptureService::Ping() const
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

QVariantMap InputCaptureService::GetState() const
{
    QVariantList sessions;
    int enabledCount = 0;
    for (const SessionRecord &record : m_sessions) {
        if (record.enabled) {
            ++enabledCount;
        }
        sessions.push_back(sessionToMap(record));
    }
    return success({
        {QStringLiteral("session_count"), m_sessions.size()},
        {QStringLiteral("enabled_count"), enabledCount},
        {QStringLiteral("sessions"), sessions},
        {QStringLiteral("backend"), m_backend ? m_backend->capabilityReport() : QVariantMap{}},
    });
}

QVariantMap InputCaptureService::GetSessionState(const QString &sessionPath) const
{
    const QString session = normalizedSession(sessionPath);
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    auto it = m_sessions.constFind(session);
    if (it == m_sessions.constEnd()) {
        return success({
            {QStringLiteral("session_handle"), session},
            {QStringLiteral("exists"), false},
            {QStringLiteral("enabled"), false},
            {QStringLiteral("barrier_count"), 0},
            {QStringLiteral("barriers"), QVariantList{}},
        });
    }
    QVariantMap out = sessionToMap(*it);
    out.insert(QStringLiteral("exists"), true);
    if (m_backend) {
        out.insert(QStringLiteral("backend"), m_backend->capabilityReport());
    }
    return success(out);
}

QVariantMap InputCaptureService::CreateSession(const QString &sessionPath,
                                               const QString &appId,
                                               const QVariantMap &options)
{
    Q_UNUSED(options)
    if (!checkPermission(Slm::Permissions::Capability::InputCaptureCreateSession,
                         QStringLiteral("CreateSession"))) {
        return {};
    }
    const QString session = normalizedSession(sessionPath);
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }

    SessionRecord record = m_sessions.value(session);
    record.sessionHandle = session;
    if (!appId.trimmed().isEmpty()) {
        record.appId = appId.trimmed();
    }
    if (calledFromDBus()) {
        record.ownerService = message().service().trimmed();
    }
    record.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_sessions.insert(session, record);
    emit SessionStateChanged(session, record.enabled, record.barriers.size());

    QVariantMap results{
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("created"), true},
        {QStringLiteral("app_id"), record.appId},
        {QStringLiteral("enabled"), record.enabled},
        {QStringLiteral("barrier_count"), record.barriers.size()},
    };
    const QVariantMap backend = m_backend
        ? m_backend->createSession(session, record.appId, options)
        : QVariantMap{};
    return success(withBackendResult(results, backend));
}

QVariantMap InputCaptureService::SetPointerBarriers(const QString &sessionPath,
                                                    const QVariantList &barriers,
                                                    const QVariantMap &options)
{
    Q_UNUSED(options)
    if (!checkPermission(Slm::Permissions::Capability::InputCaptureSetPointerBarriers,
                         QStringLiteral("SetPointerBarriers"))) {
        return {};
    }
    const QString session = normalizedSession(sessionPath);
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    QString barrierReason;
    if (!validateBarriers(barriers, &barrierReason)) {
        return invalidArgument(barrierReason);
    }
    SessionRecord record = m_sessions.value(session);
    record.sessionHandle = session;
    if (!isOwnerAllowed(record)) {
        return denied(QStringLiteral("session-owner-mismatch"));
    }
    record.barriers = barriers;
    record.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_sessions.insert(session, record);
    emit SessionStateChanged(session, record.enabled, record.barriers.size());
    QVariantMap results{
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("barriers_set"), true},
        {QStringLiteral("barriers"), record.barriers},
        {QStringLiteral("barrier_count"), record.barriers.size()},
    };
    const QVariantMap backend = m_backend
        ? m_backend->setPointerBarriers(session, record.barriers, options)
        : QVariantMap{};
    return success(withBackendResult(results, backend));
}

QVariantMap InputCaptureService::EnableSession(const QString &sessionPath, const QVariantMap &options)
{
    if (!checkPermission(Slm::Permissions::Capability::InputCaptureEnable,
                         QStringLiteral("EnableSession"))) {
        return {};
    }
    const QString session = normalizedSession(sessionPath);
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    SessionRecord record = m_sessions.value(session);
    record.sessionHandle = session;
    if (!isOwnerAllowed(record)) {
        return denied(QStringLiteral("session-owner-mismatch"));
    }
    if (record.enabled) {
        return success({
            {QStringLiteral("session_handle"), session},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("already_enabled"), true},
            {QStringLiteral("barrier_count"), record.barriers.size()},
        });
    }
    const bool allowPreempt = options.value(QStringLiteral("allow_preempt")).toBool();
    const QString preemptReason =
        options.value(QStringLiteral("preempt_reason")).toString().trimmed().isEmpty()
            ? QStringLiteral("inputcapture-preempted")
            : options.value(QStringLiteral("preempt_reason")).toString().trimmed();
    QString conflictingSession;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        if (it.key() == session) {
            continue;
        }
        if (it->enabled) {
            conflictingSession = it.key();
            break;
        }
    }
    QString preemptedSession;
    if (!conflictingSession.isEmpty()) {
        if (!allowPreempt) {
            const SessionRecord conflictRecord = m_sessions.value(conflictingSession);
            QVariantMap results{
                {QStringLiteral("session_handle"), session},
                {QStringLiteral("reason"), QStringLiteral("conflict-active-session")},
                {QStringLiteral("conflict_session"), conflictingSession},
                {QStringLiteral("requires_mediation"), true},
                {QStringLiteral("mediation_action"), QStringLiteral("inputcapture-preempt-consent")},
                {QStringLiteral("mediation_scope"), QStringLiteral("session-conflict")},
            };
            results.insert(QStringLiteral("conflict_owner"), conflictRecord.appId);
            return denied(QStringLiteral("conflict-active-session"), results);
        }
        SessionRecord prev = m_sessions.value(conflictingSession);
        prev.enabled = false;
        prev.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_sessions.insert(conflictingSession, prev);
        emit SessionStateChanged(conflictingSession, false, prev.barriers.size());
        emit SessionReleased(conflictingSession, preemptReason);
        preemptedSession = conflictingSession;
    }
    record.enabled = true;
    record.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_sessions.insert(session, record);
    emit SessionStateChanged(session, true, record.barriers.size());
    QVariantMap results{
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("barrier_count"), record.barriers.size()},
    };
    if (!preemptedSession.isEmpty()) {
        results.insert(QStringLiteral("preempted_session"), preemptedSession);
        results.insert(QStringLiteral("preempted"), true);
    }
    const QVariantMap backend = m_backend
        ? m_backend->enableSession(session, options)
        : QVariantMap{};
    return success(withBackendResult(results, backend));
}

QVariantMap InputCaptureService::DisableSession(const QString &sessionPath, const QVariantMap &options)
{
    Q_UNUSED(options)
    if (!checkPermission(Slm::Permissions::Capability::InputCaptureDisable,
                         QStringLiteral("DisableSession"))) {
        return {};
    }
    const QString session = normalizedSession(sessionPath);
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    SessionRecord record = m_sessions.value(session);
    record.sessionHandle = session;
    if (!isOwnerAllowed(record)) {
        return denied(QStringLiteral("session-owner-mismatch"));
    }
    record.enabled = false;
    record.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_sessions.insert(session, record);
    emit SessionStateChanged(session, false, record.barriers.size());
    QVariantMap results{
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("disabled"), true},
        {QStringLiteral("barrier_count"), record.barriers.size()},
    };
    const QVariantMap backend = m_backend
        ? m_backend->disableSession(session, options)
        : QVariantMap{};
    return success(withBackendResult(results, backend));
}

QVariantMap InputCaptureService::ReleaseSession(const QString &sessionPath, const QString &reason)
{
    if (!checkPermission(Slm::Permissions::Capability::InputCaptureRelease,
                         QStringLiteral("ReleaseSession"))) {
        return {};
    }
    const QString session = normalizedSession(sessionPath);
    if (session.isEmpty()) {
        return invalidArgument(QStringLiteral("missing-session-path"));
    }
    const auto it = m_sessions.constFind(session);
    if (it != m_sessions.constEnd() && !isOwnerAllowed(*it)) {
        return denied(QStringLiteral("session-owner-mismatch"));
    }
    const bool existed = m_sessions.remove(session) > 0;
    const QString normalizedReason = reason.trimmed().isEmpty()
        ? QStringLiteral("released")
        : reason.trimmed();
    emit SessionReleased(session, normalizedReason);
    QVariantMap results{
        {QStringLiteral("session_handle"), session},
        {QStringLiteral("released"), true},
        {QStringLiteral("had_session"), existed},
        {QStringLiteral("reason"), normalizedReason},
    };
    const QVariantMap backend = m_backend
        ? m_backend->releaseSession(session, normalizedReason)
        : QVariantMap{};
    return success(withBackendResult(results, backend));
}

void InputCaptureService::registerDbusService()
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

void InputCaptureService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for InputCaptureService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    const QString iface = QStringLiteral("org.slm.Desktop.InputCapture");
    m_securityGuard.registerMethodCapability(iface,
                                             QStringLiteral("CreateSession"),
                                             Slm::Permissions::Capability::InputCaptureCreateSession);
    m_securityGuard.registerMethodCapability(iface,
                                             QStringLiteral("SetPointerBarriers"),
                                             Slm::Permissions::Capability::InputCaptureSetPointerBarriers);
    m_securityGuard.registerMethodCapability(iface,
                                             QStringLiteral("EnableSession"),
                                             Slm::Permissions::Capability::InputCaptureEnable);
    m_securityGuard.registerMethodCapability(iface,
                                             QStringLiteral("DisableSession"),
                                             Slm::Permissions::Capability::InputCaptureDisable);
    m_securityGuard.registerMethodCapability(iface,
                                             QStringLiteral("ReleaseSession"),
                                             Slm::Permissions::Capability::InputCaptureRelease);
}

bool InputCaptureService::checkPermission(Slm::Permissions::Capability capability,
                                          const QString &methodName) const
{
    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("High"));

    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (!d.isAllowed()) {
        const_cast<InputCaptureService *>(this)->sendErrorReply(QStringLiteral("org.slm.PermissionDenied"),
                                                                d.reason);
        return false;
    }
    return true;
}

QVariantMap InputCaptureService::invalidArgument(const QString &reason)
{
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("invalid-argument")},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("response"), 3u},
        {QStringLiteral("results"), QVariantMap{{QStringLiteral("reason"), reason}}},
    };
}

QVariantMap InputCaptureService::denied(const QString &reason)
{
    return denied(reason, QVariantMap{});
}

QVariantMap InputCaptureService::denied(const QString &reason, const QVariantMap &results)
{
    QVariantMap outResults = results;
    outResults.insert(QStringLiteral("reason"), reason);
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("permission-denied")},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("response"), 2u},
        {QStringLiteral("results"), outResults},
    };
}

QVariantMap InputCaptureService::success(const QVariantMap &results)
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), results},
    };
}

QString InputCaptureService::normalizedSession(const QString &sessionPath)
{
    return sessionPath.trimmed();
}

QVariantMap InputCaptureService::sessionToMap(const SessionRecord &record) const
{
    return {
        {QStringLiteral("session_handle"), record.sessionHandle},
        {QStringLiteral("app_id"), record.appId},
        {QStringLiteral("enabled"), record.enabled},
        {QStringLiteral("barriers"), record.barriers},
        {QStringLiteral("barrier_count"), record.barriers.size()},
        {QStringLiteral("updated_at_ms"), record.updatedAtMs},
        {QStringLiteral("owner_service"), record.ownerService},
    };
}

QVariantMap InputCaptureService::withBackendResult(QVariantMap results,
                                                   const QVariantMap &backend) const
{
    if (!backend.isEmpty()) {
        results.insert(QStringLiteral("backend"), backend);
    }
    return results;
}

bool InputCaptureService::validateBarriers(const QVariantList &barriers, QString *reasonOut)
{
    for (int i = 0; i < barriers.size(); ++i) {
        const QVariantMap m = barriers.at(i).toMap();
        if (m.isEmpty()) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("invalid-barrier-map-%1").arg(i);
            }
            return false;
        }

        const QString id = m.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty()) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("missing-barrier-id-%1").arg(i);
            }
            return false;
        }

        bool okX1 = false;
        bool okY1 = false;
        bool okX2 = false;
        bool okY2 = false;
        const int x1 = m.value(QStringLiteral("x1")).toInt(&okX1);
        const int y1 = m.value(QStringLiteral("y1")).toInt(&okY1);
        const int x2 = m.value(QStringLiteral("x2")).toInt(&okX2);
        const int y2 = m.value(QStringLiteral("y2")).toInt(&okY2);
        if (!(okX1 && okY1 && okX2 && okY2)) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("invalid-barrier-geometry-%1").arg(i);
            }
            return false;
        }

        const bool vertical = (x1 == x2) && (y1 != y2);
        const bool horizontal = (y1 == y2) && (x1 != x2);
        if (!(vertical || horizontal)) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("unsupported-barrier-shape-%1").arg(i);
            }
            return false;
        }
    }

    return true;
}

bool InputCaptureService::isOwnerAllowed(const SessionRecord &record) const
{
    if (!calledFromDBus()) {
        return true;
    }
    const QString caller = message().service().trimmed();
    if (caller.isEmpty() || record.ownerService.isEmpty()) {
        return true;
    }
    return caller == record.ownerService;
}
