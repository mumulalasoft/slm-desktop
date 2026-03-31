#pragma once

#include <QObject>
#include <QTimer>
#include <QHash>

class ShellStateController;

// ShellLayerWatchdog — persistent-layer health monitor and stuck-overlay recovery.
//
// Responsibilities:
//   1. Tracks how long each overlay has been continuously visible.
//   2. Fires overlayStuckDetected() when an overlay exceeds overlayStuckThresholdMs.
//   3. Provides requestRecovery() which dismisses all overlays and restores base state.
//   4. Runs a 1-second health check timer emitting healthCheckCompleted(bool healthy).
//
// "Healthy" means: no overlay has been stuck beyond the threshold and
// ShellStateController is in a consistent state (no overlay exclusively blocks base shell).

class ShellLayerWatchdog : public QObject
{
    Q_OBJECT
    // No QML_ELEMENT: exposed via context property. See shellstatecontroller.h.

    Q_PROPERTY(bool anyOverlayStuck READ anyOverlayStuck NOTIFY anyOverlayStuckChanged)
    Q_PROPERTY(int overlayStuckThresholdMs READ overlayStuckThresholdMs WRITE setOverlayStuckThresholdMs)
    Q_PROPERTY(int healthCheckIntervalMs READ healthCheckIntervalMs WRITE setHealthCheckIntervalMs)

public:
    explicit ShellLayerWatchdog(ShellStateController *stateController, QObject *parent = nullptr);

    bool anyOverlayStuck() const;

    int overlayStuckThresholdMs() const;
    void setOverlayStuckThresholdMs(int ms);

    int healthCheckIntervalMs() const;
    void setHealthCheckIntervalMs(int ms);

    // Force-dismiss all overlays and restore base shell state.
    Q_INVOKABLE void requestRecovery();

    // Manually report an overlay load failure from QML Loader error boundaries.
    Q_INVOKABLE void reportOverlayLoadError(const QString &overlayName);

signals:
    void anyOverlayStuckChanged(bool stuck);
    void overlayStuckDetected(const QString &overlayName, int durationMs);
    void persistentLayerRestored();
    void healthCheckCompleted(bool healthy);
    void overlayLoadErrorReceived(const QString &overlayName);

private:
    void connectStateController();
    void onHealthTimerFired();
    void trackVisible(const QString &overlayName, bool visible);
    void updateAnyOverlayStuck(bool stuck);

    ShellStateController *m_stateController = nullptr;

    int m_overlayStuckThresholdMs = 30000; // 30 s — conservative default
    bool m_anyOverlayStuck = false;

    QTimer m_healthTimer;

    struct OverlayEntry {
        bool visible = false;
        qint64 visibleSinceMs = 0; // epoch ms when overlay became visible
    };
    QHash<QString, OverlayEntry> m_trackers;
};
