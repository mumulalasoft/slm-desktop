import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    property var notificationManager: null
    property int maxVisible: 3
    property int autoDismissMs: 6200
    property bool doNotDisturb: false

    signal dismissRequested(int notificationId)
    signal notificationClicked(int notificationId)

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    width: 392
    implicitHeight: listView.contentHeight

    ListView {
        id: listView
        anchors.fill: parent
        spacing: 8
        clip: true
        interactive: false
        model: root.notificationManager ? root.notificationManager.bannerNotifications : null

        add: Transition {
            enabled: root.microAnimationAllowed()
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: Theme.notificationBannerEnterDuration
                    easing.type: Theme.easingDefault
                }
                NumberAnimation {
                    property: "x"
                    from: Theme.notificationBannerEntryOffset
                    to: 0
                    duration: Theme.notificationBannerEnterDuration
                    easing.type: Theme.easingDecelerate
                }
            }
        }

        remove: Transition {
            enabled: root.microAnimationAllowed()
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: Theme.notificationBannerExitDuration
                    easing.type: Theme.easingDefault
                }
                NumberAnimation {
                    property: "scale"
                    from: 1
                    to: Theme.notificationBannerExitScale
                    duration: Theme.notificationBannerExitDuration
                    easing.type: Theme.easingDefault
                }
            }
        }

        delegate: Item {
            id: rowItem

            required property int index
            required property int notificationId
            required property string appName
            required property string appIcon
            required property string summary
            required property string body
            required property string priority
            required property bool banner

            property bool hovered: false
            property bool pendingDismiss: false
            readonly property bool sticky: String(priority || "").toLowerCase() === "high"

            width: listView.width
            height: visible ? card.implicitHeight : 0
            visible: index < root.maxVisible && !!banner && !pendingDismiss
                     && (!root.doNotDisturb || sticky)
            opacity: pendingDismiss ? 0 : 1
            scale: pendingDismiss ? Theme.notificationBannerExitScale : 1.0

            Behavior on opacity {
                enabled: root.microAnimationAllowed()
                NumberAnimation { duration: Theme.notificationBannerExitDuration; easing.type: Theme.easingDefault }
            }
            Behavior on scale {
                enabled: root.microAnimationAllowed()
                NumberAnimation { duration: Theme.notificationBannerExitDuration; easing.type: Theme.easingDefault }
            }

            Timer {
                id: dismissTimer
                interval: root.autoDismissMs
                repeat: false
                running: rowItem.visible && !rowItem.sticky && !rowItem.hovered
                onTriggered: rowItem.startDismiss()
            }

            function startDismiss() {
                if (pendingDismiss) {
                    return
                }
                pendingDismiss = true
                dismissFinalize.restart()
            }

            Timer {
                id: dismissFinalize
                interval: root.microAnimationAllowed() ? Theme.notificationBannerExitDuration : 1
                repeat: false
                onTriggered: root.dismissRequested(rowItem.notificationId)
            }

            NotificationCard {
                id: card
                anchors.fill: parent
                compact: true
                appName: rowItem.appName
                appIcon: rowItem.appIcon
                notificationId: rowItem.notificationId
                title: rowItem.summary.length > 0 ? rowItem.summary : rowItem.appName
                body: rowItem.body
                priority: rowItem.priority
                onClicked: root.notificationClicked(rowItem.notificationId)

                Behavior on x {
                    enabled: root.microAnimationAllowed()
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                }
            }

            HoverHandler {
                onHoveredChanged: rowItem.hovered = hovered
            }

            DragHandler {
                id: dragHandler
                target: card
                xAxis.enabled: true
                yAxis.enabled: false
                onActiveChanged: {
                    if (active) {
                        dismissTimer.stop()
                    } else {
                        if (Math.abs(card.x) > rowItem.width * 0.32) {
                            rowItem.startDismiss()
                        } else {
                            card.x = 0
                        }
                    }
                }
            }

        }
    }
}
