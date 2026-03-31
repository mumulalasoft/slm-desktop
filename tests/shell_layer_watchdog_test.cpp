#include <QtTest/QtTest>
#include "../src/core/shell/shellstatecontroller.h"
#include "../src/core/shell/shelllayerwatchdog.h"

class ShellLayerWatchdogTest : public QObject
{
    Q_OBJECT

private slots:
    // ── initial state ─────────────────────────────────────────────────────────

    void anyOverlayStuck_isFalse_initially()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        QVERIFY(!watchdog.anyOverlayStuck());
    }

    void defaultThreshold_is30s()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        QCOMPARE(watchdog.overlayStuckThresholdMs(), 30000);
    }

    void defaultHealthInterval_is1s()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        QCOMPARE(watchdog.healthCheckIntervalMs(), 1000);
    }

    // ── threshold / interval setters ─────────────────────────────────────────

    void setOverlayStuckThresholdMs_updatesValue()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(5000);
        QCOMPARE(watchdog.overlayStuckThresholdMs(), 5000);
    }

    void setOverlayStuckThresholdMs_clampsToOne()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(0);
        QCOMPARE(watchdog.overlayStuckThresholdMs(), 1);
    }

    void setHealthCheckIntervalMs_updatesValue()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setHealthCheckIntervalMs(200);
        QCOMPARE(watchdog.healthCheckIntervalMs(), 200);
    }

    void setHealthCheckIntervalMs_clampsToOne()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setHealthCheckIntervalMs(-5);
        QCOMPARE(watchdog.healthCheckIntervalMs(), 1);
    }

    // ── stuck overlay detection ───────────────────────────────────────────────

    void overlayStuckDetected_fires_whenOverlayExceedsThreshold()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(50);
        watchdog.setHealthCheckIntervalMs(30);

        QSignalSpy stuckSpy(&watchdog, &ShellLayerWatchdog::overlayStuckDetected);
        QSignalSpy anyStuckSpy(&watchdog, &ShellLayerWatchdog::anyOverlayStuckChanged);

        state.setLaunchpadVisible(true);
        QTest::qWait(130); // long enough for health timer to fire and detect stuck

        QVERIFY(stuckSpy.count() >= 1);
        QCOMPARE(stuckSpy.first().first().toString(), QStringLiteral("launchpad"));
        QVERIFY(watchdog.anyOverlayStuck());
        QVERIFY(anyStuckSpy.count() >= 1);
        QCOMPARE(anyStuckSpy.first().first().toBool(), true);
    }

    void overlayStuckDetected_notFired_beforeThreshold()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(10000); // 10 s — won't fire in test
        watchdog.setHealthCheckIntervalMs(30);

        QSignalSpy stuckSpy(&watchdog, &ShellLayerWatchdog::overlayStuckDetected);

        state.setLaunchpadVisible(true);
        QTest::qWait(80);

        QCOMPARE(stuckSpy.count(), 0);
        QVERIFY(!watchdog.anyOverlayStuck());
    }

    void healthCheckCompleted_true_whenNoOverlayStuck()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(10000);
        watchdog.setHealthCheckIntervalMs(30);

        QSignalSpy healthSpy(&watchdog, &ShellLayerWatchdog::healthCheckCompleted);
        QTest::qWait(80);

        QVERIFY(healthSpy.count() >= 1);
        QVERIFY(healthSpy.last().first().toBool()); // healthy
    }

    void healthCheckCompleted_false_whenOverlayStuck()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(50);
        watchdog.setHealthCheckIntervalMs(30);

        QSignalSpy healthSpy(&watchdog, &ShellLayerWatchdog::healthCheckCompleted);

        state.setLaunchpadVisible(true);
        QTest::qWait(130);

        // At least one health-check should have reported unhealthy.
        const bool anyUnhealthy = [&] {
            for (const auto &call : healthSpy) {
                if (!call.first().toBool()) return true;
            }
            return false;
        }();
        QVERIFY(anyUnhealthy);
    }

    // ── requestRecovery ───────────────────────────────────────────────────────

    void requestRecovery_dismissesAllOverlays()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        state.setLaunchpadVisible(true);
        state.setWorkspaceOverviewVisible(true);
        state.setToTheSpotVisible(true);

        watchdog.requestRecovery();

        QVERIFY(!state.launchpadVisible());
        QVERIFY(!state.workspaceOverviewVisible());
        QVERIFY(!state.toTheSpotVisible());
    }

    void requestRecovery_emitsPersistentLayerRestored()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        QSignalSpy spy(&watchdog, &ShellLayerWatchdog::persistentLayerRestored);
        watchdog.requestRecovery();

        QCOMPARE(spy.count(), 1);
    }

    void requestRecovery_resetsAnyOverlayStuck()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(50);
        watchdog.setHealthCheckIntervalMs(30);

        state.setLaunchpadVisible(true);
        QTest::qWait(130); // trigger stuck detection

        QVERIFY(watchdog.anyOverlayStuck());
        watchdog.requestRecovery();
        QVERIFY(!watchdog.anyOverlayStuck());
    }

    // ── reportOverlayLoadError ────────────────────────────────────────────────

    void reportOverlayLoadError_launchpad_clearsStateController()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        state.setLaunchpadVisible(true);
        watchdog.reportOverlayLoadError(QStringLiteral("launchpad"));

        QVERIFY(!state.launchpadVisible());
    }

    void reportOverlayLoadError_workspace_clearsStateController()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        state.setWorkspaceOverviewVisible(true);
        watchdog.reportOverlayLoadError(QStringLiteral("workspace"));

        QVERIFY(!state.workspaceOverviewVisible());
    }

    void reportOverlayLoadError_tothespot_clearsStateController()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        state.setToTheSpotVisible(true);
        watchdog.reportOverlayLoadError(QStringLiteral("tothespot"));

        QVERIFY(!state.toTheSpotVisible());
    }

    void reportOverlayLoadError_styleGallery_clearsStateController()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        state.setStyleGalleryVisible(true);
        watchdog.reportOverlayLoadError(QStringLiteral("style_gallery"));

        QVERIFY(!state.styleGalleryVisible());
    }

    void reportOverlayLoadError_emitsSignal()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        QSignalSpy spy(&watchdog, &ShellLayerWatchdog::overlayLoadErrorReceived);
        watchdog.reportOverlayLoadError(QStringLiteral("launchpad"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("launchpad"));
    }

    void reportOverlayLoadError_unknownName_doesNotCrash()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        // Should not crash or assert on an unrecognised overlay name.
        watchdog.reportOverlayLoadError(QStringLiteral("nonexistent_overlay"));
        QVERIFY(true);
    }

    // ── stress / crash isolation ──────────────────────────────────────────────

    void rapidToggle_doesNotCrashOrDeadlock()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);
        watchdog.setOverlayStuckThresholdMs(10);
        watchdog.setHealthCheckIntervalMs(10);

        for (int i = 0; i < 300; ++i) {
            state.setLaunchpadVisible(i % 2 == 0);
            state.setWorkspaceOverviewVisible(i % 3 == 0);
            state.setToTheSpotVisible(i % 5 == 0);
            state.setStyleGalleryVisible(i % 7 == 0);
            if (i % 11 == 0) {
                watchdog.requestRecovery();
            }
            if (i % 13 == 0) {
                watchdog.reportOverlayLoadError(QStringLiteral("launchpad"));
            }
        }
        QTest::qWait(60); // let timers fire a few times
        // If we reach here without crash/assert the test passes.
        QVERIFY(true);
    }

    void requestRecovery_afterLoaderError_baseShellAccessible()
    {
        // Simulates: Loader fires onStatusChanged(Error) → reportOverlayLoadError
        // → then requestRecovery is called → all overlays dismissed, shell healthy.
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        state.setLaunchpadVisible(true);
        state.setWorkspaceOverviewVisible(true);

        // Loader error boundary fires for workspace
        watchdog.reportOverlayLoadError(QStringLiteral("workspace"));
        QVERIFY(!state.workspaceOverviewVisible());

        // Watchdog then requests full recovery
        watchdog.requestRecovery();
        QVERIFY(!state.launchpadVisible());
        QVERIFY(!state.workspaceOverviewVisible());
        QVERIFY(!state.toTheSpotVisible());
        QVERIFY(!watchdog.anyOverlayStuck());
    }
};

QTEST_MAIN(ShellLayerWatchdogTest)
#include "shell_layer_watchdog_test.moc"
