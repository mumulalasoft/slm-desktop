// docs/APPDECK.md §18 acceptance contract.
// Each test below maps to one of the ten acceptance points the spec lists.
// These assertions are static (file-text based) — they confirm the v2 wiring
// stays in place across commits. Visual / behavioural validation (icons
// actually fly, no teleport, debug overlay shows monotone progress) happens
// in QEMU as part of /scripts/dev/.
#include <QtTest/QtTest>
#include <QFile>
#include <QRegularExpression>

namespace {
QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}
}

class AppDeckV2AcceptanceContractTest : public QObject
{
    Q_OBJECT
private slots:
    // §18.1 — dock→grid morph happens on the existing surface; no new Window
    void singleSurfaceForGrid()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY2(!win.isEmpty(), "AppDeckWindow.qml unreadable");
        const QRegularExpression rx(QStringLiteral("^Window\\s*\\{"),
                                    QRegularExpression::MultilineOption);
        int windowCount = 0;
        auto it = rx.globalMatch(win);
        while (it.hasNext()) { it.next(); ++windowCount; }
        QCOMPARE(windowCount, 1);
    }

    // §18.2 — single delegate per app (IconMorphLayer + iconsRenderedExternally hooks)
    void singleDelegatePerApp()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString iml = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/IconMorphLayer.qml"));
        QVERIFY(iml.contains(QStringLiteral("AppDeckIconDelegate")));
        // line wraps in source; check the call shape liberally
        QVERIFY(iml.contains(QStringLiteral("pointLerp(dockAnchor,")));
        QVERIFY(iml.contains(QStringLiteral("gridAnchor,")));
        QVERIFY(iml.contains(QStringLiteral("iconLayer.morphProgress")));
        const QString dock = readTextFile(base + QStringLiteral("/Qml/components/appdeck/AppDeck.qml"));
        QVERIFY(dock.contains(QStringLiteral("property bool iconsRenderedExternally")));
        const QString grid = readTextFile(base + QStringLiteral("/Qml/components/appdeck/AppDeckGridView.qml"));
        QVERIFY(grid.contains(QStringLiteral("property bool iconsRenderedExternally")));
    }

    // §18.3 + §18.4 — morphProgress is reversible from a single NumberAnimation
    void morphProgressIsReversible()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString r = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckRoot.qml"));
        QVERIFY(r.contains(QStringLiteral("NumberAnimation")));
        QVERIFY(r.contains(QStringLiteral("property: \"morphProgress\"")));
        QVERIFY(r.contains(QStringLiteral("easing.type: Theme.easingDecelerate")));
        const QString m = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckMotion.qml"));
        QVERIFY(m.contains(QStringLiteral("anim.to = 0.0")));
        QVERIFY(m.contains(QStringLiteral("anim.to = 1.0")));
        // P0 — Motion exposes toggle + interrupt entry points and the inner
        // morphAnim retargets cleanly mid-flight (no transitioning early-return).
        QVERIFY(m.contains(QStringLiteral("function toggleAppDeck(")));
        QVERIFY(m.contains(QStringLiteral("function interruptOrReverseTransition()")));
        QVERIFY(m.contains(QStringLiteral("signal toggleRequested(")));
        QVERIFY(m.contains(QStringLiteral("signal reverseRequested(")));
        QVERIFY(m.contains(QStringLiteral("anim.stop()")));
    }

    // P0 — interrupting during a morph reverses direction instead of being
    // dropped. AppDeckWindow's onAppdeckRequested checks `root.transitioning`
    // and picks enterGrid / enterDock based on `root.surfaceTransition`.
    void transitionInterruptIsHandled()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY2(!win.isEmpty(), "AppDeckWindow.qml unreadable");
        QVERIFY(win.contains(QStringLiteral("if (root.transitioning)")));
        QVERIFY(win.contains(QStringLiteral("var reverseTarget = root.surfaceTransition >= 0.5")));
        QVERIFY(win.contains(QStringLiteral("[APPDECK-LAUNCHER-SIGNAL] interrupt reverse target=")));
        // AppDeckMotion's spec-API signals are dispatched into the legacy
        // enterGrid / enterDock entry points.
        QVERIFY(win.contains(QStringLiteral("target: appDeckMotion")));
        QVERIFY(win.contains(QStringLiteral("function onToggleRequested(")));
        QVERIFY(win.contains(QStringLiteral("function onReverseRequested(")));
    }

    // P0 — watchdog prevents `transitioning` from latching true if the morph
    // ends without onStopped firing. Two layers: AppDeckRoot.transitionWatchdog
    // (v2 SSOT) and AppDeckWindow.transitionLockTimer (legacy safety net,
    // restarted on every transitioning rising edge via onTransitioningChanged).
    void transitionWatchdogPreventsLatch()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString r = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckRoot.qml"));
        QVERIFY(r.contains(QStringLiteral("id: transitionWatchdog")));
        QVERIFY(r.contains(QStringLiteral("signal watchdogReset()")));
        QVERIFY(r.contains(QStringLiteral("signal hoverRecomputeRequested()")));
        QVERIFY(r.contains(QStringLiteral("onTransitioningChanged:")));
        QVERIFY(r.contains(QStringLiteral("transitionWatchdog.restart()")));
        QVERIFY(r.contains(QStringLiteral("transitionWatchdog.stop()")));

        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY(win.contains(QStringLiteral("id: transitionLockTimer")));
        QVERIFY(win.contains(QStringLiteral("interval: root.morphDuration + 250")));
        QVERIFY(win.contains(QStringLiteral("onTransitioningChanged:")));
        QVERIFY(win.contains(QStringLiteral("transitionLockTimer.restart()")));
        // The v2 watchdogReset signal is wired up so a stuck `transitioning`
        // gets cleared even if the legacy timer paths fail.
        QVERIFY(win.contains(QStringLiteral("function onWatchdogReset()")));
    }

    // P0 — overlay layers (pulse / context / staged grid content) must not
    // accept input while they're below ~5% opacity, otherwise their child
    // MouseAreas silently dismiss the grid mid-fade.
    void overlayLayersGateInputOnOpacity()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString pulse = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/PulseLayer.qml"));
        QVERIFY(pulse.contains(QStringLiteral("enabled: visible && opacity > 0.05")));
        const QString ctx = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/ContextLayer.qml"));
        QVERIFY(ctx.contains(QStringLiteral("enabled: visible && opacity > 0.05")));
        const QString grid = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/GridContentLayer.qml"));
        QVERIFY(grid.contains(QStringLiteral("enabled: visible && opacity > 0.05")));
        // Outside-dismiss MouseArea in AppDeckGridAppsView must wait until the
        // grid is fully settled so mid-morph events don't trip dismissal.
        const QString gridView = readTextFile(base + QStringLiteral("/Qml/components/appdeck/AppDeckGridAppsView.qml"));
        QVERIFY(gridView.contains(QStringLiteral("enabled: root.morphProgress >= 0.95")));
    }

    // P0 — hover state is recomputed after every morph end so a stationary
    // pointer still resolves to the correct icon. AppDeckRoot.hoverRecompute-
    // Requested fires from onTransitioningChanged; the v2 controller (wired
    // via AppDeckWindow.qml's Connections block) re-hit-tests the cached
    // pointer X and pushes the resolved id into the global singleton.
    void hoverIsRecomputedAfterMorph()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString c = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckController.qml"));
        QVERIFY(c.contains(QStringLiteral("property int hoveredIconIndex")));
        QVERIFY(c.contains(QStringLiteral("function recomputeHover()")));
        // Hit-test consults both anchor maps depending on deckState.
        QVERIFY(c.contains(QStringLiteral("gridAnchorsByIndex")));
        QVERIFY(c.contains(QStringLiteral("dockAnchorsById")));
        // Resolved id flows back into the global hover singleton.
        QVERIFY(c.contains(QStringLiteral("AppDeckController.onHover(")));

        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY(win.contains(QStringLiteral("target: appDeckRoot")));
        QVERIFY(win.contains(QStringLiteral("function onHoverRecomputeRequested()")));
        QVERIFY(win.contains(QStringLiteral("appDeckController.recomputeHover()")));
    }

    // P0 — debug overlay surfaces the live hover state so QA can confirm a
    // stationary pointer resolves to the right icon after a morph.
    void debugOverlayShowsHoverState()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString d = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckDebugOverlay.qml"));
        QVERIFY(d.contains(QStringLiteral("hoveredIdx:")));
        QVERIFY(d.contains(QStringLiteral("hoveredItemId:")));
        QVERIFY(d.contains(QStringLiteral("hoverX:")));
        QVERIFY(d.contains(QStringLiteral("property var controller")));
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY(win.contains(QStringLiteral("controller:  appDeckController")));
    }

    // §18.5 — pulse is a mode, not a Window
    void pulseIsModeNotWindow()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString p = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/PulseLayer.qml"));
        QVERIFY(!p.contains(QStringLiteral("Window {")));
        QVERIFY(p.contains(QStringLiteral("deckMode === \"pulse\"")));
    }

    // §18.6 — context is a mode, not a Window
    void contextIsModeNotWindow()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString c = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/ContextLayer.qml"));
        QVERIFY(!c.contains(QStringLiteral("Window {")));
        QVERIFY(c.contains(QStringLiteral("deckMode === \"context\"")));
    }

    // §18.7 — outside click collapses (via compositor event bus)
    // §22 — Two-stage dismiss: outside click routes through
    // dismissCurrentLayer which peels pulse → grid first, then grid → dock
    // on a subsequent click. The legacy dismissGridByPointer is still the
    // second-stage collapser, just no longer the first-touch entry point.
    void outsideClickCollapses()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString h = readTextFile(base + QStringLiteral("/src/core/wayland/layershell/appdeckcompositorevents.h"));
        QVERIFY(h.contains(QStringLiteral("void outsidePointerPressed(")));
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY(win.contains(QStringLiteral("dismissCurrentLayer(\"outside-click\")")));
        QVERIFY(win.contains(QStringLiteral("function dismissCurrentLayer(reason)")));
        QVERIFY(win.contains(QStringLiteral("function dismissGridByPointer(reason)")));
    }

    // §18.8 — launching an app collapses without waiting for the window
    void launchAppCollapses()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY(win.contains(QStringLiteral("collapseToDock(\"dock-app-activated\")")));
    }

    // §18.9 — auto-hide keeps reveal zone (never visible:false)
    void autoHideKeepsRevealZone()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString r = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckRoot.qml"));
        QVERIFY(r.contains(QStringLiteral("property real dockPresence")));
        QVERIFY(r.contains(QStringLiteral("Behavior on dockPresence")));
        const QString t = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckTokens.qml"));
        QVERIFY(t.contains(QStringLiteral("autoHideMinPresence")));
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY(!win.contains(QStringLiteral("dockView.visible = false")));
    }

    // §18.10 — debug overlay shows morphProgress
    void debugOverlayHasShortcutAndShowsMorph()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString d = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckDebugOverlay.qml"));
        QVERIFY2(!d.isEmpty(), "AppDeckDebugOverlay.qml unreadable");
        QVERIFY(d.contains(QStringLiteral("sequence: \"Ctrl+Alt+D\"")));
        QVERIFY(d.contains(QStringLiteral("context: Qt.ApplicationShortcut")));
        QVERIFY(d.contains(QStringLiteral("morphProgress:")));
        QVERIFY(d.contains(QStringLiteral("dockRect:")));
        QVERIFY(d.contains(QStringLiteral("gridRect:")));
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        QVERIFY(win.contains(QStringLiteral("AppDeckV2.AppDeckDebugOverlay")));
    }

    // Cross-cut: spec §12 freeze/unfreeze anchors plumbed around morph
    void freezeAnchorsAroundMorph()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString r = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckRoot.qml"));
        QVERIFY(r.contains(QStringLiteral("freezeAnchors()")));
        QVERIFY(r.contains(QStringLiteral("unfreezeAnchors()")));
        const QString g = readTextFile(base + QStringLiteral("/Qml/components/appdeck/v2/AppDeckGeometry.qml"));
        QVERIFY(g.contains(QStringLiteral("function freezeAnchors")));
        QVERIFY(g.contains(QStringLiteral("function unfreezeAnchors")));
    }
};

QTEST_MAIN(AppDeckV2AcceptanceContractTest)
#include "appdeckv2_acceptance_contract_test.moc"
