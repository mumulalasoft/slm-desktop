#include "slmcapabilityservice.h"

#include "../../../dbuslogutils.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>

namespace {
constexpr const char kService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kPath[] = "/org/freedesktop/SLMCapabilities";
}

SlmCapabilityService::SlmCapabilityService(Slm::Actions::Framework::SlmActionFramework *framework, QObject *parent)
    : QObject(parent)
    , m_framework(framework)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    registerDbus();
    if (m_framework) {
        connect(m_framework, &Slm::Actions::Framework::SlmActionFramework::scannerRescanned,
                this, [this](const QStringList &) {
                    emit IndexingFinished();
                });
        emit ProviderRegistered(QStringLiteral("desktop-entry"));
        emit ProviderRegistered(QStringLiteral("slm-context-actions"));
        emit ProviderRegistered(QStringLiteral("slm-capability-framework"));
    }
}

SlmCapabilityService::~SlmCapabilityService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool SlmCapabilityService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QVariantMap SlmCapabilityService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("ready"), m_framework != nullptr},
    };
}

QVariantList SlmCapabilityService::ListActions(const QString &capability,
                                               const QStringList &uris,
                                               const QVariantMap &options)
{
    QVariantMap context = options;
    context.insert(QStringLiteral("uris"), uris);
    if (!context.contains(QStringLiteral("selection_count"))) {
        context.insert(QStringLiteral("selection_count"), uris.size());
    }
    return ListActionsWithContext(capability, context);
}

QVariantList SlmCapabilityService::ListActionsWithContext(const QString &capability,
                                                          const QVariantMap &context)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const Slm::Permissions::Capability permCapability = permissionCapabilityForList(capability);
    if (!allowedByPolicy(permCapability, context, QStringLiteral("ListActionsWithContext"), requestId)) {
        return {};
    }
    QVariantList out;
    if (m_framework) {
        out = m_framework->listActions(capability, context);
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ListActions"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), m_framework != nullptr},
                          {QStringLiteral("capability"), capability},
                          {QStringLiteral("count"), out.size()}});
    return out;
}

QVariantMap SlmCapabilityService::InvokeAction(const QString &actionId,
                                               const QStringList &uris,
                                               const QVariantMap &options)
{
    QVariantMap context = options;
    context.insert(QStringLiteral("uris"), uris);
    if (!context.contains(QStringLiteral("selection_count"))) {
        context.insert(QStringLiteral("selection_count"), uris.size());
    }
    return InvokeActionWithContext(actionId, context);
}

QVariantMap SlmCapabilityService::InvokeActionWithContext(const QString &actionId,
                                                          const QVariantMap &context)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const Slm::Permissions::Capability permCapability = permissionCapabilityForInvoke(context);
    if (!allowedByPolicy(permCapability, context, QStringLiteral("InvokeActionWithContext"), requestId)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
            {QStringLiteral("reason"), QStringLiteral("policy-deny")},
        };
    }
    QVariantMap out{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("framework-unavailable")},
    };
    if (m_framework) {
        out = m_framework->invokeAction(actionId, context);
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("InvokeAction"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), out.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("action_id"), actionId}});
    return out;
}

QVariantMap SlmCapabilityService::ResolveDefaultAction(const QString &capability,
                                                       const QVariantMap &context)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    if (!allowedByPolicy(permissionCapabilityForList(capability),
                         context,
                         QStringLiteral("ResolveDefaultAction"),
                         requestId)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
            {QStringLiteral("reason"), QStringLiteral("policy-deny")},
        };
    }
    if (!m_framework) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("framework-unavailable")},
        };
    }
    return m_framework->resolveDefaultAction(capability, context);
}

QVariantList SlmCapabilityService::SearchActions(const QString &query,
                                                 const QVariantMap &context)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    if (!allowedByPolicy(Slm::Permissions::Capability::SearchQueryFiles,
                         context,
                         QStringLiteral("SearchActions"),
                         requestId)) {
        return {};
    }
    if (!m_framework) {
        return {};
    }
    return m_framework->searchActions(query, context);
}

QVariantList SlmCapabilityService::ListPermissionGrants()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    if (!adminAllowed(QStringLiteral("ListPermissionGrants"), requestId)) {
        return {};
    }
    return m_permissionStore.listPermissions();
}

QVariantList SlmCapabilityService::ListPermissionAudit(int limit)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    if (!adminAllowed(QStringLiteral("ListPermissionAudit"), requestId)) {
        return {};
    }
    return m_permissionStore.listAuditLogs(limit);
}

QVariantMap SlmCapabilityService::ResetPermissionsForApp(const QString &appId)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    if (!adminAllowed(QStringLiteral("ResetPermissionsForApp"), requestId)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
            {QStringLiteral("reason"), QStringLiteral("admin-required")},
        };
    }
    const bool ok = m_permissionStore.resetPermissionsForApp(appId);
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("appId"), appId},
    };
}

QVariantMap SlmCapabilityService::ResetAllPermissions()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    if (!adminAllowed(QStringLiteral("ResetAllPermissions"), requestId)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
            {QStringLiteral("reason"), QStringLiteral("admin-required")},
        };
    }
    const bool ok = m_permissionStore.resetAllPermissions();
    return {
        {QStringLiteral("ok"), ok},
    };
}

void SlmCapabilityService::Reload()
{
    if (m_framework) {
        emit IndexingStarted();
        m_framework->rescanNow();
        emit IndexingFinished();
    }
}

void SlmCapabilityService::registerDbus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        return;
    }

    const QString service = QString::fromLatin1(kService);
    const QString path = QString::fromLatin1(kPath);
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        return;
    }

    if (iface->isServiceRegistered(service)) {
        iface->unregisterService(service);
    }
    if (!bus.registerService(service)) {
        m_serviceRegistered = false;
        return;
    }
    if (!bus.registerObject(path, this,
                            QDBusConnection::ExportAllSlots
                                | QDBusConnection::ExportAllSignals
                                | QDBusConnection::ExportAllProperties)) {
        bus.unregisterService(service);
        m_serviceRegistered = false;
        return;
    }
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

void SlmCapabilityService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[slm-perm] failed to open permission store";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);
}

Slm::Permissions::Capability SlmCapabilityService::permissionCapabilityForList(const QString &capability) const
{
    const QString c = capability.trimmed().toLower();
    if (c == QStringLiteral("share")) {
        return Slm::Permissions::Capability::ShareInvoke;
    }
    if (c == QStringLiteral("quickaction")) {
        return Slm::Permissions::Capability::QuickActionInvoke;
    }
    if (c == QStringLiteral("searchaction")) {
        return Slm::Permissions::Capability::SearchQueryFiles;
    }
    // ContextMenu/OpenWith/DragDrop and fallback path.
    return Slm::Permissions::Capability::FileActionInvoke;
}

Slm::Permissions::Capability SlmCapabilityService::permissionCapabilityForInvoke(const QVariantMap &context) const
{
    const QString cap = context.value(QStringLiteral("capability")).toString().trimmed().toLower();
    if (cap == QStringLiteral("share")) {
        return Slm::Permissions::Capability::ShareInvoke;
    }
    if (cap == QStringLiteral("quickaction")) {
        return Slm::Permissions::Capability::QuickActionInvoke;
    }
    if (cap == QStringLiteral("openwith")) {
        return Slm::Permissions::Capability::OpenWithInvoke;
    }
    return Slm::Permissions::Capability::FileActionInvoke;
}

bool SlmCapabilityService::allowedByPolicy(const Slm::Permissions::Capability capability,
                                           const QVariantMap &context,
                                           const QString &methodName,
                                           const QString &requestId) const
{
    QVariantMap accessContext = context;
    accessContext.insert(QStringLiteral("capability"),
                         Slm::Permissions::capabilityToString(capability));
    if (!accessContext.contains(QStringLiteral("resourceType"))) {
        accessContext.insert(QStringLiteral("resourceType"), QStringLiteral("action"));
    }
    if (!accessContext.contains(QStringLiteral("sensitivityLevel"))) {
        accessContext.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("Medium"));
    }

    Slm::Permissions::PolicyDecision decision;
    if (calledFromDBus()) {
        decision = m_securityGuard.check(message(), capability, accessContext);
    } else {
        // In-process call path (not DBus): keep current behavior.
        decision.type = Slm::Permissions::DecisionType::Allow;
        decision.capability = capability;
        decision.reason = QStringLiteral("local-inprocess");
    }

    if (decision.isAllowed()) {
        return true;
    }

    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         methodName,
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("deny"),
                         {{QStringLiteral("ok"), false},
                          {QStringLiteral("error"), QStringLiteral("permission-denied")},
                          {QStringLiteral("capability"),
                           Slm::Permissions::capabilityToString(capability)},
                          {QStringLiteral("decision"),
                           Slm::Permissions::decisionTypeToString(decision.type)},
                          {QStringLiteral("reason"), decision.reason}});
    return false;
}

bool SlmCapabilityService::adminAllowed(const QString &methodName,
                                        const QString &requestId) const
{
    if (!calledFromDBus()) {
        return true;
    }
    Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message());
    caller = m_trustResolver.resolveTrust(caller);
    if (caller.trustLevel == Slm::Permissions::TrustLevel::CoreDesktopComponent) {
        return true;
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         methodName,
                         requestId,
                         message().service(),
                         QString(),
                         QStringLiteral("deny"),
                         {{QStringLiteral("ok"), false},
                          {QStringLiteral("error"), QStringLiteral("permission-denied")},
                          {QStringLiteral("reason"), QStringLiteral("admin-required")}});
    return false;
}
