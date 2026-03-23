#include "multitaskingcontroller.h"

#include "spacesmanager.h"
#include "workspacemanager.h"

#include <QTimer>

MultitaskingController::MultitaskingController(WorkspaceManager *workspaceManager,
                                               SpacesManager *spacesManager,
                                               QObject *parent)
    : QObject(parent)
    , m_workspaceManager(workspaceManager)
    , m_spacesManager(spacesManager)
    , m_switchSettleTimer(new QTimer(this))
    , m_hudHideTimer(new QTimer(this))
{
    m_switchSettleTimer->setSingleShot(true);
    m_switchSettleTimer->setInterval(240);
    connect(m_switchSettleTimer, &QTimer::timeout,
            this, &MultitaskingController::endTransientSwitch);

    m_hudHideTimer->setSingleShot(true);
    m_hudHideTimer->setInterval(520);
    connect(m_hudHideTimer, &QTimer::timeout,
            this, &MultitaskingController::hideSpaceSwitchHud);

    if (m_spacesManager) {
        m_activeWorkspace = m_spacesManager->activeSpace();
        m_workspaceCount = m_spacesManager->spaceCount();

        connect(m_spacesManager, &SpacesManager::activeSpaceChanged, this, [this]() {
            const int next = m_spacesManager ? m_spacesManager->activeSpace() : 1;
            if (m_activeWorkspace != next) {
                m_activeWorkspace = next;
                emit activeWorkspaceChanged();
            }
            if (!m_workspaceVisible && m_switchingWorkspace) {
                // Keep transient switching short-lived: settle when backend reports
                // the new active workspace.
                m_switchSettleTimer->start();
            } else if (m_workspaceVisible) {
                endTransientSwitch();
                setMode(WorkspaceOverviewMode);
            }
        });

        connect(m_spacesManager, &SpacesManager::spaceCountChanged, this, [this]() {
            const int next = m_spacesManager ? m_spacesManager->spaceCount() : 1;
            if (m_workspaceCount != next) {
                m_workspaceCount = next;
                emit workspaceCountChanged();
            }
        });
    }

    if (m_workspaceManager) {
        connect(m_workspaceManager, &WorkspaceManager::WorkspaceVisibilityChanged,
                this, &MultitaskingController::updateWorkspaceVisible);
    }
}

MultitaskingController::Mode MultitaskingController::mode() const
{
    return m_mode;
}

bool MultitaskingController::workspaceVisible() const
{
    return m_workspaceVisible;
}

bool MultitaskingController::overviewVisible() const
{
    return workspaceVisible();
}

bool MultitaskingController::switchingWorkspace() const
{
    return m_switchingWorkspace;
}

int MultitaskingController::activeWorkspace() const
{
    return m_activeWorkspace;
}

int MultitaskingController::workspaceCount() const
{
    return m_workspaceCount;
}

bool MultitaskingController::workspaceSwipeActive() const
{
    return m_workspaceSwipeActive;
}

bool MultitaskingController::workspaceSwipeSettling() const
{
    return m_workspaceSwipeSettling;
}

double MultitaskingController::workspaceSwipeOffset() const
{
    return m_workspaceSwipeOffset;
}

int MultitaskingController::workspaceSwipeStartSpace() const
{
    return m_workspaceSwipeStartSpace;
}

int MultitaskingController::workspaceSwipeTargetSpace() const
{
    return m_workspaceSwipeTargetSpace;
}

bool MultitaskingController::spaceSwitchHudVisible() const
{
    return m_spaceSwitchHudVisible;
}

QString MultitaskingController::spaceSwitchHudText() const
{
    return m_spaceSwitchHudText;
}

int MultitaskingController::spaceSwitchDirection() const
{
    return m_spaceSwitchDirection;
}

void MultitaskingController::toggleOverview()
{
    toggleWorkspace();
}

void MultitaskingController::showOverview()
{
    showWorkspace();
}

void MultitaskingController::exitOverview()
{
    exitWorkspace();
}

void MultitaskingController::toggleWorkspace()
{
    if (m_workspaceVisible) {
        exitWorkspace();
        return;
    }
    showWorkspace();
}

void MultitaskingController::showWorkspace()
{
    if (m_workspaceManager) {
        m_workspaceManager->ShowWorkspace();
    }
    updateWorkspaceVisible(true);
}

void MultitaskingController::exitWorkspace()
{
    if (m_workspaceManager) {
        m_workspaceManager->HideWorkspace();
    }
    updateWorkspaceVisible(false);
}

void MultitaskingController::switchWorkspace(int index)
{
    const int next = clampWorkspaceIndex(index);
    if (next <= 0 || next == m_activeWorkspace) {
        return;
    }

    const int from = m_activeWorkspace;
    if (!m_workspaceVisible) {
        beginTransientSwitch(from, next);
    } else {
        // Contract: switching from workspace strip while overview is visible
        // must not close overview.
        endTransientSwitch();
        setMode(WorkspaceOverviewMode);
    }

    if (m_workspaceManager) {
        m_workspaceManager->SwitchWorkspace(next);
    } else if (m_spacesManager) {
        m_spacesManager->setActiveSpace(next);
    }
}

void MultitaskingController::switchWorkspaceDelta(int delta)
{
    if (delta == 0 || !m_spacesManager) {
        return;
    }
    const int count = qMax(1, m_spacesManager->spaceCount());
    int target = m_activeWorkspace + delta;
    if (target < 1) {
        target = 1;
    } else if (target > count) {
        target = count;
    }
    switchWorkspace(target);
}

bool MultitaskingController::presentWindow(const QString &appId)
{
    // Focus handoff is delegated to WorkspaceManager so switching workspace
    // and focusing the target window stays consistent with backend behavior.
    return m_workspaceManager && m_workspaceManager->PresentWindow(appId);
}

bool MultitaskingController::presentView(const QString &viewId)
{
    // Same as presentWindow: WorkspaceManager remains the authority for
    // workspace-aware focus routing.
    return m_workspaceManager && m_workspaceManager->PresentView(viewId);
}

void MultitaskingController::setMode(Mode nextMode)
{
    if (m_mode == nextMode) {
        return;
    }
    m_mode = nextMode;
    emit modeChanged();
}

void MultitaskingController::updateWorkspaceVisible(bool visible)
{
    const bool next = visible;
    if (m_workspaceVisible != next) {
        m_workspaceVisible = next;
        emit workspaceVisibleChanged();
        emit overviewVisibleChanged();
    }

    if (m_workspaceVisible) {
        endTransientSwitch();
        setMode(WorkspaceOverviewMode);
        return;
    }

    if (!m_switchingWorkspace) {
        setMode(NormalMode);
    }
}

void MultitaskingController::beginTransientSwitch(int fromWorkspace, int toWorkspace)
{
    m_pendingSwitchTarget = toWorkspace;
    if (!m_switchingWorkspace) {
        m_switchingWorkspace = true;
        emit switchingWorkspaceChanged();
    }
    setMode(WorkspaceSwitchingMode);
    emit workspaceSwitchStarted(fromWorkspace, toWorkspace);
    m_switchSettleTimer->start();
}

void MultitaskingController::endTransientSwitch()
{
    if (m_switchSettleTimer->isActive()) {
        m_switchSettleTimer->stop();
    }
    m_pendingSwitchTarget = -1;
    if (m_switchingWorkspace) {
        m_switchingWorkspace = false;
        emit switchingWorkspaceChanged();
        emit workspaceSwitchSettled(m_activeWorkspace);
    }
    setMode(m_workspaceVisible ? WorkspaceOverviewMode : NormalMode);
}

int MultitaskingController::clampWorkspaceIndex(int index) const
{
    const int maxCount = m_spacesManager ? qMax(1, m_spacesManager->spaceCount()) : qMax(1, m_workspaceCount);
    if (index < 1) {
        return 1;
    }
    if (index > maxCount) {
        return maxCount;
    }
    return index;
}

void MultitaskingController::beginSwipe()
{
    if (m_workspaceSwipeActive) {
        return;
    }
    m_workspaceSwipeActive = true;
    m_workspaceSwipeSettling = false;
    m_workspaceSwipeOffset = 0;
    m_workspaceSwipeStartSpace = m_activeWorkspace;
    m_workspaceSwipeTargetSpace = m_activeWorkspace;
    emit workspaceSwipeActiveChanged();
    emit workspaceSwipeSettlingChanged();
    emit workspaceSwipeOffsetChanged();
    emit workspaceSwipeStartSpaceChanged();
    emit workspaceSwipeTargetSpaceChanged();
}

void MultitaskingController::updateSwipe(double offset)
{
    if (!m_workspaceSwipeActive) {
        return;
    }
    if (m_workspaceSwipeOffset == offset) {
        return;
    }
    m_workspaceSwipeOffset = offset;
    emit workspaceSwipeOffsetChanged();
}

void MultitaskingController::finishSwipe(double dx, double velocity)
{
    if (!m_workspaceSwipeActive) {
        return;
    }

    const int startSpace = m_workspaceSwipeStartSpace;
    const int count = m_workspaceCount;
    int target = startSpace;

    // Thresholds from DesktopScene.qml (hardcoded for now as default)
    const double threshold = 96.0; // Math.max(96, width * 0.14)
    const double velocityThreshold = 720.0;

    const bool goNext = (dx <= -threshold) || (velocity <= -velocityThreshold);
    const bool goPrev = (dx >= threshold) || (velocity >= velocityThreshold);

    if (goNext && startSpace < count) {
        target = startSpace + 1;
    } else if (goPrev && startSpace > 1) {
        target = startSpace - 1;
    }

    m_workspaceSwipeTargetSpace = target;
    emit workspaceSwipeTargetSpaceChanged();

    if (target != startSpace) {
        switchWorkspace(target);
    }

    m_workspaceSwipeActive = false;
    m_workspaceSwipeSettling = true;
    m_workspaceSwipeOffset = 0;
    emit workspaceSwipeActiveChanged();
    emit workspaceSwipeSettlingChanged();
    emit workspaceSwipeOffsetChanged();
}

void MultitaskingController::showSpaceSwitchHud(int nextSpace, int prevSpace)
{
    m_spaceSwitchDirection = nextSpace > prevSpace ? 1 : (nextSpace < prevSpace ? -1 : 0);
    m_spaceSwitchHudText = QStringLiteral("Space %1").arg(nextSpace);
    m_spaceSwitchHudVisible = true;
    emit spaceSwitchDirectionChanged();
    emit spaceSwitchHudTextChanged();
    emit spaceSwitchHudVisibleChanged();

    m_hudHideTimer->start();
}

void MultitaskingController::hideSpaceSwitchHud()
{
    if (!m_spaceSwitchHudVisible) {
        return;
    }
    m_spaceSwitchHudVisible = false;
    emit spaceSwitchHudVisibleChanged();
}
