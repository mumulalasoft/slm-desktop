#pragma once

#include <QObject>
#include <QTimer>

class ShellStateController;

// ShellInputRouter — centralized shell input dispatch with layer-priority gating.
//
// Responsibilities:
//   1. Layer priority: derives active shell layer from ShellStateController state.
//   2. canDispatch(action): returns whether the current layer allows the action.
//   3. dispatch(action): gates via canDispatch, emits actionDispatched.
//   4. Timeout guard: if forceDismiss() is called but the overlay is still alive
//      after 500 ms, dismissAllOverlays() is called on ShellStateController.
//
// QML shortcuts check ShellInputRouter.canDispatch("action.name") instead of
// duplicating lock-screen / overlay-mode guards inline.

class ShellInputRouter : public QObject
{
    Q_OBJECT
    // No QML_ELEMENT: exposed via context property. See shellstatecontroller.h.

public:
    // Layer priority — higher value = higher priority.
    enum class ShellLayer : int {
        BaseLayer         = 0,
        WorkspaceOverview = 10,
        Launchpad         = 20,
        ToTheSpot         = 30,
        LockScreen        = 100,
    };
    Q_ENUM(ShellLayer)

    enum class DispatchResult : int {
        Dispatched      = 0,   // action allowed and emitted
        BlockedByMode   = 1,   // current layer blocks this action
        UnknownAction   = 2,   // action name not in the routing table
    };
    Q_ENUM(DispatchResult)

    explicit ShellInputRouter(ShellStateController *stateController, QObject *parent = nullptr);

    // Returns the highest-priority active layer given the current state.
    ShellLayer activeLayer() const;

    // Returns true if the action is permitted in the current layer.
    Q_INVOKABLE bool canDispatch(const QString &action) const;

    // Gates via canDispatch, emits actionDispatched on success.
    Q_INVOKABLE ShellInputRouter::DispatchResult dispatch(const QString &action);

    // Begins a 500 ms countdown for the named overlay.
    // If the overlay is still visible when the timer fires, dismissAllOverlays() is called.
    Q_INVOKABLE void scheduleForceDismiss(const QString &overlayName);

    // Cancel any pending force-dismiss for the named overlay.
    Q_INVOKABLE void cancelForceDismiss(const QString &overlayName);

    // Dismiss timeout in milliseconds (default: 500). Exposed for tests.
    int dismissTimeoutMs() const;
    void setDismissTimeoutMs(int ms);

signals:
    void actionDispatched(const QString &action);
    void layerChanged(ShellInputRouter::ShellLayer layer);
    void overlayDismissTimedOut(const QString &overlayName);

private:
    static bool actionAllowedInLayer(const QString &action, ShellLayer layer);

    void onStateChanged();

    ShellStateController *m_stateController = nullptr;
    ShellLayer m_currentLayer = ShellLayer::BaseLayer;
    int m_dismissTimeoutMs = 500;

    QString m_pendingDismissOverlay;
    QTimer m_dismissTimeoutTimer;
};
