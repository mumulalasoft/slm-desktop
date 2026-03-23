import QtQuick 2.15

MouseArea {
    id: root

    property var listView: null

    signal clearSelectionRequested()

    anchors.fill: parent
    acceptedButtons: Qt.LeftButton
    propagateComposedEvents: true
    z: 2

    onPressed: function(mouse) {
        var idx = (root.listView && root.listView.indexAt)
                ? root.listView.indexAt(mouse.x, mouse.y)
                : -1
        if (idx >= 0) {
            mouse.accepted = false
        }
    }

    onClicked: function(mouse) {
        var idx = (root.listView && root.listView.indexAt)
                ? root.listView.indexAt(mouse.x, mouse.y)
                : -1
        if (idx >= 0) {
            mouse.accepted = false
            return
        }
        if (mouse && (mouse.modifiers & Qt.ControlModifier)) {
            root.clearSelectionRequested()
        }
        mouse.accepted = false
    }
}
