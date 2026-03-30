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
        QCOMPARE(ctrl.topBarOpacity(), 0.72);
        QCOMPARE(ctrl.dockOpacity(), 0.0);
        QVERIFY(ctrl.workspaceBlurred());
        QCOMPARE(ctrl.workspaceBlurAlpha(), 0.50);
        QVERIFY(ctrl.workspaceInteractionBlocked());

        QCOMPARE(topBarSpy.count(), 1);
        QCOMPARE(dockSpy.count(), 1);
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
};

QTEST_MAIN(ShellStateControllerTest)
#include "shell_state_controller_test.moc"
