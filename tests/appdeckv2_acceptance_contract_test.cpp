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
    void outsideClickCollapses()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString h = readTextFile(base + QStringLiteral("/src/core/wayland/layershell/appdeckcompositorevents.h"));
        QVERIFY(h.contains(QStringLiteral("void outsidePointerPressed(")));
        const QString win = readTextFile(base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml"));
        // existing handler still wired
        QVERIFY(win.contains(QStringLiteral("dismissGridByPointer(\"outside-click\")")));
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
