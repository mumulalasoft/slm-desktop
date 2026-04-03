import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property string label: ""
    property string iconPath: ""
    property real baseSlotWidth: 58
    property real itemScale: 1.0
    property real influence: 0
    property real hoverLift: 5
    property real liftOffset: 0
    property int animationDuration: 150
    property real gapWidthExtra: 28
    property real gapSpring: Theme.physicsSpringDefault
    property real gapDamping: Theme.physicsDampingDefault
    property real gapMass: Theme.physicsMassDefault
    property real dragSourceOpacity: 0.36
    property int dragThresholdMousePx: 6
    property int dragThresholdTouchpadPx: 3
    property bool showRunningDot: true
    property bool focusedApp: false
    property bool hasOtherWorkspaceDot: false
    property bool showMixedWorkspaceDot: false
    property bool dragEnabled: false
    property bool dragSource: false
    property bool gapTarget: false
    property string gapPreviewIconPath: ""
    property bool reorderArmed: false
    property bool hoverIndicatorEnabled: false
    property bool directHoverOverride: false
    // Notification badge count. 0 = hidden; >0 = show red bubble with count.
    property int badgeCount: 0
    // Separator — shows a slim vertical divider to the right of this item.
    // The delegate width grows by separatorSlotWidth to accommodate it.
    property bool separatorAfter: false
    readonly property real separatorSlotWidth: separatorAfter ? 16 : 0
    property bool hovered: mouseArea.containsMouse || directHoverOverride
    property real hoverBlend: hovered ? 1.0 : 0.0
    property bool dragging: false
    property real dragOffsetX: 0
    property real bounceOffset: 0
    property real iconLift: hovered ? Math.max(0, hoverLift) : 0
    signal clicked()
    signal bounceCompleted()
    signal dragStarted()
    signal dragMoved(real deltaX)
    signal dragFinished(real deltaX)

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    width: baseSlotWidth + (gapTarget ? gapWidthExtra : 0) + separatorSlotWidth
    height: 76
    z: root.dragging ? 200 : 0
    Behavior on width {
        SpringAnimation {
            spring: gapSpring
            damping: gapDamping
            mass: gapMass
        }
    }
    Behavior on hoverBlend {
        enabled: root.microAnimationAllowed()
        NumberAnimation {
            duration: Theme.durationSm
            easing.type: Theme.easingDecelerate
        }
    }
    Behavior on itemScale {
        enabled: root.microAnimationAllowed()
        NumberAnimation {
            duration: Theme.durationMicro
            easing.type: Theme.easingDecelerate
        }
    }
    Behavior on dragOffsetX {
        enabled: !root.dragging
        NumberAnimation {
            duration: Theme.durationMicro
            easing.type: Theme.easingDecelerate
        }
    }

    Item {
        id: visualContainer
        width: parent.width
        height: parent.height
        x: root.dragOffsetX
        y: -root.iconLift

        Behavior on y {
            NumberAnimation {
                duration: root.animationDuration
                easing.type: Theme.easingDecelerate
            }
        }

        Item {
            id: iconPlate
            width: Math.round(52 * root.itemScale)
            height: width
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 7 + root.bounceOffset - (mouseArea.pressed ? 1 : 0)
            opacity: root.gapTarget && !root.dragSource ? 0.0
                    : (root.dragSource && root.dragging ? root.dragSourceOpacity
                                                        : (root.hovered ? 1.0 : 0.92))
            scale: root.hovered ? 1.02 : 1.0

            Image {
                anchors.centerIn: parent
                width: Math.round(parent.width * 0.94)
                height: width
                source: root.iconPath
                fillMode: Image.PreserveAspectFit
            }

            Behavior on width {
                enabled: root.microAnimationAllowed()
                NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
            }
            Behavior on scale {
                enabled: root.microAnimationAllowed()
                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDecelerate }
            }
            Behavior on opacity {
                enabled: root.microAnimationAllowed()
                NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
            }

            // Notification badge bubble — anchored to top-right of iconPlate
            Rectangle {
                id: badgeBubble
                visible: root.badgeCount > 0
                width: Math.max(height, badgeLabel.contentWidth + 8)
                height: Math.round(Theme.fontSize("tiny") * 1.6)
                radius: height * 0.5
                color: Theme.color("error")
                border.width: Theme.borderWidthThick
                border.color: Theme.color("dockBg")
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: -4
                anchors.rightMargin: -4
                z: 10
                scale: root.badgeCount > 0 ? 1.0 : 0.4
                opacity: root.badgeCount > 0 ? 1.0 : 0.0

                Behavior on scale {
                    enabled: root.microAnimationAllowed()
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                }
                Behavior on opacity {
                    enabled: root.microAnimationAllowed()
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDecelerate }
                }

                Label {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: root.badgeCount > 99 ? "99+" : String(root.badgeCount)
                    color: Theme.color("accentText")
                    font.pixelSize: Theme.fontSize("tiny")
                    font.weight: Theme.fontWeight("bold")
                }
            }
        }

        Rectangle {
            visible: root.gapTarget && !root.dragSource
            width: Math.round(52 * (root.itemScale + (0.02 * root.hoverBlend)))
            height: width
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 7
            radius: Math.round(width * 0.26)
            color: Theme.color("dockGapPreviewBg")
            border.width: Theme.borderWidthThick
            border.color: Theme.color("dockBorder")
            opacity: Theme.opacityDockPreview

            Image {
                anchors.centerIn: parent
                width: Math.round(parent.width * 0.62)
                height: width
                source: root.gapPreviewIconPath
                fillMode: Image.PreserveAspectFit
                opacity: Theme.opacityElevated
                visible: root.gapPreviewIconPath.length > 0
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            width: showRunningDot ? (showMixedWorkspaceDot ? 13 : 7) : 0
            height: showRunningDot ? 7 : 0
            radius: height * 0.5
            color: showMixedWorkspaceDot
                   ? "transparent"
                   : (focusedApp ? Theme.color("dockRunningDotActive")
                                 : Theme.color("dockRunningDotInactive"))
            opacity: showRunningDot ? Theme.opacityHint : 0.0
            visible: showRunningDot

            Rectangle {
                visible: root.showMixedWorkspaceDot
                width: 6.5
                height: 6.5
                radius: height * 0.5
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: root.focusedApp ? Theme.color("dockRunningDotActive") : Theme.color("dockRunningDotInactive")
            }

            Rectangle {
                visible: root.showMixedWorkspaceDot
                width: 6.5
                height: 6.5
                radius: height * 0.5
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.color("accent")
                opacity: Theme.opacityGhost
            }

            Rectangle {
                visible: !root.showMixedWorkspaceDot && root.hasOtherWorkspaceDot
                anchors.fill: parent
                radius: height * 0.5
                color: Theme.color("accentSoft")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("accent")
                opacity: Theme.opacityGhost
            }

            Rectangle {
                visible: !root.showMixedWorkspaceDot && root.hasOtherWorkspaceDot
                width: 2.5
                height: 2.5
                radius: height * 0.5
                anchors.centerIn: parent
                color: Theme.color("accent")
                opacity: Theme.opacityGhost
            }
        }

        Label {
            id: tooltip
            anchors.horizontalCenter: iconPlate.horizontalCenter
            anchors.bottom: iconPlate.top
            anchors.bottomMargin: 8
            z: 30
            text: root.label
            color: Theme.color("textOnGlass")
            font.pixelSize: Theme.fontSize("small")
            font.weight: Theme.fontWeight("bold")
            padding: 6
            opacity: (hoverIndicatorEnabled && !root.dragging) ? root.hoverBlend : 0
            visible: opacity > 0.01
            background: Rectangle {
                radius: Theme.radiusControlLarge
                color: Theme.color("dockTooltipBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("dockBorder")
            }

            Behavior on opacity {
                enabled: root.microAnimationAllowed()
                NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
            }
        }

        Rectangle {
            id: ripple
            width: Math.round(iconPlate.width * 0.6)
            height: width
            radius: width / 2
            anchors.centerIn: iconPlate
            color: Qt.rgba(1, 1, 1, 0.33)
            opacity: 0
            scale: 0.4
            visible: opacity > 0.001
        }
    }

    Rectangle {
        id: separatorLine
        visible: root.separatorAfter
        width: 1
        height: 28
        radius: Theme.borderWidthThin
        color: Theme.color("dockBorder")
        opacity: root.separatorAfter ? 0.55 : 0.0
        anchors.right: parent.right
        anchors.rightMargin: Math.floor(root.separatorSlotWidth / 2)
        anchors.verticalCenter: parent.verticalCenter
        Behavior on opacity {
            enabled: root.microAnimationAllowed()
            NumberAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDecelerate }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        property real pressX: 0
        property real pressY: 0
        property real pressGlobalX: 0
        property real lastDx: 0
        property int deltaSamples: 0
        property int fineStepSamples: 0
        property bool suppressNextClick: false

        onPressed: function(mouse) {
            pressX = mouse.x
            pressY = mouse.y
            pressGlobalX = root.mapToItem(null, mouse.x, mouse.y).x
            lastDx = 0
            deltaSamples = 0
            fineStepSamples = 0
            holdToReorderTimer.restart()
        }
        onDoubleClicked: function(mouse) {
            if (!root.dragEnabled) {
                return
            }
            clickConfirmTimer.stop()
            clickPending = false
            root.armReorderMode()
            pressX = mouse.x
            pressY = mouse.y
        }
        onPressAndHold: function(mouse) {
            if (!root.dragEnabled) {
                return
            }
            clickConfirmTimer.stop()
            clickPending = false
            root.armReorderMode()
        }
        onReleased: {
            holdToReorderTimer.stop()
            if (root.dragging) {
                root.dragFinished(root.dragOffsetX)
                root.dragOffsetX = 0
                root.dragging = false
                root.reorderArmed = false
                reorderArmTimeout.stop()
                suppressNextClick = true
            }
        }
        onCanceled: {
            holdToReorderTimer.stop()
            if (root.dragging) {
                root.dragFinished(root.dragOffsetX)
                mouseArea.suppressNextClick = true
            }
            root.dragOffsetX = 0
            root.dragging = false
            root.reorderArmed = false
            reorderArmTimeout.stop()
            clickConfirmTimer.stop()
            clickPending = false
        }
        onClicked: function(mouse) {
            if (suppressNextClick) {
                suppressNextClick = false
                return
            }
            if (!root.dragEnabled) {
                rippleAnim.restart()
                root.clicked()
                return
            }
            if (root.reorderArmed) {
                return
            }

            // Defer single-click action briefly so a second tap can arm reorder.
            if (!clickPending) {
                clickPending = true
                clickConfirmTimer.restart()
                return
            }

            // Second tap detected within window: arm reorder mode.
            clickConfirmTimer.stop()
            clickPending = false
            root.armReorderMode()
        }

        property bool clickPending: false
        Timer {
            id: clickConfirmTimer
            interval: 230
            repeat: false
            onTriggered: {
                if (mouseArea.clickPending && !root.reorderArmed && !root.dragging) {
                    rippleAnim.restart()
                    root.clicked()
                }
                mouseArea.clickPending = false
            }
        }

        Timer {
            id: holdToReorderTimer
            interval: 280
            repeat: false
            onTriggered: {
                if (!mouseArea.pressed || !root.dragEnabled || root.dragging) {
                    return
                }
                clickConfirmTimer.stop()
                mouseArea.clickPending = false
                root.armReorderMode()
            }
        }
        onPositionChanged: function(mouse) {
            if (!root.dragEnabled || !pressed) {
                return
            }

            var currentGlobalX = root.mapToItem(null, mouse.x, mouse.y).x
            var dx = currentGlobalX - pressGlobalX
            var dy = mouse.y - pressY
            var moveDistance = Math.sqrt(dx * dx + dy * dy)
            var step = Math.abs(dx - lastDx)
            if (!root.dragging && step > 0.01 && deltaSamples < 10) {
                deltaSamples += 1
                if (step <= 1.45) {
                    fineStepSamples += 1
                }
            }
            lastDx = dx

            if (!root.dragging) {
                var looksLikeTouchpad = deltaSamples >= 4
                                         && fineStepSamples / Math.max(1, deltaSamples) >= 0.65
                var threshold = looksLikeTouchpad ? root.dragThresholdTouchpadPx : root.dragThresholdMousePx
                if (moveDistance < threshold) {
                    return
                }
                clickConfirmTimer.stop()
                mouseArea.clickPending = false
                root.reorderArmed = true
                root.dragging = true
                root.dragStarted()
            }

            root.dragOffsetX = dx
            root.dragMoved(dx)
        }
    }

    HoverHandler {
        id: iconHover
        acceptedDevices: PointerDevice.Mouse
        enabled: false
    }

    function playBounce(mode) {
        if (mode === "launch") {
            launchBounceAnim.restart()
        } else {
            focusBounceAnim.restart()
        }
    }


    ParallelAnimation {
        id: rippleAnim
        NumberAnimation {
            target: ripple
            property: "scale"
            from: 0.4
            to: 1.5
            duration: Theme.durationSlow
            easing.type: Theme.easingDefault
        }
        NumberAnimation {
            target: ripple
            property: "opacity"
            from: 0.18
            to: 0.0
            duration: Theme.durationSlow
            easing.type: Theme.easingLight
        }
    }

    function armReorderMode() {
        if (!dragEnabled) {
            return
        }
        reorderArmed = true
        reorderArmTimeout.restart()
    }

    Timer {
        id: reorderArmTimeout
        interval: 6000
        repeat: false
        onTriggered: root.reorderArmed = false
    }

    SequentialAnimation {
        id: focusBounceAnim
        NumberAnimation {
            target: root
            property: "bounceOffset"
            to: -8
            duration: Theme.durationMicro
            easing.type: Theme.easingDefault
        }
        NumberAnimation {
            target: root
            property: "bounceOffset"
            to: -2
            duration: Theme.durationMicro
            easing.type: Theme.easingLight
        }
        NumberAnimation {
            target: root
            property: "bounceOffset"
            to: 0
            duration: Theme.durationFast
            easing.type: Theme.easingDefault
        }
    }

    SequentialAnimation {
        id: launchBounceAnim
        NumberAnimation {
            target: root
            property: "bounceOffset"
            to: -14
            duration: Theme.durationMicro
            easing.type: Theme.easingDefault
        }
        NumberAnimation {
            target: root
            property: "bounceOffset"
            to: -6
            duration: Theme.durationMicro
            easing.type: Theme.easingLight
        }
        NumberAnimation {
            target: root
            property: "bounceOffset"
            to: -2
            duration: Theme.durationMicro
            easing.type: Theme.easingLight
        }
        NumberAnimation {
            target: root
            property: "bounceOffset"
            to: 0
            duration: Theme.durationSm
            easing.type: Theme.easingDefault
        }
        ScriptAction {
            script: root.bounceCompleted()
        }
    }
}
