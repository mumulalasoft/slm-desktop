#include <QtTest/QtTest>
#include "../src/core/shell/shellstatecontroller.h"

class ShellStateControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultState_allOverlaysHidden()
    {
        ShellStateController ctrl;
        QVERIFY(!ctrl.appdeckVisible());
        QVERIFY(!ctrl.workspaceOverviewVisible());
        QVERIFY(!ctrl.toTheSpotVisible());
        QVERIFY(!ctrl.styleGalleryVisible());
        QVERIFY(!ctrl.showDesktop());
        QVERIFY(!ctrl.anyOverlayVisible());
    }

    void defaultDerivedState_fullOpacity()
    {
        ShellStateController ctrl;
        QCOMPARE(ctrl.topBarOpacity(), 1.0);
        QCOMPARE(ctrl.dockOpacity(), 1.0);
        QVERIFY(!ctrl.workspaceBlurred());
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.0);
        QVERIFY(!ctrl.workspaceInteractionBlocked());
    }

    void setAppDeckVisible_updatesDerivedState()
    {
        ShellStateController ctrl;
        QSignalSpy topBarSpy(&ctrl, &ShellStateController::topBarOpacityChanged);
        QSignalSpy dockSpy(&ctrl, &ShellStateController::dockOpacityChanged);
        QSignalSpy blurSpy(&ctrl, &ShellStateController::workspaceBlurredChanged);
        QSignalSpy blockSpy(&ctrl, &ShellStateController::workspaceInteractionBlockedChanged);
        QSignalSpy overlaySpy(&ctrl, &ShellStateController::anyOverlayVisibleChanged);

        ctrl.setAppDeckVisible(true);

        QVERIFY(ctrl.appdeckVisible());
        QVERIFY(ctrl.anyOverlayVisible());
        QCOMPARE(ctrl.topBarOpacity(), 1.0);
        QCOMPARE(ctrl.dockOpacity(), 1.0);
        QVERIFY(ctrl.workspaceBlurred());
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.50);
        QVERIFY(ctrl.workspaceInteractionBlocked());

        QCOMPARE(topBarSpy.count(), 0);
        QCOMPARE(dockSpy.count(), 0);
        QCOMPARE(blurSpy.count(), 1);
        QCOMPARE(blockSpy.count(), 1);
        QCOMPARE(overlaySpy.count(), 1);
    }

    void setAppDeckVisible_false_restoresDerivedState()
    {
        ShellStateController ctrl;
        ctrl.setAppDeckVisible(true);
        ctrl.setAppDeckVisible(false);

        QVERIFY(!ctrl.appdeckVisible());
        QCOMPARE(ctrl.topBarOpacity(), 1.0);
        QCOMPARE(ctrl.dockOpacity(), 1.0);
        QVERIFY(!ctrl.workspaceBlurred());
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.0);
        QVERIFY(!ctrl.workspaceInteractionBlocked());
        QVERIFY(!ctrl.anyOverlayVisible());
    }

    void showDesktop_hidesDocAndBlursWorkspace()
    {
        ShellStateController ctrl;
        ctrl.setShowDesktop(true);

        QCOMPARE(ctrl.dockOpacity(), 0.0);
        QVERIFY(ctrl.workspaceBlurred());
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.40);
        // Crown is unaffected by show-desktop
        QCOMPARE(ctrl.topBarOpacity(), 1.0);
        // Workspace interaction is not blocked in show-desktop (only in appdeck)
        QVERIFY(!ctrl.workspaceInteractionBlocked());
        // show-desktop is not an overlay
        QVERIFY(!ctrl.anyOverlayVisible());
    }

    void setAppDeck_noSignalOnNoop()
    {
        ShellStateController ctrl;
        ctrl.setAppDeckVisible(true);

        QSignalSpy spy(&ctrl, &ShellStateController::appdeckVisibleChanged);
        ctrl.setAppDeckVisible(true); // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void toggleAppDeck_flipsState()
    {
        ShellStateController ctrl;
        QVERIFY(!ctrl.appdeckVisible());
        ctrl.toggleAppDeck();
        QVERIFY(ctrl.appdeckVisible());
        ctrl.toggleAppDeck();
        QVERIFY(!ctrl.appdeckVisible());
    }

    void dismissAllOverlays_clearsEverything()
    {
        ShellStateController ctrl;
        ctrl.setAppDeckVisible(true);
        ctrl.setWorkspaceOverviewVisible(true);
        ctrl.setPulseVisible(true);
        ctrl.setStyleGalleryVisible(true);

        ctrl.dismissAllOverlays();

        QVERIFY(!ctrl.appdeckVisible());
        QVERIFY(!ctrl.workspaceOverviewVisible());
        QVERIFY(!ctrl.toTheSpotVisible());
        QVERIFY(!ctrl.styleGalleryVisible());
        QVERIFY(!ctrl.anyOverlayVisible());
    }

    void anyOverlayVisible_trueForEachOverlay()
    {
        {
            ShellStateController ctrl;
            ctrl.setWorkspaceOverviewVisible(true);
            QVERIFY(ctrl.anyOverlayVisible());
        }
        {
            ShellStateController ctrl;
            ctrl.setPulseVisible(true);
            QVERIFY(ctrl.anyOverlayVisible());
        }
        {
            ShellStateController ctrl;
            ctrl.setStyleGalleryVisible(true);
            QVERIFY(ctrl.anyOverlayVisible());
        }
    }

    void appdeck_overridesShowDesktop_forDockOpacity()
    {
        // Both showDesktop and appdeck active — dock stays hidden
        ShellStateController ctrl;
        ctrl.setShowDesktop(true);
        ctrl.setAppDeckVisible(true);
        QCOMPARE(ctrl.dockOpacity(), 0.0);
    }

    void appdeckBlurAlpha_precedesShowDesktop()
    {
        ShellStateController ctrl;
        ctrl.setShowDesktop(true);
        ctrl.setAppDeckVisible(true);
        // AppDeck has higher blur intensity than show-desktop
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.50);
    }

    void searchVisible_aliasesPulseVisibility()
    {
        ShellStateController ctrl;
        QSignalSpy searchSpy(&ctrl, &ShellStateController::searchVisibleChanged);
        QSignalSpy toTheSpotSpy(&ctrl, &ShellStateController::toTheSpotVisibleChanged);

        ctrl.setPulseVisible(true);
        QVERIFY(ctrl.toTheSpotVisible());
        QCOMPARE(searchSpy.count(), 1);
        QCOMPARE(toTheSpotSpy.count(), 1);

        ctrl.setPulseVisible(false);
        QVERIFY(!ctrl.toTheSpotVisible());
        QCOMPARE(searchSpy.count(), 2);
        QCOMPARE(toTheSpotSpy.count(), 2);
    }

    void searchQuery_roundTrips()
    {
        ShellStateController ctrl;
        QSignalSpy spy(&ctrl, &ShellStateController::searchQueryChanged);

        ctrl.setSearchQuery(QStringLiteral("firewall"));
        QCOMPARE(ctrl.searchQuery(), QStringLiteral("firewall"));
        QCOMPARE(spy.count(), 1);

        ctrl.setSearchQuery(QStringLiteral("firewall"));
        QCOMPARE(spy.count(), 1);

        ctrl.setSearchQuery(QStringLiteral(""));
        QCOMPARE(ctrl.searchQuery(), QStringLiteral(""));
        QCOMPARE(spy.count(), 2);
    }

    void appDeckSearchSeed_roundTrips()
    {
        ShellStateController ctrl;
        QSignalSpy spy(&ctrl, &ShellStateController::appDeckSearchSeedChanged);

        ctrl.setAppDeckSearchSeed(QStringLiteral("utilities"));
        QCOMPARE(ctrl.appDeckSearchSeed(), QStringLiteral("utilities"));
        QCOMPARE(spy.count(), 1);

        ctrl.setAppDeckSearchSeed(QStringLiteral("utilities"));
        QCOMPARE(spy.count(), 1);

        ctrl.setAppDeckSearchSeed(QStringLiteral(""));
        QCOMPARE(ctrl.appDeckSearchSeed(), QStringLiteral(""));
        QCOMPARE(spy.count(), 2);
    }

    void dockHoveredItem_roundTrips()
    {
        ShellStateController ctrl;
        QSignalSpy spy(&ctrl, &ShellStateController::dockHoveredItemChanged);

        ctrl.setDockHoveredItem(QStringLiteral("appdeck"));
        QCOMPARE(ctrl.dockHoveredItem(), QStringLiteral("appdeck"));
        QCOMPARE(spy.count(), 1);

        ctrl.setDockHoveredItem(QStringLiteral("appdeck"));
        QCOMPARE(spy.count(), 1);

        ctrl.setDockHoveredItem(QStringLiteral(""));
        QCOMPARE(ctrl.dockHoveredItem(), QStringLiteral(""));
        QCOMPARE(spy.count(), 2);
    }

    void dockExpandedItem_roundTrips()
    {
        ShellStateController ctrl;
        QSignalSpy spy(&ctrl, &ShellStateController::dockExpandedItemChanged);

        ctrl.setDockExpandedItem(QStringLiteral("org.example.App.desktop"));
        QCOMPARE(ctrl.dockExpandedItem(), QStringLiteral("org.example.App.desktop"));
        QCOMPARE(spy.count(), 1);

        ctrl.setDockExpandedItem(QStringLiteral("org.example.App.desktop"));
        QCOMPARE(spy.count(), 1);

        ctrl.setDockExpandedItem(QStringLiteral(""));
        QCOMPARE(ctrl.dockExpandedItem(), QStringLiteral(""));
        QCOMPARE(spy.count(), 2);
    }

    void dragSession_roundTripsAndClears()
    {
        ShellStateController ctrl;
        QSignalSpy spy(&ctrl, &ShellStateController::dragSessionChanged);

        QVariantMap session{
            {QStringLiteral("source"), QStringLiteral("appdeck")},
            {QStringLiteral("item_id"), QStringLiteral("org.example.App.desktop")},
            {QStringLiteral("active"), true},
            {QStringLiteral("position"), 12.0},
        };
        ctrl.setDragSession(session);
        QCOMPARE(ctrl.dragSession().value(QStringLiteral("source")).toString(),
                 QStringLiteral("appdeck"));
        QCOMPARE(ctrl.dragSession().value(QStringLiteral("item_id")).toString(),
                 QStringLiteral("org.example.App.desktop"));
        QCOMPARE(spy.count(), 1);

        ctrl.setDragSession(session);
        QCOMPARE(spy.count(), 1);

        ctrl.clearDragSession();
        QVERIFY(ctrl.dragSession().isEmpty());
        QCOMPARE(spy.count(), 2);
    }
};

QTEST_MAIN(ShellStateControllerTest)
#include "shell_state_controller_test.moc"
