import QtQuick 2.15

Item {
    id: root
    visible: false
    width: 0
    height: 0

    property bool shellContextMenuOpen: false
    property bool anyPopupOpen: false
    property bool expanded: false

    onAnyPopupOpenChanged: {
        if (anyPopupOpen) {
            collapseTimer.stop()
            expanded = true
        } else {
            collapseTimer.restart()
        }
    }

    Timer {
        id: collapseTimer
        interval: 900
        repeat: false
        onTriggered: {
            if (!root.shellContextMenuOpen && !root.anyPopupOpen) {
                root.expanded = false
            }
        }
    }

    Timer {
        id: guardTimer
        interval: 1000
        repeat: true
        running: false
        onTriggered: {
            if (root.expanded &&
                    !root.shellContextMenuOpen &&
                    !root.anyPopupOpen) {
                root.expanded = false
            }
        }
    }
}
