import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Row {
    id: root

    property var filters: []
    property int filterIndex: 0
    property bool selectFolders: false
    property string sortKey: "name"
    property bool sortDescending: false
    property bool showHidden: false

    signal filterIndexChangedByUser(int index)
    signal sortKeyChangedByUser(string key)
    signal sortDirectionToggled()
    signal hiddenToggled()

    width: parent ? parent.width : 0
    height: 30
    spacing: 8

    Item {
        width: Math.max(120, root.width - 300)
        height: 1
    }

    ComboBox {
        visible: false
        width: 108
        height: root.height
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
            root.sortKeyChangedByUser(String(row.key || "name"))
        }
    }

    Button {
        visible: false
        width: 58
        height: root.height
        text: root.sortDescending ? "Z-A" : "A-Z"
        onClicked: root.sortDirectionToggled()
    }

    Button {
        width: 68
        height: root.height
        visible: false
        text: root.showHidden ? "Hide" : "Hidden"
        onClicked: root.hiddenToggled()
    }

    ComboBox {
        width: 106
        height: root.height
        model: root.filters
        textRole: "label"
        currentIndex: root.filterIndex
        visible: !root.selectFolders && root.filters && root.filters.length > 0
        onActivated: root.filterIndexChangedByUser(currentIndex)
    }
}
