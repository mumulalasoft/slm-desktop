#include <QtTest/QtTest>
#include "../src/core/shell/shellstatecontroller.h"
#include "../src/core/shell/shellinputrouter.h"

class ShellInputRouterTest : public QObject
{
    Q_OBJECT

private slots:
    // ── activeLayer ───────────────────────────────────────────────────────────

    void baseLayer_whenNoOverlayActive()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        QCOMPARE(router.activeLayer(), ShellInputRouter::ShellLayer::BaseLayer);
    }

    void apphubLayer_whenAppHubVisible()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setAppDeckVisible(true);
        QCOMPARE(router.activeLayer(), ShellInputRouter::ShellLayer::AppDeck);
    }

    void workspaceOverviewLayer_whenWorkspaceVisible()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setWorkspaceOverviewVisible(true);
        QCOMPARE(router.activeLayer(), ShellInputRouter::ShellLayer::WorkspaceOverview);
    }

    void toTheSpotLayer_whenPulseVisible()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setPulseVisible(true);
        QCOMPARE(router.activeLayer(), ShellInputRouter::ShellLayer::Pulse);
    }

    void lockScreenLayer_isHighestPriority()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setAppDeckVisible(true);
        state.setPulseVisible(true);
        state.setLockScreenActive(true);
        QCOMPARE(router.activeLayer(), ShellInputRouter::ShellLayer::LockScreen);
    }

    void toTheSpot_outprioritisesAppHub()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setAppDeckVisible(true);
        state.setPulseVisible(true);
        QCOMPARE(router.activeLayer(), ShellInputRouter::ShellLayer::Pulse);
    }

    // ── canDispatch / layer blocking ─────────────────────────────────────────

    void lockScreen_blocksAllExceptLock()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setLockScreenActive(true);

        QVERIFY(!router.canDispatch(QStringLiteral("shell.file_manager")));
        QVERIFY(!router.canDispatch(QStringLiteral("shell.pulse")));
        QVERIFY(!router.canDispatch(QStringLiteral("shell.settings")));
        QVERIFY(!router.canDispatch(QStringLiteral("workspace.prev")));
        QVERIFY(!router.canDispatch(QStringLiteral("workspace.next")));
        QVERIFY(!router.canDispatch(QStringLiteral("shell.workspace_overview")));
        // Only lock itself is permitted
        QVERIFY(router.canDispatch(QStringLiteral("shell.lock")));
    }

    void lockScreen_blocksScreenshots()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setLockScreenActive(true);

        // Hard-block screenshots while locked — defense-in-depth at the input gate.
        QVERIFY(!router.canDispatch(QStringLiteral("screenshot.area")));
        QVERIFY(!router.canDispatch(QStringLiteral("screenshot.fullscreen")));

        QSignalSpy spy(&router, &ShellInputRouter::actionDispatched);
        QCOMPARE(router.dispatch(QStringLiteral("screenshot.area")),
                 ShellInputRouter::DispatchResult::BlockedByMode);
        QCOMPARE(router.dispatch(QStringLiteral("screenshot.fullscreen")),
                 ShellInputRouter::DispatchResult::BlockedByMode);
        QCOMPARE(spy.count(), 0);

        // Unlocking restores screenshot dispatch.
        state.setLockScreenActive(false);
        QVERIFY(router.canDispatch(QStringLiteral("screenshot.area")));
        QVERIFY(router.canDispatch(QStringLiteral("screenshot.fullscreen")));
    }

    void apphub_blocksWorkspaceNav()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setAppDeckVisible(true);

        QVERIFY(!router.canDispatch(QStringLiteral("workspace.prev")));
        QVERIFY(!router.canDispatch(QStringLiteral("workspace.next")));
        QVERIFY(!router.canDispatch(QStringLiteral("window.move_workspace_prev")));
        QVERIFY(!router.canDispatch(QStringLiteral("window.move_workspace_next")));
        // Non-nav actions are still allowed
        QVERIFY(router.canDispatch(QStringLiteral("overlay.dismiss")));
        QVERIFY(router.canDispatch(QStringLiteral("shell.settings")));
    }

    void workspaceOverview_blocksPulse_andClipboard()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setWorkspaceOverviewVisible(true);

        QVERIFY(!router.canDispatch(QStringLiteral("shell.pulse")));
        QVERIFY(!router.canDispatch(QStringLiteral("shell.clipboard")));
        // Workspace nav remains allowed inside overview
        QVERIFY(router.canDispatch(QStringLiteral("workspace.prev")));
        QVERIFY(router.canDispatch(QStringLiteral("workspace.next")));
        QVERIFY(router.canDispatch(QStringLiteral("overlay.dismiss")));
    }

    void toTheSpot_blocksWorkspaceNav()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setPulseVisible(true);

        QVERIFY(!router.canDispatch(QStringLiteral("shell.workspace_overview")));
        QVERIFY(!router.canDispatch(QStringLiteral("workspace.prev")));
        QVERIFY(!router.canDispatch(QStringLiteral("workspace.next")));
        // Other actions still allowed
        QVERIFY(router.canDispatch(QStringLiteral("overlay.dismiss")));
        QVERIFY(router.canDispatch(QStringLiteral("shell.settings")));
    }

    void baseLayer_allowsAllKnownActions()
    {
        ShellStateController state;
        ShellInputRouter router(&state);

        const QStringList allActions{
            QStringLiteral("shell.file_manager"),
            QStringLiteral("shell.print"),
            QStringLiteral("shell.pulse"),
            QStringLiteral("shell.clipboard"),
            QStringLiteral("shell.settings"),
            QStringLiteral("shell.workspace_overview"),
            QStringLiteral("shell.lock"),
            QStringLiteral("workspace.prev"),
            QStringLiteral("workspace.next"),
            QStringLiteral("window.move_workspace_prev"),
            QStringLiteral("window.move_workspace_next"),
            QStringLiteral("overlay.dismiss"),
            QStringLiteral("screenshot.area"),
            QStringLiteral("screenshot.fullscreen"),
            QStringLiteral("debug.motion_overlay"),
        };
        for (const auto &action : allActions) {
            QVERIFY2(router.canDispatch(action), qPrintable(action + " should be allowed in BaseLayer"));
        }
    }

    // ── dispatch ─────────────────────────────────────────────────────────────

    void dispatch_emitsSignal_whenAllowed()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        QSignalSpy spy(&router, &ShellInputRouter::actionDispatched);

        const auto result = router.dispatch(QStringLiteral("shell.settings"));
        QCOMPARE(result, ShellInputRouter::DispatchResult::Dispatched);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("shell.settings"));
    }

    void dispatch_blockedByMode_noSignal()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setLockScreenActive(true);

        QSignalSpy spy(&router, &ShellInputRouter::actionDispatched);
        const auto result = router.dispatch(QStringLiteral("shell.settings"));
        QCOMPARE(result, ShellInputRouter::DispatchResult::BlockedByMode);
        QCOMPARE(spy.count(), 0);
    }

    void rapidDispatch_noDeadlock()
    {
        // Rapidly toggle overlays while dispatching actions — must not deadlock or crash.
        ShellStateController state;
        ShellInputRouter router(&state);

        for (int i = 0; i < 200; ++i) {
            state.setAppDeckVisible(i % 2 == 0);
            state.setWorkspaceOverviewVisible(i % 3 == 0);
            state.setPulseVisible(i % 5 == 0);
            router.dispatch(QStringLiteral("workspace.prev"));
            router.dispatch(QStringLiteral("shell.settings"));
            router.dispatch(QStringLiteral("overlay.dismiss"));
        }
        // If we reach here without crash/assert, the test passes.
        QVERIFY(true);
    }

    // ── layerChanged signal ───────────────────────────────────────────────────

    void layerChanged_emitted_whenLayerTransitions()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        QSignalSpy spy(&router, &ShellInputRouter::layerChanged);

        state.setAppDeckVisible(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<ShellInputRouter::ShellLayer>(),
                 ShellInputRouter::ShellLayer::AppDeck);

        state.setAppDeckVisible(false);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().first().value<ShellInputRouter::ShellLayer>(),
                 ShellInputRouter::ShellLayer::BaseLayer);
    }

    void layerChanged_notEmitted_onNoop()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        state.setAppDeckVisible(true);

        QSignalSpy spy(&router, &ShellInputRouter::layerChanged);
        state.setAppDeckVisible(true); // same value
        QCOMPARE(spy.count(), 0);
    }

    // ── Timeout guard ─────────────────────────────────────────────────────────

    void forceDismiss_doesNothing_whenOverlayAlreadyGone()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        router.setDismissTimeoutMs(20);

        QSignalSpy timeoutSpy(&router, &ShellInputRouter::overlayDismissTimedOut);

        // Overlay already gone before timeout fires
        state.setAppDeckVisible(false);
        router.scheduleForceDismiss(QStringLiteral("appdeck"));
        QTest::qWait(40); // let timer fire

        QCOMPARE(timeoutSpy.count(), 0);
    }

    void forceDismiss_fires_andDismissesStuckOverlay()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        router.setDismissTimeoutMs(20);

        QSignalSpy timeoutSpy(&router, &ShellInputRouter::overlayDismissTimedOut);

        state.setAppDeckVisible(true);
        router.scheduleForceDismiss(QStringLiteral("appdeck"));
        QTest::qWait(40); // let timer fire

        QCOMPARE(timeoutSpy.count(), 1);
        QCOMPARE(timeoutSpy.first().first().toString(), QStringLiteral("appdeck"));
        QVERIFY(!state.appdeckVisible());
    }

    void forceDismiss_canBeCancelled()
    {
        ShellStateController state;
        ShellInputRouter router(&state);
        router.setDismissTimeoutMs(30);

        QSignalSpy timeoutSpy(&router, &ShellInputRouter::overlayDismissTimedOut);

        state.setAppDeckVisible(true);
        router.scheduleForceDismiss(QStringLiteral("appdeck"));
        router.cancelForceDismiss(QStringLiteral("appdeck"));
        QTest::qWait(60); // well past the original timeout

        QCOMPARE(timeoutSpy.count(), 0);
        // Overlay is still visible because we cancelled the force-dismiss
        QVERIFY(state.appdeckVisible());
    }
};

QTEST_MAIN(ShellInputRouterTest)
#include "shell_input_router_test.moc"
