#include <QtTest/QtTest>
#include "../src/core/shell/shellstatecontroller.h"

class ShellStateControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultState_allOverlaysHidden()
    {
        ShellStateController ctrl;
        QVERIFY(!ctrl.launchpadVisible());
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

    void setLaunchpadVisible_updatesDerivedState()
    {
        ShellStateController ctrl;
        QSignalSpy topBarSpy(&ctrl, &ShellStateController::topBarOpacityChanged);
        QSignalSpy dockSpy(&ctrl, &ShellStateController::dockOpacityChanged);
        QSignalSpy blurSpy(&ctrl, &ShellStateController::workspaceBlurredChanged);
        QSignalSpy blockSpy(&ctrl, &ShellStateController::workspaceInteractionBlockedChanged);
        QSignalSpy overlaySpy(&ctrl, &ShellStateController::anyOverlayVisibleChanged);

        ctrl.setLaunchpadVisible(true);

        QVERIFY(ctrl.launchpadVisible());
        QVERIFY(ctrl.anyOverlayVisible());
        QCOMPARE(ctrl.topBarOpacity(), 0.0);
        QCOMPARE(ctrl.dockOpacity(), 1.0);
        QVERIFY(ctrl.workspaceBlurred());
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.50);
        QVERIFY(ctrl.workspaceInteractionBlocked());

        QCOMPARE(topBarSpy.count(), 1);
        QCOMPARE(dockSpy.count(), 0);
        QCOMPARE(blurSpy.count(), 1);
        QCOMPARE(blockSpy.count(), 1);
        QCOMPARE(overlaySpy.count(), 1);
    }

    void setLaunchpadVisible_false_restoresDerivedState()
    {
        ShellStateController ctrl;
        ctrl.setLaunchpadVisible(true);
        ctrl.setLaunchpadVisible(false);

        QVERIFY(!ctrl.launchpadVisible());
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
        // TopBar is unaffected by show-desktop
        QCOMPARE(ctrl.topBarOpacity(), 1.0);
        // Workspace interaction is not blocked in show-desktop (only in launchpad)
        QVERIFY(!ctrl.workspaceInteractionBlocked());
        // show-desktop is not an overlay
        QVERIFY(!ctrl.anyOverlayVisible());
    }

    void setLaunchpad_noSignalOnNoop()
    {
        ShellStateController ctrl;
        ctrl.setLaunchpadVisible(true);

        QSignalSpy spy(&ctrl, &ShellStateController::launchpadVisibleChanged);
        ctrl.setLaunchpadVisible(true); // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void toggleLaunchpad_flipsState()
    {
        ShellStateController ctrl;
        QVERIFY(!ctrl.launchpadVisible());
        ctrl.toggleLaunchpad();
        QVERIFY(ctrl.launchpadVisible());
        ctrl.toggleLaunchpad();
        QVERIFY(!ctrl.launchpadVisible());
    }

    void dismissAllOverlays_clearsEverything()
    {
        ShellStateController ctrl;
        ctrl.setLaunchpadVisible(true);
        ctrl.setWorkspaceOverviewVisible(true);
        ctrl.setToTheSpotVisible(true);
        ctrl.setStyleGalleryVisible(true);

        ctrl.dismissAllOverlays();

        QVERIFY(!ctrl.launchpadVisible());
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
            ctrl.setToTheSpotVisible(true);
            QVERIFY(ctrl.anyOverlayVisible());
        }
        {
            ShellStateController ctrl;
            ctrl.setStyleGalleryVisible(true);
            QVERIFY(ctrl.anyOverlayVisible());
        }
    }

    void launchpad_overridesShowDesktop_forDockOpacity()
    {
        // Both showDesktop and launchpad active — dock stays hidden
        ShellStateController ctrl;
        ctrl.setShowDesktop(true);
        ctrl.setLaunchpadVisible(true);
        QCOMPARE(ctrl.dockOpacity(), 0.0);
    }

    void launchpadBlurAlpha_precedesShowDesktop()
    {
        ShellStateController ctrl;
        ctrl.setShowDesktop(true);
        ctrl.setLaunchpadVisible(true);
        // Launchpad has higher blur intensity than show-desktop
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.50);
    }

    void searchVisible_aliasesToTheSpotVisibility()
    {
        ShellStateController ctrl;
        QSignalSpy searchSpy(&ctrl, &ShellStateController::searchVisibleChanged);
        QSignalSpy toTheSpotSpy(&ctrl, &ShellStateController::toTheSpotVisibleChanged);

        ctrl.setToTheSpotVisible(true);
        QVERIFY(ctrl.toTheSpotVisible());
        QCOMPARE(searchSpy.count(), 1);
        QCOMPARE(toTheSpotSpy.count(), 1);

        ctrl.setToTheSpotVisible(false);
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

    void dockHoveredItem_roundTrips()
    {
        ShellStateController ctrl;
        QSignalSpy spy(&ctrl, &ShellStateController::dockHoveredItemChanged);

        ctrl.setDockHoveredItem(QStringLiteral("launchpad"));
        QCOMPARE(ctrl.dockHoveredItem(), QStringLiteral("launchpad"));
        QCOMPARE(spy.count(), 1);

        ctrl.setDockHoveredItem(QStringLiteral("launchpad"));
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
            {QStringLiteral("source"), QStringLiteral("dock")},
            {QStringLiteral("item_id"), QStringLiteral("org.example.App.desktop")},
            {QStringLiteral("active"), true},
            {QStringLiteral("position"), 12.0},
        };
        ctrl.setDragSession(session);
        QCOMPARE(ctrl.dragSession().value(QStringLiteral("source")).toString(),
                 QStringLiteral("dock"));
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
