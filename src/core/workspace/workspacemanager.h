#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantMap>

class SpacesManager;
class WindowingBackendManager;

class WorkspaceManager : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceManager(WindowingBackendManager *windowingBackend,
                              SpacesManager *spacesManager,
                              QObject *compositorStateModel,
                              QObject *parent = nullptr);

    // Deprecated legacy alias API kept for compatibility.
    Q_INVOKABLE void ShowOverview();
    Q_INVOKABLE void HideOverview();
    Q_INVOKABLE void ToggleOverview();
    // Canonical workspace API.
    Q_INVOKABLE void ShowWorkspace();
    Q_INVOKABLE void HideWorkspace();
    Q_INVOKABLE void ToggleWorkspace();
    Q_INVOKABLE void ShowAppGrid();
    Q_INVOKABLE void ShowDesktop();
    Q_INVOKABLE bool PresentWindow(const QString &app_id);
    Q_INVOKABLE bool PresentView(const QString &view_id);
    Q_INVOKABLE bool CloseView(const QString &view_id);
    Q_INVOKABLE void SwitchWorkspace(int index);
    Q_INVOKABLE void SwitchWorkspaceByDelta(int delta);
    Q_INVOKABLE bool MoveWindowToWorkspace(const QVariant &window, int index);
    Q_INVOKABLE bool MoveFocusedWindowByDelta(int delta);

signals:
    // Deprecated legacy alias signals kept for compatibility.
    void OverviewShown();
    void OverviewVisibilityChanged(bool visible);
    // Canonical workspace signals.
    void WorkspaceShown();
    void WorkspaceVisibilityChanged(bool visible);
    void WorkspaceChanged();
    void WindowAttention(const QVariantMap &window);

private:
    QVariantList activeViewIds() const;
    void reconcileWindowWorkspaceAssignments();
    QVariantMap windowAt(int index) const;
    int windowCount() const;
    QString extractViewId(const QVariant &window) const;
    QString focusedUserViewId() const;
    QVariantMap findWindowByAppId(const QString &appId) const;
    bool focusWindowByViewId(const QString &viewId);
    bool sendCommand(const QString &command);
    void setWorkspaceVisible(bool visible);
    void setOverviewVisible(bool visible); // legacy alias

    WindowingBackendManager *m_windowingBackend = nullptr;
    SpacesManager *m_spacesManager = nullptr;
    QObject *m_compositorStateModel = nullptr;
    bool m_workspaceVisible = false;
};
