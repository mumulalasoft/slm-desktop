import QtQuick 2.15
import QtQuick.Window 2.15
import "." as OverlayComp

Window {
    id: root

    property var rootWindow: null
    property int panelHeight: 0
    property bool overlayVisible: false
    property var client: null
    readonly property bool animatedVisible: overlayVisible || clipboardOverlayFrame.opacity > 0.01

    signal closeRequested()

    visible: !!rootWindow && !!rootWindow.visible && animatedVisible
    color: "transparent"
    flags: Qt.Popup | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    transientParent: rootWindow
    title: "Clipboard Overlay"
    width: Math.min(760, Math.max(560, (rootWindow ? rootWindow.width : 0) - 200))
    height: Math.min(560, Math.max(360, (rootWindow ? rootWindow.height : 0) - 220))
    x: (rootWindow ? rootWindow.x : 0) + Math.round(((rootWindow ? rootWindow.width : width) - width) / 2)
    y: (rootWindow ? rootWindow.y : 0)
       + Math.max(panelHeight + 12, Math.round((rootWindow ? rootWindow.height : height) * 0.1))

    onVisibleChanged: {
        if (visible && overlayVisible) {
            requestActivate()
        }
    }
    onActiveChanged: {
        if (!active && overlayVisible) {
            closeRequested()
        }
    }

    Item {
        id: clipboardOverlayFrame
        anchors.fill: parent
        opacity: root.overlayVisible ? 1 : 0
        scale: root.overlayVisible ? 1.0 : 0.95
        transformOrigin: Item.Top

        Behavior on opacity {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }
        Behavior on scale {
            NumberAnimation { duration: 150; easing.type: Easing.OutBack }
        }

        OverlayComp.ClipboardOverlay {
            anchors.fill: parent
            opened: root.overlayVisible
            client: root.client
            onCloseRequested: root.closeRequested()
        }
    }
}

