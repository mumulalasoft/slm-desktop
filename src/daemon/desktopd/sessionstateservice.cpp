#include "sessionstateservice.h"

#include "../../../dbuslogutils.h"
#include "sessionauthenticator.h"
#include "sessionstatemanager.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QStandardPaths>

namespace {
constexpr const char kService[] = "org.slm.SessionState";
constexpr const char kPath[] = "/org/slm/SessionState";
constexpr const char kApiVersion[] = "1.0";
}

SessionStateService::SessionStateService(SessionStateManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    loadUnlockThrottleState();
    registerDbusService();

    if (m_manager) {
        connect(m_manager, &SessionStateManager::SessionLocked,
                this, &SessionStateService::SessionLocked);
        connect(m_manager, &SessionStateManager::SessionUnlocked,
                this, &SessionStateService::SessionUnlocked);
        connect(m_manager, &SessionStateManager::IdleChanged,
                this, &SessionStateService::IdleChanged);
        connect(m_manager, &SessionStateManager::ActiveAppChanged,
                this, &SessionStateService::ActiveAppChanged);
    }
}

SessionStateService::~SessionStateService()
{
    saveUnlockThrottleState();
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool SessionStateService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString SessionStateService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap SessionStateService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("sessionRegistered"), m_serviceRegistered},
        {QStringLiteral("api_version"), apiVersion()},
    };
}

QVariantMap SessionStateService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("inhibit"),
             QStringLiteral("logout"),
             QStringLiteral("shutdown"),
             QStringLiteral("reboot"),
             QStringLiteral("lock"),
             QStringLiteral("unlock"),
        }},
    };
}

uint SessionStateService::Inhibit(const QString &reason, uint flags)
{
    if (!checkPermission(Slm::Permissions::Capability::SessionInhibit, QStringLiteral("Inhibit"))) {
        return 0u;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const uint token = m_manager ? m_manager->Inhibit(reason, flags) : 0u;
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Inhibit"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), token != 0u},
                      {QStringLiteral("token"), token},
                      {QStringLiteral("flags"), flags},
                      {QStringLiteral("reason"), reason}});
    return token;
}

void SessionStateService::Logout()
{
    if (!checkPermission(Slm::Permissions::Capability::SessionLogout, QStringLiteral("Logout"))) {
        return;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_manager) {
        m_manager->Logout();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Logout"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_manager != nullptr}});
}

void SessionStateService::Shutdown()
{
    if (!checkPermission(Slm::Permissions::Capability::SessionShutdown, QStringLiteral("Shutdown"))) {
        return;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_manager) {
        m_manager->Shutdown();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Shutdown"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_manager != nullptr}});
}

void SessionStateService::Reboot()
{
    if (!checkPermission(Slm::Permissions::Capability::SessionReboot, QStringLiteral("Reboot"))) {
        return;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_manager) {
        m_manager->Reboot();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Reboot"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_manager != nullptr}});
}

void SessionStateService::Lock()
{
    if (!checkPermission(Slm::Permissions::Capability::SessionLock, QStringLiteral("Lock"))) {
        return;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_manager) {
        m_manager->Lock();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Lock"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_manager != nullptr}});
}

QVariantMap SessionStateService::Unlock(const QString &password)
{
    if (!checkPermission(Slm::Permissions::Capability::SessionUnlock, QStringLiteral("Unlock"))) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
        };
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QString key = throttleKey();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Slm::Desktopd::SessionUnlockThrottleState currentState = m_unlockThrottle.value(key);
    if (applyThrottleDecay(currentState, nowMs)) {
        m_unlockThrottle.insert(key, currentState);
        saveUnlockThrottleState();
    }

    if (currentState.lockoutUntilMs > nowMs) {
        const QVariantMap limited = rateLimitedResult(key, nowMs);
        if (calledFromDBus()) {
            Slm::Permissions::CallerIdentity callerId = Slm::Permissions::resolveCallerIdentityFromDbus(message());
            callerId = m_trustResolver.resolveTrust(callerId);
            Slm::Permissions::AccessContext auditContext;
            auditContext.capability = Slm::Permissions::Capability::SessionUnlock;
            auditContext.resourceType = QStringLiteral("session");
            auditContext.timestamp = nowMs;
            Slm::Permissions::PolicyDecision auditDecision;
            auditDecision.capability = Slm::Permissions::Capability::SessionUnlock;
            auditDecision.type = Slm::Permissions::DecisionType::Deny;
            auditDecision.reason = QStringLiteral("unlock-rate-limited");
            m_auditLogger.recordDecision(callerId, auditContext, auditDecision);
        }
        SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("Unlock"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), false},
                          {QStringLiteral("error"), QStringLiteral("rate-limited")},
                          {QStringLiteral("retry_after_sec"), limited.value(QStringLiteral("retry_after_sec"))}});
        return limited;
    }

    QString reason;
    const bool authOk = SessionAuthenticator::authenticateCurrentUser(password, &reason);
    if (authOk && m_manager) {
        m_manager->Unlock();
        recordUnlockSuccess(key);
    } else {
        recordUnlockFailure(key, nowMs);
    }

    const Slm::Desktopd::SessionUnlockThrottleState updatedState = m_unlockThrottle.value(key);
    const QVariantMap throttleInfo = rateLimitedResult(key, nowMs);
    const bool nowRateLimited = updatedState.lockoutUntilMs > nowMs;

    if (calledFromDBus()) {
        Slm::Permissions::CallerIdentity callerId = Slm::Permissions::resolveCallerIdentityFromDbus(message());
        callerId = m_trustResolver.resolveTrust(callerId);
        Slm::Permissions::AccessContext auditContext;
        auditContext.capability = Slm::Permissions::Capability::SessionUnlock;
        auditContext.resourceType = QStringLiteral("session");
        auditContext.timestamp = QDateTime::currentMSecsSinceEpoch();
        Slm::Permissions::PolicyDecision auditDecision;
        auditDecision.capability = Slm::Permissions::Capability::SessionUnlock;
        auditDecision.type = authOk ? Slm::Permissions::DecisionType::Allow
                                    : Slm::Permissions::DecisionType::Deny;
        auditDecision.reason = authOk ? QStringLiteral("unlock-auth-success")
                                      : (nowRateLimited
                                             ? QStringLiteral("unlock-auth-failed-rate-limited:%1").arg(reason)
                                             : QStringLiteral("unlock-auth-failed:%1").arg(reason));
        m_auditLogger.recordDecision(callerId, auditContext, auditDecision);
    }

    QVariantMap result{
        {QStringLiteral("ok"), authOk && (m_manager != nullptr)},
        {QStringLiteral("error"), authOk ? QString() : reason},
    };
    if (!authOk) {
        result.insert(QStringLiteral("retry_after_sec"),
                      nowRateLimited ? throttleInfo.value(QStringLiteral("retry_after_sec")) : 0);
    }

    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Unlock"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                      {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()},
                      {QStringLiteral("retry_after_sec"),
                       result.value(QStringLiteral("retry_after_sec"), 0)}});
    return result;
}

void SessionStateService::registerDbusService()
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
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllSignals |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

void SessionStateService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for SessionStateService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.SessionState"), QStringLiteral("Logout"), Slm::Permissions::Capability::SessionLogout);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.SessionState"), QStringLiteral("Shutdown"), Slm::Permissions::Capability::SessionShutdown);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.SessionState"), QStringLiteral("Reboot"), Slm::Permissions::Capability::SessionReboot);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.SessionState"), QStringLiteral("Lock"), Slm::Permissions::Capability::SessionLock);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.SessionState"), QStringLiteral("Unlock"), Slm::Permissions::Capability::SessionUnlock);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.SessionState"), QStringLiteral("Inhibit"), Slm::Permissions::Capability::SessionInhibit);
}

bool SessionStateService::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
{
    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("High"));

    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (d.type == Slm::Permissions::DecisionType::Allow) {
        return true;
    }

    sendErrorReply(QStringLiteral("org.slm.Desktop.Error.PermissionDenied"),
                   QStringLiteral("Permission denied for method %1: %2")
                       .arg(methodName)
                       .arg(d.reason));
    return false;
}

QString SessionStateService::throttleKey() const
{
    if (!calledFromDBus()) {
        return QStringLiteral("local");
    }
    const QString bus = message().service().trimmed();
    if (bus.isEmpty()) {
        return QStringLiteral("unknown");
    }
    return bus;
}

int SessionStateService::lockoutSecondsForLevel(int level) const
{
    return Slm::Desktopd::SessionUnlockThrottle::lockoutSecondsForLevel(level);
}

int SessionStateService::decayWindowSeconds() const
{
    return Slm::Desktopd::SessionUnlockThrottle::decayWindowSeconds();
}

bool SessionStateService::applyThrottleDecay(Slm::Desktopd::SessionUnlockThrottleState &state, qint64 nowMs) const
{
    return Slm::Desktopd::SessionUnlockThrottle::applyDecay(state, nowMs);
}

QVariantMap SessionStateService::rateLimitedResult(const QString &key, qint64 nowMs)
{
    const Slm::Desktopd::SessionUnlockThrottleState state = m_unlockThrottle.value(key);
    const qint64 remainingMs = qMax<qint64>(0, state.lockoutUntilMs - nowMs);
    const int retryAfterSec = int((remainingMs + 999) / 1000);
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("rate-limited")},
        {QStringLiteral("retry_after_sec"), retryAfterSec},
    };
}

void SessionStateService::recordUnlockFailure(const QString &key, qint64 nowMs)
{
    Slm::Desktopd::SessionUnlockThrottleState state = m_unlockThrottle.value(key);
    Slm::Desktopd::SessionUnlockThrottle::recordFailure(state, nowMs);
    m_unlockThrottle.insert(key, state);
    saveUnlockThrottleState();
}

void SessionStateService::recordUnlockSuccess(const QString &key)
{
    Slm::Desktopd::SessionUnlockThrottleState state = m_unlockThrottle.value(key);
    Slm::Desktopd::SessionUnlockThrottle::recordSuccess(state);
    m_unlockThrottle.insert(key, state);
    saveUnlockThrottleState();
}

QString SessionStateService::throttleStatePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.trimmed().isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/slm");
    }
    return QDir(base).filePath(QStringLiteral("session_unlock_throttle.json"));
}

void SessionStateService::loadUnlockThrottleState()
{
    const QString path = throttleStatePath();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    if (!Slm::Desktopd::SessionUnlockThrottle::load(path, m_unlockThrottle, nowMs, &changed)) {
        qWarning() << "[desktopd] failed to load unlock throttle state:" << path;
        return;
    }
    if (changed) {
        saveUnlockThrottleState();
    }
}

void SessionStateService::saveUnlockThrottleState() const
{
    const QString path = throttleStatePath();
    if (!Slm::Desktopd::SessionUnlockThrottle::save(path, m_unlockThrottle)) {
        qWarning() << "[desktopd] failed to save unlock throttle state:" << path;
    }
}
