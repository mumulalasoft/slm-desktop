#include "shellstatecontroller.h"

ShellStateController::ShellStateController(QObject *parent)
    : QObject(parent)
{
    recomputeDerivedState();
}

bool ShellStateController::apphubVisible() const        { return m_apphubVisible; }
bool ShellStateController::workspaceOverviewVisible() const { return m_workspaceOverviewVisible; }
bool ShellStateController::toTheSpotVisible() const        { return m_toTheSpotVisible; }
QString ShellStateController::searchQuery() const           { return m_searchQuery; }
QString ShellStateController::appHubSearchSeed() const      { return m_appHubSearchSeed; }
bool ShellStateController::styleGalleryVisible() const     { return m_styleGalleryVisible; }
bool ShellStateController::showDesktop() const             { return m_showDesktop; }
bool ShellStateController::lockScreenActive() const        { return m_lockScreenActive; }
bool ShellStateController::notificationsVisible() const    { return m_notificationsVisible; }
bool ShellStateController::focusMode() const               { return m_focusMode; }
QString ShellStateController::dockHoveredItem() const      { return m_dockHoveredItem; }
QString ShellStateController::dockExpandedItem() const     { return m_dockExpandedItem; }
QVariantMap ShellStateController::dragSession() const       { return m_dragSession; }

qreal ShellStateController::topBarOpacity() const              { return m_topBarOpacity; }
qreal ShellStateController::dockOpacity() const                { return m_dockOpacity; }
bool ShellStateController::workspaceBlurred() const            { return m_workspaceBlurred; }
qreal ShellStateController::workspaceBlurAlpha() const         { return m_workspaceBlurAlpha; }
bool ShellStateController::workspaceInteractionBlocked() const  { return m_workspaceInteractionBlocked; }
bool ShellStateController::anyOverlayVisible() const           { return m_anyOverlayVisible; }

void ShellStateController::setAppHubVisible(bool visible)
{
    if (m_apphubVisible == visible) return;
    m_apphubVisible = visible;
    emit apphubVisibleChanged(visible);
    recomputeDerivedState();
}

void ShellStateController::setWorkspaceOverviewVisible(bool visible)
{
    if (m_workspaceOverviewVisible == visible) return;
    m_workspaceOverviewVisible = visible;
    emit workspaceOverviewVisibleChanged(visible);
    recomputeDerivedState();
}

void ShellStateController::setPulseVisible(bool visible)
{
    if (m_toTheSpotVisible == visible) return;
    m_toTheSpotVisible = visible;
    emit toTheSpotVisibleChanged(visible);
    emit searchVisibleChanged(visible);
    recomputeDerivedState();
}

void ShellStateController::setSearchQuery(const QString &query)
{
    const QString normalized = query;
    if (m_searchQuery == normalized) return;
    m_searchQuery = normalized;
    emit searchQueryChanged(m_searchQuery);
}

void ShellStateController::setAppHubSearchSeed(const QString &seed)
{
    const QString normalized = seed;
    if (m_appHubSearchSeed == normalized) return;
    m_appHubSearchSeed = normalized;
    emit appHubSearchSeedChanged(m_appHubSearchSeed);
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

void ShellStateController::setDockHoveredItem(const QString &itemId)
{
    const QString normalized = itemId;
    if (m_dockHoveredItem == normalized) return;
    m_dockHoveredItem = normalized;
    emit dockHoveredItemChanged(m_dockHoveredItem);
}

void ShellStateController::setDockExpandedItem(const QString &itemId)
{
    const QString normalized = itemId;
    if (m_dockExpandedItem == normalized) return;
    m_dockExpandedItem = normalized;
    emit dockExpandedItemChanged(m_dockExpandedItem);
}

void ShellStateController::setDragSession(const QVariantMap &session)
{
    if (m_dragSession == session) return;
    m_dragSession = session;
    emit dragSessionChanged(m_dragSession);
}

void ShellStateController::clearDragSession()
{
    if (m_dragSession.isEmpty()) return;
    m_dragSession.clear();
    emit dragSessionChanged(m_dragSession);
}

void ShellStateController::toggleAppHub()
{
    setAppHubVisible(!m_apphubVisible);
}

void ShellStateController::toggleWorkspaceOverview()
{
    setWorkspaceOverviewVisible(!m_workspaceOverviewVisible);
}

void ShellStateController::togglePulse()
{
    setPulseVisible(!m_toTheSpotVisible);
}

void ShellStateController::dismissAllOverlays()
{
    setAppHubVisible(false);
    setWorkspaceOverviewVisible(false);
    setPulseVisible(false);
    setStyleGalleryVisible(false);
}

void ShellStateController::recomputeDerivedState()
{
    // topBarOpacity: hidden during apphub, full otherwise
    const qreal newCrownOpacity = m_apphubVisible ? 0.0 : 1.0;
    if (!qFuzzyCompare(m_topBarOpacity, newCrownOpacity)) {
        m_topBarOpacity = newCrownOpacity;
        emit topBarOpacityChanged(m_topBarOpacity);
    }

    // dockOpacity: hidden during show-desktop only; appdeck remains visible during apphub.
    const qreal newDockOpacity = m_showDesktop ? 0.0 : 1.0;
    if (!qFuzzyCompare(m_dockOpacity, newDockOpacity)) {
        m_dockOpacity = newDockOpacity;
        emit dockOpacityChanged(m_dockOpacity);
    }

    // workspaceBlurred: blurred when apphub or show-desktop is active
    const bool newWorkspaceBlurred = m_apphubVisible || m_showDesktop;
    if (m_workspaceBlurred != newWorkspaceBlurred) {
        m_workspaceBlurred = newWorkspaceBlurred;
        emit workspaceBlurredChanged(m_workspaceBlurred);
    }

    // workspaceBlurAlpha: per-mode blur intensity
    qreal newBlurAlpha = 0.0;
    if (m_apphubVisible) {
        newBlurAlpha = 0.50;
    } else if (m_showDesktop) {
        newBlurAlpha = 0.40;
    }
    if (!qFuzzyCompare(m_workspaceBlurAlpha, newBlurAlpha)) {
        m_workspaceBlurAlpha = newBlurAlpha;
        emit workspaceBlurAlphaChanged(m_workspaceBlurAlpha);
    }

    // workspaceInteractionBlocked: blocked when apphub is visible
    const bool newBlocked = m_apphubVisible;
    if (m_workspaceInteractionBlocked != newBlocked) {
        m_workspaceInteractionBlocked = newBlocked;
        emit workspaceInteractionBlockedChanged(m_workspaceInteractionBlocked);
    }

    // anyOverlayVisible: any transient overlay is active
    const bool newAnyOverlay = m_apphubVisible || m_workspaceOverviewVisible
                               || m_toTheSpotVisible || m_styleGalleryVisible;
    if (m_anyOverlayVisible != newAnyOverlay) {
        m_anyOverlayVisible = newAnyOverlay;
        emit anyOverlayVisibleChanged(m_anyOverlayVisible);
    }
}
