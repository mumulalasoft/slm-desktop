#include "desktopdaemonservice.h"

#include "../../../dbuslogutils.h"
#include "../../../appmodel.h"
#include "daemonhealthmonitor.h"
#include "../../core/workspace/spacesmanager.h"
#include "../../core/workspace/windowingbackendmanager.h"
#include "../../core/workspace/workspacemanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QtGlobal>

namespace {
constexpr const char kService[] = "org.slm.WorkspaceManager";
constexpr const char kPath[] = "/org/slm/WorkspaceManager";
constexpr const char kApiVersion[] = "1.0";
}

DesktopDaemonService::DesktopDaemonService(WorkspaceManager *workspaceManager,
                                           WindowingBackendManager *windowingBackend,
                                           SpacesManager *spacesManager,
                                           DesktopAppModel *appModel,
                                           DaemonHealthMonitor *healthMonitor,
                                           QObject *parent)
    : QObject(parent)
    , m_workspaceManager(workspaceManager)
    , m_windowingBackend(windowingBackend)
    , m_spacesManager(spacesManager)
    , m_appModel(appModel)
    , m_healthMonitor(healthMonitor)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    registerDbusService();

    if (m_workspaceManager) {
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceShown,
                this, &DesktopDaemonService::WorkspaceShown);
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceVisibilityChanged,
                this, &DesktopDaemonService::WorkspaceVisibilityChanged);
        // Canonical workspace signals are bridged to legacy overview aliases.
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceShown,
                this, &DesktopDaemonService::OverviewShown);
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceVisibilityChanged,
                this, &DesktopDaemonService::OverviewVisibilityChanged);
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceChanged,
                this, &DesktopDaemonService::WorkspaceChanged);
        connect(m_workspaceManager, &WorkspaceManager::WindowAttention,
                this, &DesktopDaemonService::WindowAttention);
    }
    if (m_windowingBackend) {
        connect(m_windowingBackend, &WindowingBackendManager::backendChanged,
                this, &DesktopDaemonService::backendChanged);
        connect(m_windowingBackend, &WindowingBackendManager::connectedChanged,
                this, &DesktopDaemonService::connectedChanged);
    }
}

DesktopDaemonService::~DesktopDaemonService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool DesktopDaemonService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString DesktopDaemonService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QString DesktopDaemonService::backend() const
{
    return m_windowingBackend ? m_windowingBackend->backend() : QStringLiteral("unknown");
}

bool DesktopDaemonService::connected() const
{
    return m_windowingBackend && m_windowingBackend->connected();
}

QVariantMap DesktopDaemonService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("backend"), backend()},
        {QStringLiteral("connected"), connected()},
    };
}

QVariantMap DesktopDaemonService::GetCapabilities() const
{
    const QStringList canonicalCapabilities{
        QStringLiteral("show_workspace"),
        QStringLiteral("hide_workspace"),
        QStringLiteral("toggle_workspace"),
        QStringLiteral("show_app_grid"),
        QStringLiteral("show_desktop"),
        QStringLiteral("present_window"),
        QStringLiteral("present_view"),
        QStringLiteral("close_view"),
        QStringLiteral("switch_workspace"),
        QStringLiteral("switch_workspace_by_delta"),
        QStringLiteral("move_window_to_workspace"),
        QStringLiteral("move_focused_window_by_delta"),
        QStringLiteral("list_ranked_apps"),
        QStringLiteral("diagnostic_snapshot"),
        QStringLiteral("daemon_health_snapshot"),
    };
    const QStringList legacyCapabilities{
        QStringLiteral("show_overview"),
        QStringLiteral("hide_overview"),
        QStringLiteral("toggle_overview"),
    };
    QStringList allCapabilities = canonicalCapabilities;
    allCapabilities.append(legacyCapabilities);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("capabilities"), allCapabilities},
        {QStringLiteral("canonical_capabilities"), canonicalCapabilities},
        {QStringLiteral("legacy_capability_aliases"), QVariantMap{
             {QStringLiteral("show_overview"), QStringLiteral("show_workspace")},
             {QStringLiteral("hide_overview"), QStringLiteral("hide_workspace")},
             {QStringLiteral("toggle_overview"), QStringLiteral("toggle_workspace")},
        }},
    };
}

void DesktopDaemonService::ShowOverview()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->ShowWorkspace();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("ShowWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("telemetry_action"), QStringLiteral("show_workspace")},
                      {QStringLiteral("legacy_api"), true}});
}

void DesktopDaemonService::HideOverview()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->HideWorkspace();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("HideWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("telemetry_action"), QStringLiteral("hide_workspace")},
                      {QStringLiteral("legacy_api"), true}});
}

void DesktopDaemonService::ToggleOverview()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->ToggleWorkspace();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("ToggleWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("telemetry_action"), QStringLiteral("toggle_workspace")},
                      {QStringLiteral("legacy_api"), true}});
}

void DesktopDaemonService::ShowWorkspace()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->ShowWorkspace();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("ShowWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("telemetry_action"), QStringLiteral("show_workspace")},
                      {QStringLiteral("legacy_api"), false}});
}

void DesktopDaemonService::HideWorkspace()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->HideWorkspace();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("HideWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("telemetry_action"), QStringLiteral("hide_workspace")},
                      {QStringLiteral("legacy_api"), false}});
}

void DesktopDaemonService::ToggleWorkspace()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->ToggleWorkspace();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("ToggleWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("telemetry_action"), QStringLiteral("toggle_workspace")},
                      {QStringLiteral("legacy_api"), false}});
}

void DesktopDaemonService::ShowAppGrid()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->ShowAppGrid();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("ShowAppGrid"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr}});
}

void DesktopDaemonService::ShowDesktop()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->ShowDesktop();
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("ShowDesktop"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr}});
}

bool DesktopDaemonService::PresentWindow(const QString &app_id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_workspaceManager && m_workspaceManager->PresentWindow(app_id);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("PresentWindow"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok},
                      {QStringLiteral("app_id"), app_id}});
    return ok;
}

bool DesktopDaemonService::PresentView(const QString &view_id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_workspaceManager && m_workspaceManager->PresentView(view_id);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("PresentView"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok},
                      {QStringLiteral("view_id"), view_id}});
    return ok;
}

bool DesktopDaemonService::CloseView(const QString &view_id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_workspaceManager && m_workspaceManager->CloseView(view_id);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("CloseView"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok},
                      {QStringLiteral("view_id"), view_id}});
    return ok;
}

void DesktopDaemonService::SwitchWorkspace(int index)
{
    if (!checkPermission(Slm::Permissions::Capability::WorkspaceManage, QStringLiteral("SwitchWorkspace"))) {
        return;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->SwitchWorkspace(index);
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("SwitchWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("index"), index}});
}

void DesktopDaemonService::SwitchWorkspaceByDelta(int delta)
{
    if (!checkPermission(Slm::Permissions::Capability::WorkspaceManage, QStringLiteral("SwitchWorkspaceByDelta"))) {
        return;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_workspaceManager) {
        m_workspaceManager->SwitchWorkspaceByDelta(delta);
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("SwitchWorkspaceByDelta"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), m_workspaceManager != nullptr},
                      {QStringLiteral("delta"), delta}});
}

bool DesktopDaemonService::MoveWindowToWorkspace(const QVariant &window, int index)
{
    if (!checkPermission(Slm::Permissions::Capability::WorkspaceManage, QStringLiteral("MoveWindowToWorkspace"))) {
        return false;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_workspaceManager && m_workspaceManager->MoveWindowToWorkspace(window, index);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("MoveWindowToWorkspace"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok},
                      {QStringLiteral("index"), index}});
    return ok;
}

bool DesktopDaemonService::MoveFocusedWindowByDelta(int delta)
{
    if (!checkPermission(Slm::Permissions::Capability::WorkspaceManage, QStringLiteral("MoveFocusedWindowByDelta"))) {
        return false;
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_workspaceManager && m_workspaceManager->MoveFocusedWindowByDelta(delta);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("MoveFocusedWindowByDelta"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok},
                      {QStringLiteral("delta"), delta}});
    return ok;
}

QVariantList DesktopDaemonService::ListRankedApps(int limit) const
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const int bounded = qBound(1, limit, 256);
    QVariantList out;
    if (m_appModel) {
        out = m_appModel->frequentApps(bounded);
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ListRankedApps"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), m_appModel != nullptr},
                          {QStringLiteral("limit"), bounded},
                          {QStringLiteral("count"), out.size()}});
    return out;
}

QVariantMap DesktopDaemonService::DiagnosticSnapshot() const
{
    if (!const_cast<DesktopDaemonService*>(this)->checkPermission(Slm::Permissions::Capability::DiagnosticsRead, QStringLiteral("DiagnosticSnapshot"))) {
        return {};
    }
    QVariantMap out;
    out.insert(QStringLiteral("service"), QString::fromLatin1(kService));
    out.insert(QStringLiteral("path"), QString::fromLatin1(kPath));
    out.insert(QStringLiteral("serviceRegistered"), m_serviceRegistered);
    out.insert(QStringLiteral("backend"), backend());
    out.insert(QStringLiteral("connected"), connected());
    if (m_spacesManager) {
        out.insert(QStringLiteral("activeSpace"), m_spacesManager->activeSpace());
        out.insert(QStringLiteral("spaceCount"), m_spacesManager->spaceCount());
    } else {
        out.insert(QStringLiteral("activeSpace"), 1);
        out.insert(QStringLiteral("spaceCount"), 1);
    }
    out.insert(QStringLiteral("daemonHealth"), DaemonHealthSnapshot());
    return out;
}

QVariantMap DesktopDaemonService::DaemonHealthSnapshot() const
{
    if (!m_healthMonitor) {
        return {};
    }
    return m_healthMonitor->snapshot();
}

void DesktopDaemonService::registerDbusService()
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

void DesktopDaemonService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for DesktopDaemonService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.WorkspaceManager"), QStringLiteral("SwitchWorkspace"), Slm::Permissions::Capability::WorkspaceManage);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.WorkspaceManager"), QStringLiteral("SwitchWorkspaceByDelta"), Slm::Permissions::Capability::WorkspaceManage);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.WorkspaceManager"), QStringLiteral("MoveWindowToWorkspace"), Slm::Permissions::Capability::WorkspaceManage);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.WorkspaceManager"), QStringLiteral("MoveFocusedWindowByDelta"), Slm::Permissions::Capability::WorkspaceManage);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.WorkspaceManager"), QStringLiteral("DiagnosticSnapshot"), Slm::Permissions::Capability::DiagnosticsRead);
}

bool DesktopDaemonService::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
{
    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("Medium"));

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
