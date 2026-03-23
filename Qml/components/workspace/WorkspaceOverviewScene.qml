import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var windows: []
    property var layoutItems: []
    property var iconResolver: null
    property Item dragMapTarget: null
    property bool enableWindowDrag: true
    property int horizontalPadding: 32
    property bool transitionActive: false
    property bool active: true

    signal windowActivated(string viewId)
    signal windowCloseRequested(string viewId)
    signal windowDragStarted(string viewId, point center)
    signal windowDragMoved(point center)
    signal windowDragEnded()

    function normalizedAspectForWindow(w) {
        var ww = Math.max(120, Number((w && w.width) || 0))
        var hh = Math.max(80, Number((w && w.height) || 0))
        var a = ww / hh
        return Math.max(0.7, Math.min(2.4, a))
    }

    function recalcLayout() {
        var ws = windows || []
        if (ws.length <= 0 || width <= 0 || height <= 0) {
            layoutItems = []
            return
        }

        var results = WindowThumbnailLayoutEngine.calculateLayout(ws, width, height, cellGap)
        var items = []
        for (var i = 0; i < results.length; ++i) {
            var res = results[i]
            items.push({
                           "layoutX": res.x,
                           "layoutY": res.y,
                           "layoutW": res.width,
                           "layoutH": res.height,
                           "windowData": ws[i]
                       })
        }
        layoutItems = items
    }

    onWindowsChanged: recalcLayout()
    onWidthChanged: recalcLayout()
    onHeightChanged: recalcLayout()

    onVisibleChanged: {
        if (visible) {
            transitionActive = true
            transitionResetTimer.restart()
        }
    }

    Timer {
        id: transitionResetTimer
        interval: 32 // Ensure at least one frame with transitionActive = true
        onTriggered: root.transitionActive = false
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Label {
        visible: root.layoutItems.length === 0
        anchors.centerIn: parent
        text: "No windows in this workspace"
        color: Theme.color("textSecondary")
        font.pixelSize: Theme.fontSize("body")
    }

    Repeater {
        model: root.layoutItems

        delegate: WindowThumbnail {
            id: thumbnail
            required property var modelData

            x: (root.transitionActive || !root.active) ? Number((windowData && windowData.x) || 0) : Number((modelData && modelData.layoutX) || 0)
            y: (root.transitionActive || !root.active) ? Number((windowData && windowData.y) || 0) : Number((modelData && modelData.layoutY) || 0)
            width: (root.transitionActive || !root.active) ? Number((windowData && windowData.width) || 180) : Number((modelData && modelData.layoutW) || 180)
            height: (root.transitionActive || !root.active) ? Number((windowData && windowData.height) || 120) : Number((modelData && modelData.layoutH) || 120)
            windowData: modelData && modelData.windowData ? modelData.windowData : {}
            transitionActive: root.transitionActive
            mapTarget: root.dragMapTarget
            enableDrag: root.enableWindowDrag
            iconSource: (root.iconResolver && modelData && modelData.windowData)
                        ? String(root.iconResolver(modelData.windowData))
                        : "qrc:/icons/logo.svg"

            onActivated: function(viewId) { root.windowActivated(viewId) }
            onCloseRequested: function(viewId) { root.windowCloseRequested(viewId) }
            onDragStarted: function(viewId, center) { root.windowDragStarted(viewId, center) }
            onDragMoved: function(center) { root.windowDragMoved(center) }
            onDragEnded: root.windowDragEnded()
        }
    }
}
