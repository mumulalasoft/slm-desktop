import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    required property var windowData
    property Item mapTarget: null
    property bool enableDrag: true
    property bool showCloseButton: true
    property string iconSource: "qrc:/icons/logo.svg"
    property bool transitionActive: false
    readonly property bool focusedWindow: !!(windowData && windowData.focused)
    property real focusBlend: focusedWindow ? 1.0 : 0.0

    property bool wasDragged: false
    property string dragViewId: String((windowData && windowData.viewId) || "")
    property real homeX: x
    property real homeY: y

    signal activated(string viewId)
    signal closeRequested(string viewId)
    signal dragStarted(string viewId, point center)
    signal dragMoved(point center)
    signal dragEnded()

    function centerInTarget() {
        if (mapTarget) {
            return root.mapToItem(mapTarget, root.width / 2, root.height / 2)
        }
        return Qt.point(root.x + (root.width / 2), root.y + (root.height / 2))
    }

    onXChanged: {
        if (!thumbDragHandler.active) {
            homeX = x
        }
    }
    onYChanged: {
        if (!thumbDragHandler.active) {
            homeY = y
        }
    }

    Drag.active: thumbDragHandler.active
    Drag.source: root
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2
    Drag.keys: [ "workspaceWindowThumbnail" ]

    Behavior on focusBlend {
        enabled: !root.transitionActive && Theme.animationsEnabled
        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
    }
    Behavior on x { enabled: !root.transitionActive; NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }
    Behavior on y { enabled: !root.transitionActive; NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }
    Behavior on width { enabled: !root.transitionActive; NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }
    Behavior on height { enabled: !root.transitionActive; NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -2
        radius: Theme.radiusControlLarge + 2
        color: Theme.color("accent")
        opacity: root.focusBlend * Theme.opacityFaint
        visible: opacity > 0
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusControlLarge
        color: "transparent"
        border.width: root.focusedWindow ? Theme.borderWidthThick : Theme.borderWidthThin
        border.color: root.focusedWindow
                      ? Theme.color("accent")
                      : Theme.color("workspaceWindowBorderUnfocused")
        Behavior on border.width {
            enabled: !root.transitionActive && Theme.animationsEnabled
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
        }
        Behavior on border.color {
            enabled: !root.transitionActive && Theme.animationsEnabled
            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
        }
    }

    Image {
        id: previewImage
        anchors.fill: parent
        anchors.margins: 2
        fillMode: Image.PreserveAspectCrop
        source: String((windowData && windowData.previewSource) || "")
        smooth: true
        visible: source.toString().length > 0
        opacity: Theme.opacityMuted + (root.focusBlend * (Theme.opacitySurfaceStrong - Theme.opacityMuted))
        Behavior on opacity {
            enabled: !root.transitionActive && Theme.animationsEnabled
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: Theme.radiusControl
        color: Theme.color("workspaceWindowPlaceholder")
        visible: !previewImage.visible
        opacity: Theme.opacityMuted + (root.focusBlend * (Theme.opacitySurfaceStrong - Theme.opacityMuted))
        Behavior on opacity {
            enabled: !root.transitionActive && Theme.animationsEnabled
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
        }
    }

    Image {
        width: Math.min(parent.width * 0.24, parent.height * 0.24)
        height: width
        anchors.centerIn: parent
        source: root.iconSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: !previewImage.visible
    }

    Rectangle {
        visible: root.showCloseButton
        width: 24
        height: 24
        radius: Theme.radiusCard
        x: -10
        y: -10
        color: Theme.color("workspaceCloseBtnBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("workspaceCloseBtnBorder")

        Label {
            anchors.centerIn: parent
            text: "\u00d7"
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("body")
            font.weight: Theme.fontWeight("bold")
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRequested(root.dragViewId)
        }
    }

    Rectangle {
        width: Math.min(parent.width - 12, 240)
        height: 30
        radius: Theme.radiusControl
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -14
        color: Theme.color("workspaceCaptionBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("workspaceCaptionBorder")
        opacity: Theme.opacityMuted + (root.focusBlend * (Theme.opacitySurfaceStrong - Theme.opacityMuted))
        Behavior on opacity {
            enabled: !root.transitionActive && Theme.animationsEnabled
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
        }

        Row {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 6

            Image {
                width: 16
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                source: root.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                width: parent.width - 24
                anchors.verticalCenter: parent.verticalCenter
                text: (windowData && windowData.title && windowData.title.length > 0)
                      ? windowData.title
                      : String((windowData && windowData.appId) || "Application")
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("xs")
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: root.wasDragged = false
        onClicked: {
            if (!root.wasDragged) {
                root.activated(root.dragViewId)
            }
        }
    }

    DragHandler {
        id: thumbDragHandler
        enabled: root.enableDrag
        target: root
        onTranslationChanged: {
            root.dragMoved(root.centerInTarget())
            if (Math.abs(translation.x) > 4 || Math.abs(translation.y) > 4) {
                root.wasDragged = true
            }
        }
        onActiveChanged: {
            if (active) {
                root.dragStarted(root.dragViewId, root.centerInTarget())
            } else {
                root.x = root.homeX
                root.y = root.homeY
                root.dragEnded()
            }
        }
    }
}
