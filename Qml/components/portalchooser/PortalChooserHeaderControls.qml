import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Row {
    id: root

    property string titleText: ""
    property string sortKey: "name"
    property bool showHidden: false
    property bool sortDescending: false

    signal sortKeySelected(string key)
    signal toggleHiddenRequested()
    signal toggleSortDirectionRequested()

    width: parent ? parent.width : 0
    height: 34
    spacing: 8

    Label {
        width: Math.max(0, parent.width - 284)
        height: parent.height
        text: root.titleText
        color: Theme.color("textPrimary")
        font.pixelSize: Theme.fontSize("titleLarge")
        font.weight: Theme.fontWeight("bold")
        verticalAlignment: Text.AlignVCenter
    }

    ComboBox {
        id: topPortalSortCombo
        width: 108
        height: 30
        model: [
            { "label": "Name", "key": "name" },
            { "label": "Modified", "key": "date" },
            { "label": "Type", "key": "type" }
        ]
        textRole: "label"
        currentIndex: {
            if (root.sortKey === "date") return 1
            if (root.sortKey === "type") return 2
            return 0
        }
        onActivated: {
            var row = model[currentIndex] || ({ "key": "name" })
            root.sortKeySelected(String(row.key || "name"))
        }
    }

    Button {
        width: 68
        height: 30
        text: root.showHidden ? "Hide" : "Show"
        onClicked: root.toggleHiddenRequested()
    }

    Button {
        text: root.sortDescending ? "Sort: Z-A" : "Sort: A-Z"
        width: 86
        height: 30
        onClicked: root.toggleSortDirectionRequested()
    }
}
