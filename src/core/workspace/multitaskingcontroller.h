#pragma once

#include <QObject>

class QTimer;
class WorkspaceManager;
class SpacesManager;

class MultitaskingController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool workspaceVisible READ workspaceVisible NOTIFY workspaceVisibleChanged)
    // Deprecated compatibility alias: use workspaceVisible.
    Q_PROPERTY(bool overviewVisible READ overviewVisible NOTIFY overviewVisibleChanged)
    Q_PROPERTY(bool switchingWorkspace READ switchingWorkspace NOTIFY switchingWorkspaceChanged)
    Q_PROPERTY(int activeWorkspace READ activeWorkspace NOTIFY activeWorkspaceChanged)
    Q_PROPERTY(int workspaceCount READ workspaceCount NOTIFY workspaceCountChanged)
    Q_PROPERTY(bool workspaceSwipeActive READ workspaceSwipeActive NOTIFY workspaceSwipeActiveChanged)
    Q_PROPERTY(bool workspaceSwipeSettling READ workspaceSwipeSettling NOTIFY workspaceSwipeSettlingChanged)
    Q_PROPERTY(double workspaceSwipeOffset READ workspaceSwipeOffset NOTIFY workspaceSwipeOffsetChanged)
    Q_PROPERTY(int workspaceSwipeStartSpace READ workspaceSwipeStartSpace NOTIFY workspaceSwipeStartSpaceChanged)
    Q_PROPERTY(int workspaceSwipeTargetSpace READ workspaceSwipeTargetSpace NOTIFY workspaceSwipeTargetSpaceChanged)
    Q_PROPERTY(bool spaceSwitchHudVisible READ spaceSwitchHudVisible NOTIFY spaceSwitchHudVisibleChanged)
    Q_PROPERTY(QString spaceSwitchHudText READ spaceSwitchHudText NOTIFY spaceSwitchHudTextChanged)
    Q_PROPERTY(int spaceSwitchDirection READ spaceSwitchDirection NOTIFY spaceSwitchDirectionChanged)

public:
    enum Mode {
        NormalMode = 0,
        WorkspaceOverviewMode = 1,
        WorkspaceSwitchingMode = 2
    };
    Q_ENUM(Mode)

    explicit MultitaskingController(WorkspaceManager *workspaceManager,
                                    SpacesManager *spacesManager,
                                    QObject *parent = nullptr);

    Mode mode() const;
    bool workspaceVisible() const;
    bool overviewVisible() const; // deprecated alias
    bool switchingWorkspace() const;
    int activeWorkspace() const;
    int workspaceCount() const;
    bool workspaceSwipeActive() const;
    bool workspaceSwipeSettling() const;
    double workspaceSwipeOffset() const;
    int workspaceSwipeStartSpace() const;
    int workspaceSwipeTargetSpace() const;
    bool spaceSwitchHudVisible() const;
    QString spaceSwitchHudText() const;
    int spaceSwitchDirection() const;

    Q_INVOKABLE void toggleWorkspace();
    Q_INVOKABLE void showWorkspace();
    Q_INVOKABLE void exitWorkspace();
    // Deprecated alias API: use toggle/show/exitWorkspace.
    Q_INVOKABLE void toggleOverview();
    Q_INVOKABLE void showOverview();
    Q_INVOKABLE void exitOverview();
    Q_INVOKABLE void switchWorkspace(int index);
    Q_INVOKABLE void switchWorkspaceDelta(int delta);
    Q_INVOKABLE bool presentWindow(const QString &appId);
    Q_INVOKABLE bool presentView(const QString &viewId);
    Q_INVOKABLE void beginSwipe();
    Q_INVOKABLE void updateSwipe(double offset);
    Q_INVOKABLE void finishSwipe(double dx, double velocity);
    Q_INVOKABLE void showSpaceSwitchHud(int nextSpace, int prevSpace);
    Q_INVOKABLE void hideSpaceSwitchHud();

signals:
    void modeChanged();
    void overviewVisibleChanged();
    void workspaceVisibleChanged();
    void switchingWorkspaceChanged();
    void activeWorkspaceChanged();
    void workspaceCountChanged();
    void workspaceSwitchStarted(int fromWorkspace, int toWorkspace);
    void workspaceSwitchSettled(int activeWorkspace);
    void workspaceSwipeActiveChanged();
    void workspaceSwipeSettlingChanged();
    void workspaceSwipeOffsetChanged();
    void workspaceSwipeStartSpaceChanged();
    void workspaceSwipeTargetSpaceChanged();
    void spaceSwitchHudVisibleChanged();
    void spaceSwitchHudTextChanged();
    void spaceSwitchDirectionChanged();

private:
    void setMode(Mode nextMode);
    void updateWorkspaceVisible(bool visible);
    void beginTransientSwitch(int fromWorkspace, int toWorkspace);
    void endTransientSwitch();
    int clampWorkspaceIndex(int index) const;

    WorkspaceManager *m_workspaceManager = nullptr;
    SpacesManager *m_spacesManager = nullptr;
    QTimer *m_switchSettleTimer = nullptr;
    QTimer *m_hudHideTimer = nullptr;

    Mode m_mode = NormalMode;
    bool m_workspaceVisible = false;
    bool m_switchingWorkspace = false;
    int m_activeWorkspace = 1;
    int m_workspaceCount = 1;
    int m_pendingSwitchTarget = -1;
    bool m_workspaceSwipeActive = false;
    bool m_workspaceSwipeSettling = false;
    double m_workspaceSwipeOffset = 0;
    int m_workspaceSwipeStartSpace = 1;
    int m_workspaceSwipeTargetSpace = 1;

    bool m_spaceSwitchHudVisible = false;
    QString m_spaceSwitchHudText;
    int m_spaceSwitchDirection = 0;
};
