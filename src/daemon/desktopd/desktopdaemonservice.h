#pragma once

#include <QObject>
#include <QDBusContext>
#include <QVariant>
#include <QVariantMap>
#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

class WorkspaceManager;
class WindowingBackendManager;
class SpacesManager;
class DesktopAppModel;
class DaemonHealthMonitor;

class DesktopDaemonService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.WorkspaceManager1")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)
    Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit DesktopDaemonService(WorkspaceManager *workspaceManager,
                                  WindowingBackendManager *windowingBackend,
                                  SpacesManager *spacesManager,
                                  DesktopAppModel *appModel = nullptr,
                                  DaemonHealthMonitor *healthMonitor = nullptr,
                                  QObject *parent = nullptr);
    ~DesktopDaemonService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;
    QString backend() const;
    bool connected() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    // Deprecated legacy alias API kept for compatibility.
    void ShowOverview();
    void HideOverview();
    void ToggleOverview();
    // Canonical workspace API.
    void ShowWorkspace();
    void HideWorkspace();
    void ToggleWorkspace();
    void ShowAppGrid();
    void ShowDesktop();
    bool PresentWindow(const QString &app_id);
    bool PresentView(const QString &view_id);
    bool CloseView(const QString &view_id);
    void SwitchWorkspace(int index);
    void SwitchWorkspaceByDelta(int delta);
    bool MoveWindowToWorkspace(const QVariant &window, int index);
    bool MoveFocusedWindowByDelta(int delta);
    QVariantList ListRankedApps(int limit = 24) const;
    QVariantMap DiagnosticSnapshot() const;
    QVariantMap DaemonHealthSnapshot() const;

signals:
    void serviceRegisteredChanged();
    void backendChanged();
    void connectedChanged();

    // Deprecated legacy alias signals kept for compatibility.
    void OverviewShown();
    void OverviewVisibilityChanged(bool visible);
    // Canonical workspace signals.
    void WorkspaceShown();
    void WorkspaceVisibilityChanged(bool visible);
    void WorkspaceChanged();
    void WindowAttention(const QVariantMap &window);

private:
    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);

    WorkspaceManager *m_workspaceManager = nullptr;
    WindowingBackendManager *m_windowingBackend = nullptr;
    SpacesManager *m_spacesManager = nullptr;
    DesktopAppModel *m_appModel = nullptr;
    DaemonHealthMonitor *m_healthMonitor = nullptr;
    bool m_serviceRegistered = false;

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
