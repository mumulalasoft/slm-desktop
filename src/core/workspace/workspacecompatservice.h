#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantMap>

class WorkspaceManager;
class WindowingBackendManager;
class SpacesManager;

class WorkspaceCompatService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop_shell.WorkspaceManager1")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit WorkspaceCompatService(WorkspaceManager *workspaceManager,
                                    WindowingBackendManager *windowingBackend,
                                    SpacesManager *spacesManager,
                                    QObject *parent = nullptr);
    ~WorkspaceCompatService() override;

    bool serviceRegistered() const;

public slots:
    // Deprecated legacy alias API; prefer Show/Hide/ToggleWorkspace.
    void ShowOverview();
    void HideOverview();
    void ToggleOverview();
    void ShowWorkspace();
    void HideWorkspace();
    void ToggleWorkspace();
    void ShowAppGrid();
    void ShowDesktop();
    bool PresentWindow(const QString &app_id);
    bool PresentView(const QString &view_id);
    bool CloseView(const QString &view_id);
    void SwitchWorkspace(int index);
    bool MoveWindowToWorkspace(const QVariant &window, int index);
    QVariantMap DiagnosticSnapshot() const;

signals:
    void serviceRegisteredChanged();
    // Deprecated legacy alias signals for old clients.
    void OverviewShown();
    void OverviewVisibilityChanged(bool visible);
    // Canonical workspace signals mirrored for newer clients.
    void WorkspaceShown();
    void WorkspaceVisibilityChanged(bool visible);
    void WorkspaceChanged();
    void WindowAttention(const QVariantMap &window);

private:
    void registerDbusService();

    WorkspaceManager *m_workspaceManager = nullptr;
    WindowingBackendManager *m_windowingBackend = nullptr;
    SpacesManager *m_spacesManager = nullptr;
    bool m_serviceRegistered = false;
};
