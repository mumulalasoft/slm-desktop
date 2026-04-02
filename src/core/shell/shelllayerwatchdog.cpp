#include "shelllayerwatchdog.h"
#include "shellstatecontroller.h"

#include <QDateTime>
#include <QDebug>

ShellLayerWatchdog::ShellLayerWatchdog(ShellStateController *stateController, QObject *parent)
    : QObject(parent)
    , m_stateController(stateController)
{
    // Initialise tracker entries for all known transient overlays.
    for (const char *name : {"launchpad", "workspace", "tothespot", "style_gallery"}) {
        m_trackers.insert(QLatin1String(name), OverlayEntry{});
    }

    m_healthTimer.setInterval(1000); // 1 s health cadence
    QObject::connect(&m_healthTimer, &QTimer::timeout,
                     this, &ShellLayerWatchdog::onHealthTimerFired);
    m_healthTimer.start();

    connectStateController();
}

bool ShellLayerWatchdog::anyOverlayStuck() const { return m_anyOverlayStuck; }

int ShellLayerWatchdog::overlayStuckThresholdMs() const { return m_overlayStuckThresholdMs; }

void ShellLayerWatchdog::setOverlayStuckThresholdMs(int ms)
{
    m_overlayStuckThresholdMs = qMax(1, ms);
}

int ShellLayerWatchdog::healthCheckIntervalMs() const
{
    return m_healthTimer.interval();
}

void ShellLayerWatchdog::setHealthCheckIntervalMs(int ms)
{
    m_healthTimer.setInterval(qMax(1, ms));
    if (!m_healthTimer.isActive()) {
        m_healthTimer.start();
    }
}

void ShellLayerWatchdog::requestRecovery()
{
    qWarning().noquote() << "[watchdog] requestRecovery() called — dismissing all overlays";
    if (m_stateController) {
        m_stateController->dismissAllOverlays();
    }
    // Reset all tracker timestamps so they don't immediately re-fire.
    for (auto &entry : m_trackers) {
        entry.visible = false;
        entry.visibleSinceMs = 0;
        entry.stuckReported = false;
    }
    updateAnyOverlayStuck(false);
    emit persistentLayerRestored();
}

void ShellLayerWatchdog::reportOverlayLoadError(const QString &overlayName)
{
    qWarning().noquote() << "[watchdog] overlay load error reported:" << overlayName;
    // Force-clear the overlay state so ShellStateController doesn't think it's visible.
    if (m_stateController) {
        if (overlayName == QLatin1String("launchpad")) {
            m_stateController->setLaunchpadVisible(false);
        } else if (overlayName == QLatin1String("workspace")) {
            m_stateController->setWorkspaceOverviewVisible(false);
        } else if (overlayName == QLatin1String("tothespot")) {
            m_stateController->setToTheSpotVisible(false);
        } else if (overlayName == QLatin1String("style_gallery")) {
            m_stateController->setStyleGalleryVisible(false);
        }
    }
    trackVisible(overlayName, false);
    emit overlayLoadErrorReceived(overlayName);
}

void ShellLayerWatchdog::connectStateController()
{
    if (!m_stateController) {
        return;
    }
    QObject::connect(m_stateController, &ShellStateController::launchpadVisibleChanged,
                     this, [this](bool visible) { trackVisible(QStringLiteral("launchpad"), visible); });
    QObject::connect(m_stateController, &ShellStateController::workspaceOverviewVisibleChanged,
                     this, [this](bool visible) { trackVisible(QStringLiteral("workspace"), visible); });
    QObject::connect(m_stateController, &ShellStateController::toTheSpotVisibleChanged,
                     this, [this](bool visible) { trackVisible(QStringLiteral("tothespot"), visible); });
    QObject::connect(m_stateController, &ShellStateController::styleGalleryVisibleChanged,
                     this, [this](bool visible) { trackVisible(QStringLiteral("style_gallery"), visible); });
}

void ShellLayerWatchdog::trackVisible(const QString &overlayName, bool visible)
{
    auto it = m_trackers.find(overlayName);
    if (it == m_trackers.end()) {
        return;
    }
    if (it->visible == visible) {
        return;
    }
    it->visible = visible;
    it->visibleSinceMs = visible ? QDateTime::currentMSecsSinceEpoch() : 0;
    if (!visible)
        it->stuckReported = false;
}

void ShellLayerWatchdog::onHealthTimerFired()
{
    bool anyStuck = false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (auto it = m_trackers.begin(); it != m_trackers.end(); ++it) {
        auto &entry = it.value();
        if (!entry.visible || entry.visibleSinceMs <= 0) {
            continue;
        }
        const int durationMs = static_cast<int>(now - entry.visibleSinceMs);
        if (durationMs >= m_overlayStuckThresholdMs) {
            anyStuck = true;
            if (!entry.stuckReported) {
                entry.stuckReported = true;
                qWarning().noquote()
                    << "[watchdog] overlay stuck:" << it.key()
                    << "visible for" << durationMs << "ms";
                emit overlayStuckDetected(it.key(), durationMs);
            }
        }
    }

    updateAnyOverlayStuck(anyStuck);
    emit healthCheckCompleted(!anyStuck);
}

void ShellLayerWatchdog::updateAnyOverlayStuck(bool stuck)
{
    if (m_anyOverlayStuck == stuck) {
        return;
    }
    m_anyOverlayStuck = stuck;
    emit anyOverlayStuckChanged(stuck);
}
