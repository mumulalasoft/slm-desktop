#include "shellstatecontroller.h"

ShellStateController::ShellStateController(QObject *parent)
    : QObject(parent)
{
    recomputeDerivedState();
}

bool ShellStateController::launchpadVisible() const        { return m_launchpadVisible; }
bool ShellStateController::workspaceOverviewVisible() const { return m_workspaceOverviewVisible; }
bool ShellStateController::toTheSpotVisible() const        { return m_toTheSpotVisible; }
bool ShellStateController::styleGalleryVisible() const     { return m_styleGalleryVisible; }
bool ShellStateController::showDesktop() const             { return m_showDesktop; }
bool ShellStateController::lockScreenActive() const        { return m_lockScreenActive; }
bool ShellStateController::notificationsVisible() const    { return m_notificationsVisible; }
bool ShellStateController::focusMode() const               { return m_focusMode; }

qreal ShellStateController::topBarOpacity() const              { return m_topBarOpacity; }
qreal ShellStateController::dockOpacity() const                { return m_dockOpacity; }
bool ShellStateController::workspaceBlurred() const            { return m_workspaceBlurred; }
qreal ShellStateController::workspaceBlurAlpha() const         { return m_workspaceBlurAlpha; }
bool ShellStateController::workspaceInteractionBlocked() const  { return m_workspaceInteractionBlocked; }
bool ShellStateController::anyOverlayVisible() const           { return m_anyOverlayVisible; }

void ShellStateController::setLaunchpadVisible(bool visible)
{
    if (m_launchpadVisible == visible) return;
    m_launchpadVisible = visible;
    emit launchpadVisibleChanged(visible);
    recomputeDerivedState();
}

void ShellStateController::setWorkspaceOverviewVisible(bool visible)
{
    if (m_workspaceOverviewVisible == visible) return;
    m_workspaceOverviewVisible = visible;
    emit workspaceOverviewVisibleChanged(visible);
    recomputeDerivedState();
}

void ShellStateController::setToTheSpotVisible(bool visible)
{
    if (m_toTheSpotVisible == visible) return;
    m_toTheSpotVisible = visible;
    emit toTheSpotVisibleChanged(visible);
    recomputeDerivedState();
}

void ShellStateController::setStyleGalleryVisible(bool visible)
{
    if (m_styleGalleryVisible == visible) return;
    m_styleGalleryVisible = visible;
    emit styleGalleryVisibleChanged(visible);
    recomputeDerivedState();
}

void ShellStateController::setShowDesktop(bool active)
{
    if (m_showDesktop == active) return;
    m_showDesktop = active;
    emit showDesktopChanged(active);
    recomputeDerivedState();
}

void ShellStateController::setLockScreenActive(bool active)
{
    if (m_lockScreenActive == active) return;
    m_lockScreenActive = active;
    emit lockScreenActiveChanged(active);
    // Lock screen does not affect the derived opacity/blur state of the shell layers;
    // the lock surface is drawn above everything by the compositor.
}

void ShellStateController::setNotificationsVisible(bool visible)
{
    if (m_notificationsVisible == visible) return;
    m_notificationsVisible = visible;
    emit notificationsVisibleChanged(visible);
}

void ShellStateController::setFocusMode(bool active)
{
    if (m_focusMode == active) return;
    m_focusMode = active;
    emit focusModeChanged(active);
}

void ShellStateController::toggleLaunchpad()
{
    setLaunchpadVisible(!m_launchpadVisible);
}

void ShellStateController::toggleWorkspaceOverview()
{
    setWorkspaceOverviewVisible(!m_workspaceOverviewVisible);
}

void ShellStateController::toggleToTheSpot()
{
    setToTheSpotVisible(!m_toTheSpotVisible);
}

void ShellStateController::dismissAllOverlays()
{
    setLaunchpadVisible(false);
    setWorkspaceOverviewVisible(false);
    setToTheSpotVisible(false);
    setStyleGalleryVisible(false);
}

void ShellStateController::recomputeDerivedState()
{
    // topBarOpacity: hidden during launchpad, full otherwise
    const qreal newTopBarOpacity = m_launchpadVisible ? 0.0 : 1.0;
    if (!qFuzzyCompare(m_topBarOpacity, newTopBarOpacity)) {
        m_topBarOpacity = newTopBarOpacity;
        emit topBarOpacityChanged(m_topBarOpacity);
    }

    // dockOpacity: hidden during show-desktop only; dock remains visible during launchpad.
    const qreal newDockOpacity = m_showDesktop ? 0.0 : 1.0;
    if (!qFuzzyCompare(m_dockOpacity, newDockOpacity)) {
        m_dockOpacity = newDockOpacity;
        emit dockOpacityChanged(m_dockOpacity);
    }

    // workspaceBlurred: blurred when launchpad or show-desktop is active
    const bool newWorkspaceBlurred = m_launchpadVisible || m_showDesktop;
    if (m_workspaceBlurred != newWorkspaceBlurred) {
        m_workspaceBlurred = newWorkspaceBlurred;
        emit workspaceBlurredChanged(m_workspaceBlurred);
    }

    // workspaceBlurAlpha: per-mode blur intensity
    qreal newBlurAlpha = 0.0;
    if (m_launchpadVisible) {
        newBlurAlpha = 0.50;
    } else if (m_showDesktop) {
        newBlurAlpha = 0.40;
    }
    if (!qFuzzyCompare(m_workspaceBlurAlpha, newBlurAlpha)) {
        m_workspaceBlurAlpha = newBlurAlpha;
        emit workspaceBlurAlphaChanged(m_workspaceBlurAlpha);
    }

    // workspaceInteractionBlocked: blocked when launchpad is visible
    const bool newBlocked = m_launchpadVisible;
    if (m_workspaceInteractionBlocked != newBlocked) {
        m_workspaceInteractionBlocked = newBlocked;
        emit workspaceInteractionBlockedChanged(m_workspaceInteractionBlocked);
    }

    // anyOverlayVisible: any transient overlay is active
    const bool newAnyOverlay = m_launchpadVisible || m_workspaceOverviewVisible
                               || m_toTheSpotVisible || m_styleGalleryVisible;
    if (m_anyOverlayVisible != newAnyOverlay) {
        m_anyOverlayVisible = newAnyOverlay;
        emit anyOverlayVisibleChanged(m_anyOverlayVisible);
    }
}
