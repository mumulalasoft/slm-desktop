import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import "../power" as PowerComp

Window {
    id: root

    required property var rootWindow
    property bool overlayActive: typeof PowerController !== "undefined"
                                 && PowerController
                                 && PowerController.overlayVisible
    readonly property bool scheduleBannerActive: !root.overlayActive
                                                 && typeof ScheduleController !== "undefined"
                                                 && ScheduleController
                                                 && ScheduleController.hasPending
    readonly property int bannerWindowWidth: Math.min(560, Math.max(360, Screen.width - 32))
    readonly property int bannerWindowHeight: 58

    title: "SLM TopOverlay Power"
    visible: root.overlayActive || root.scheduleBannerActive
    visibility: root.overlayActive ? Window.FullScreen : Window.Windowed
    width: root.overlayActive ? Screen.width : root.bannerWindowWidth
    height: root.overlayActive ? Screen.height : root.bannerWindowHeight
    x: root.overlayActive ? 0 : Math.round((Screen.width - width) / 2)
    y: root.overlayActive ? 0 : 18
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"

    Item {
        id: overlayRoot
        anchors.fill: parent
        z: 1000
        property real overlayProgress: root.overlayActive ? 1.0 : 0.0

        Rectangle {
            anchors.fill: parent
            visible: root.overlayActive
            color: Qt.rgba(0, 0, 0, 0.42 * overlayRoot.overlayProgress)
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                onClicked: PowerController.cancel()
            }
        }

        Behavior on overlayProgress {
            NumberAnimation {
                duration: MotionController.preset("enter").duration
                easing.type: MotionController.preset("enter").easing
            }
        }

        FocusScope {
            id: dialogHost
            anchors.fill: parent
            visible: root.overlayActive
            focus: root.overlayActive
            Keys.onEscapePressed: PowerController.cancel()

            PowerComp.PowerDialog {
                anchors.fill: parent
                visible: PowerController.powerDialogVisible
                opacity: visible ? 1 : 0
                scale: visible ? 1.0 : 0.96
                action: PowerController.pendingAction.length > 0 ? PowerController.pendingAction : "shutdown"
                busy: PowerController.busy
                onCancelRequested: PowerController.cancel()
                onActionRequested: function(action) { PowerController.requestNow(action) }
                onScheduleRequested: function(action) { PowerController.openScheduledDialog(action) }
                onScheduleAtRequested: function(action, executeAt) { PowerController.scheduleAt(action, executeAt) }
                onScheduleInRequested: function(action, minutes) { PowerController.scheduleInMinutes(action, minutes) }
            }

            PowerComp.ScheduledShutdownDialog {
                anchors.fill: parent
                visible: PowerController.scheduledDialogVisible
                action: PowerController.pendingAction.length > 0 ? PowerController.pendingAction : "shutdown"
                onCancelRequested: PowerController.cancel()
                onExecuteNowRequested: function(action) { PowerController.requestNow(action) }
                onScheduleInRequested: function(action, minutes) { PowerController.scheduleInMinutes(action, minutes) }
                onScheduleAtRequested: function(action, executeAt) { PowerController.scheduleAt(action, executeAt) }
            }

            PowerComp.PowerSafetyDialog {
                anchors.fill: parent
                visible: PowerController.safetyDialogVisible
                action: PowerController.pendingAction
                apps: PowerController.remainingApps
                busy: PowerController.busy
                onCancelRequested: PowerController.cancel()
                onReviewRequested: PowerController.reviewApps()
                onForceRequested: PowerController.forceExecute()
            }

            PowerComp.PowerSafetyDialog {
                anchors.fill: parent
                visible: PowerController.blockingWarningVisible
                action: PowerController.pendingAction
                apps: []
                busy: PowerController.busy
                titleText: PowerController.pendingAction === "restart"
                           ? qsTr("SLM will restart in 30 seconds.")
                           : qsTr("SLM will shut down in 30 seconds.")
                descriptionText: qsTr("You can cancel the scheduled action or run it now.")
                onCancelRequested: {
                    ScheduleController.cancelSchedule()
                    PowerController.cancel()
                }
                onReviewRequested: PowerController.cancel()
                onForceRequested: PowerController.executeScheduledNow()
            }
        }

        PowerComp.PowerCountdownBanner {
            anchors.centerIn: parent
            visible: root.scheduleBannerActive
            text: visible ? ScheduleController.countdownText : ""
            onCancelRequested: ScheduleController.cancelSchedule()
            onEditRequested: PowerController.openScheduledDialog(ScheduleController.action || "shutdown")
            onExecuteNowRequested: PowerController.executeScheduledNow()
        }
    }
}
