#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QMap>

#include "../src/core/shell/shellstatecontroller.h"
#include "../src/core/shell/shelllayerwatchdog.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helper: parse a QML file and extract "readonly property int <name>: <value>"
// ─────────────────────────────────────────────────────────────────────────────
static QMap<QString, int> parseQmlIntProperties(const QString &filePath)
{
    QMap<QString, int> result;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }
    const QString content = QString::fromUtf8(f.readAll());
    static const QRegularExpression re(
        R"(readonly\s+property\s+int\s+(\w+)\s*:\s*(-?\d+))",
        QRegularExpression::MultilineOption);
    auto it = re.globalMatch(content);
    while (it.hasNext()) {
        const auto m = it.next();
        result.insert(m.captured(1), m.captured(2).toInt());
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────

class ShellContractTest : public QObject
{
    Q_OBJECT

private slots:

    // ══════════════════════════════════════════════════════════════════════════
    // 1. persistent_layer_stability_test
    //    Toggle overlays 1000x — dockOpacity and topBarOpacity must always be
    //    in a valid/consistent range, never producing a corrupt derived state.
    // ══════════════════════════════════════════════════════════════════════════

    void persistentLayer_stability_overlayChurn()
    {
        ShellStateController state;

        for (int i = 0; i < 1000; ++i) {
            state.setLaunchpadVisible(i % 2 == 0);
            state.setWorkspaceOverviewVisible(i % 3 == 0);
            state.setToTheSpotVisible(i % 5 == 0);
            state.setStyleGalleryVisible(i % 7 == 0);
            state.setShowDesktop(i % 11 == 0);

            // Contract: topBarOpacity always in [0, 1] and > 0
            QVERIFY2(state.topBarOpacity() > 0.0,
                     "topBarOpacity must never reach 0 — TopBar is always accessible");
            QVERIFY2(state.topBarOpacity() <= 1.0, "topBarOpacity must not exceed 1.0");

            // Contract: dockOpacity in [0, 1]
            QVERIFY2(state.dockOpacity() >= 0.0, "dockOpacity must not be negative");
            QVERIFY2(state.dockOpacity() <= 1.0, "dockOpacity must not exceed 1.0");

            // Contract: workspaceBlurAlpha in [0, 1]
            QVERIFY2(state.workspaceBlurAlpha() >= 0.0, "workspaceBlurAlpha must not be negative");
            QVERIFY2(state.workspaceBlurAlpha() <= 1.0, "workspaceBlurAlpha must not exceed 1.0");
        }
    }

    void persistentLayer_topBarOpacity_neverZero_afterDismissAll()
    {
        ShellStateController state;
        state.setLaunchpadVisible(true);  // dims topBar
        state.dismissAllOverlays();
        QCOMPARE(state.topBarOpacity(), 1.0);
    }

    void persistentLayer_dockOpacity_restoresAfterLaunchpadDismiss()
    {
        ShellStateController state;
        state.setLaunchpadVisible(true);
        QCOMPARE(state.dockOpacity(), 0.0);  // launchpad hides dock
        state.setLaunchpadVisible(false);
        QCOMPARE(state.dockOpacity(), 1.0);  // restored
    }

    void persistentLayer_showDesktop_hideDock_butNotTopBar()
    {
        ShellStateController state;
        state.setShowDesktop(true);
        QCOMPARE(state.dockOpacity(), 0.0);
        QVERIFY(state.topBarOpacity() > 0.0);  // topBar stays accessible
    }

    // ══════════════════════════════════════════════════════════════════════════
    // 2. overlay_isolation_test
    //    An overlay crash (via ShellLayerWatchdog.reportOverlayLoadError) must
    //    not corrupt the persistent layer state (topBar/dock remain accessible).
    // ══════════════════════════════════════════════════════════════════════════

    void overlayIsolation_loadError_doesNotAffectTopBarOpacity()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        // All overlays active — topBarOpacity is dimmed by launchpad
        state.setLaunchpadVisible(true);
        state.setWorkspaceOverviewVisible(true);
        state.setToTheSpotVisible(true);

        // Simulate load errors for all overlays
        watchdog.reportOverlayLoadError(QStringLiteral("launchpad"));
        watchdog.reportOverlayLoadError(QStringLiteral("workspace"));
        watchdog.reportOverlayLoadError(QStringLiteral("tothespot"));

        // Persistent layer must be fully restored
        QCOMPARE(state.topBarOpacity(), 1.0);
        QCOMPARE(state.dockOpacity(), 1.0);
        QVERIFY(!state.launchpadVisible());
        QVERIFY(!state.workspaceOverviewVisible());
        QVERIFY(!state.toTheSpotVisible());
    }

    void overlayIsolation_requestRecovery_persistentLayerIntact()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        state.setLaunchpadVisible(true);
        state.setWorkspaceOverviewVisible(true);

        watchdog.requestRecovery();

        QCOMPARE(state.topBarOpacity(), 1.0);
        QCOMPARE(state.dockOpacity(), 1.0);
        QVERIFY(!state.anyOverlayVisible());
    }

    void overlayIsolation_multipleLoadErrors_noCumulativeCorruption()
    {
        ShellStateController state;
        ShellLayerWatchdog watchdog(&state);

        // Simulate repeated load errors (crash-loop scenario)
        for (int i = 0; i < 10; ++i) {
            state.setLaunchpadVisible(true);
            watchdog.reportOverlayLoadError(QStringLiteral("launchpad"));
            QCOMPARE(state.topBarOpacity(), 1.0);
            QCOMPARE(state.dockOpacity(), 1.0);
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    // 3. state_machine_transition_test
    //    All reachable mode transitions must produce a consistent derived state.
    //    No combination of setXxx() calls should leave the state machine in an
    //    undefined or contradictory condition.
    // ══════════════════════════════════════════════════════════════════════════

    void stateMachine_allOverlaysOff_baselineState()
    {
        ShellStateController state;
        QVERIFY(!state.anyOverlayVisible());
        QCOMPARE(state.topBarOpacity(), 1.0);
        QCOMPARE(state.dockOpacity(), 1.0);
        QVERIFY(!state.workspaceBlurred());
        QCOMPARE(state.workspaceBlurAlpha(), 0.0);
        QVERIFY(!state.workspaceInteractionBlocked());
    }

    void stateMachine_launchpadOn_derivedState()
    {
        ShellStateController state;
        state.setLaunchpadVisible(true);

        QVERIFY(state.anyOverlayVisible());
        QVERIFY(qFuzzyCompare(state.topBarOpacity(), 0.72)); // dimmed
        QCOMPARE(state.dockOpacity(), 0.0);                   // hidden
        QVERIFY(state.workspaceBlurred());
        QVERIFY(state.workspaceInteractionBlocked());
    }

    void stateMachine_workspaceOverview_doesNotAffectPersistentLayers()
    {
        ShellStateController state;
        state.setWorkspaceOverviewVisible(true);

        QVERIFY(state.anyOverlayVisible());
        // WorkspaceOverview is a separate Window — it doesn't dim TopBar or Dock
        QCOMPARE(state.topBarOpacity(), 1.0);
        QCOMPARE(state.dockOpacity(), 1.0);
        QVERIFY(!state.workspaceBlurred());
        QVERIFY(!state.workspaceInteractionBlocked());
    }

    void stateMachine_toTheSpot_doesNotAffectPersistentLayers()
    {
        ShellStateController state;
        state.setToTheSpotVisible(true);

        QVERIFY(state.anyOverlayVisible());
        QCOMPARE(state.topBarOpacity(), 1.0);
        QCOMPARE(state.dockOpacity(), 1.0);
        QVERIFY(!state.workspaceBlurred());
    }

    void stateMachine_showDesktop_blursWorkspace_hidesDock()
    {
        ShellStateController state;
        state.setShowDesktop(true);

        QVERIFY(!state.anyOverlayVisible());  // showDesktop is not an overlay
        QCOMPARE(state.dockOpacity(), 0.0);
        QCOMPARE(state.topBarOpacity(), 1.0); // topBar stays
        QVERIFY(state.workspaceBlurred());
        QVERIFY(qFuzzyCompare(state.workspaceBlurAlpha(), 0.40));
    }

    void stateMachine_launchpadPlusShoDesktop_dockHidden()
    {
        ShellStateController state;
        state.setShowDesktop(true);
        state.setLaunchpadVisible(true);
        // Both conditions hide the dock — opacity must still be 0, not negative
        QCOMPARE(state.dockOpacity(), 0.0);
    }

    void stateMachine_dismissAll_resetsToBaseline()
    {
        ShellStateController state;
        state.setLaunchpadVisible(true);
        state.setWorkspaceOverviewVisible(true);
        state.setToTheSpotVisible(true);
        state.setStyleGalleryVisible(true);

        state.dismissAllOverlays();

        QVERIFY(!state.anyOverlayVisible());
        QCOMPARE(state.topBarOpacity(), 1.0);
        QCOMPARE(state.dockOpacity(), 1.0);
        QVERIFY(!state.workspaceBlurred());
        QCOMPARE(state.workspaceBlurAlpha(), 0.0);
        QVERIFY(!state.workspaceInteractionBlocked());
    }

    void stateMachine_toggles_areIdempotent()
    {
        ShellStateController state;
        state.toggleLaunchpad();
        QVERIFY(state.launchpadVisible());
        state.toggleLaunchpad();
        QVERIFY(!state.launchpadVisible());

        state.toggleWorkspaceOverview();
        QVERIFY(state.workspaceOverviewVisible());
        state.toggleWorkspaceOverview();
        QVERIFY(!state.workspaceOverviewVisible());

        state.toggleToTheSpot();
        QVERIFY(state.toTheSpotVisible());
        state.toggleToTheSpot();
        QVERIFY(!state.toTheSpotVisible());
    }

    void stateMachine_lockScreen_doesNotAlterDerivedOpacity()
    {
        ShellStateController state;
        state.setLockScreenActive(true);
        // Lock screen is drawn by the compositor — ShellStateController
        // must not dim TopBar or Dock in response.
        QCOMPARE(state.topBarOpacity(), 1.0);
        QCOMPARE(state.dockOpacity(), 1.0);
        QVERIFY(!state.workspaceBlurred());
    }

    // ══════════════════════════════════════════════════════════════════════════
    // 4. z_order_policy_test
    //    Parse ShellZOrder.qml and assert the ordering contracts that the shell
    //    layer architecture depends on. Fails if someone silently changes a
    //    z-order constant without reviewing the contract.
    // ══════════════════════════════════════════════════════════════════════════

    void zOrderPolicy_fileIsReadable()
    {
        const QString path = QStringLiteral(SLM_SOURCE_DIR "/Qml/ShellZOrder.qml");
        QVERIFY2(QFile::exists(path), qPrintable("ShellZOrder.qml not found at: " + path));
    }

    void zOrderPolicy_allExpectedConstantsDefined()
    {
        const QString path = QStringLiteral(SLM_SOURCE_DIR "/Qml/ShellZOrder.qml");
        const auto props = parseQmlIntProperties(path);
        const QStringList required{
            QStringLiteral("wallpaper"),
            QStringLiteral("desktopIcons"),
            QStringLiteral("workspaceSurfaces"),
            QStringLiteral("dock"),
            QStringLiteral("topBar"),
            QStringLiteral("pointerCapture"),
            QStringLiteral("debugOverlay"),
        };
        for (const auto &name : required) {
            QVERIFY2(props.contains(name),
                     qPrintable("Missing z-order constant: " + name));
        }
    }

    void zOrderPolicy_dockAboveWorkspaceSurfaces()
    {
        const auto props = parseQmlIntProperties(
            QStringLiteral(SLM_SOURCE_DIR "/Qml/ShellZOrder.qml"));
        QVERIFY2(props.value(QStringLiteral("dock")) > props.value(QStringLiteral("workspaceSurfaces")),
                 "dock must render above workspaceSurfaces");
    }

    void zOrderPolicy_topBarAboveDock()
    {
        const auto props = parseQmlIntProperties(
            QStringLiteral(SLM_SOURCE_DIR "/Qml/ShellZOrder.qml"));
        QVERIFY2(props.value(QStringLiteral("topBar")) > props.value(QStringLiteral("dock")),
                 "topBar must render above dock");
    }

    void zOrderPolicy_pointerCaptureAboveTopBar()
    {
        const auto props = parseQmlIntProperties(
            QStringLiteral(SLM_SOURCE_DIR "/Qml/ShellZOrder.qml"));
        QVERIFY2(props.value(QStringLiteral("pointerCapture")) > props.value(QStringLiteral("topBar")),
                 "pointerCapture must be above topBar to intercept all input");
    }

    void zOrderPolicy_debugOverlayIsHighest()
    {
        const auto props = parseQmlIntProperties(
            QStringLiteral(SLM_SOURCE_DIR "/Qml/ShellZOrder.qml"));
        const int debugOverlay = props.value(QStringLiteral("debugOverlay"));
        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            if (it.key() == QLatin1String("debugOverlay")) continue;
            QVERIFY2(debugOverlay >= it.value(),
                     qPrintable("debugOverlay must be >= " + it.key()
                                + " (got " + QString::number(it.value()) + ")"));
        }
    }

    void zOrderPolicy_dockZOrder_unchangedByShellStateController()
    {
        // Contract: ShellStateController controls opacity, never z-order.
        // Verify by confirming no z-order property exists on ShellStateController.
        ShellStateController state;
        const QMetaObject *meta = state.metaObject();
        for (int i = 0; i < meta->propertyCount(); ++i) {
            const QString name = QString::fromLatin1(meta->property(i).name());
            QVERIFY2(!name.contains(QLatin1String("zOrder"), Qt::CaseInsensitive)
                     && !name.contains(QLatin1String("z_order"), Qt::CaseInsensitive),
                     qPrintable("ShellStateController must not own z-order: " + name));
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    // 5. Performance test
    //    A single mode-switch (state change + derived state recompute) must
    //    complete in < 16ms (one vsync frame at 60 Hz).
    // ══════════════════════════════════════════════════════════════════════════

    void performance_modeSwitchLatency_under16ms()
    {
        ShellStateController state;
        constexpr int kIterations = 1000;
        constexpr qint64 kBudgetMs = 16;

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < kIterations; ++i) {
            state.setLaunchpadVisible(i % 2 == 0);
            state.setWorkspaceOverviewVisible(i % 3 == 0);
            state.setToTheSpotVisible(i % 5 == 0);
            state.setShowDesktop(i % 7 == 0);
            state.dismissAllOverlays();
        }

        const qint64 totalMs = timer.elapsed();
        const qint64 avgUs   = (timer.nsecsElapsed() / kIterations) / 1000;

        qDebug("mode-switch perf: %lld ms total / %lld µs avg per iteration",
               totalMs, avgUs);

        QVERIFY2(avgUs < kBudgetMs * 1000,
                 qPrintable(QString("avg mode-switch %1 µs exceeds 16ms budget").arg(avgUs)));
    }

    void performance_signalDelivery_doesNotBlockStateMachine()
    {
        ShellStateController state;

        // Wire up a heavy slot to verify signal delivery doesn't block recompute.
        int callCount = 0;
        QObject::connect(&state, &ShellStateController::anyOverlayVisibleChanged,
                         [&callCount](bool) { ++callCount; });

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < 500; ++i) {
            state.setLaunchpadVisible(true);
            state.dismissAllOverlays();
        }

        const qint64 ms = timer.elapsed();
        QVERIFY2(ms < 500, qPrintable(QString("500 toggle-pairs took %1 ms — too slow").arg(ms)));
        QVERIFY(callCount > 0);
    }
};

QTEST_MAIN(ShellContractTest)
#include "shell_contract_test.moc"
