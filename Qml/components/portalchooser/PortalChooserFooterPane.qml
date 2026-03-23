import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Column {
    id: root

    property string chooserMode: "open"
    property string chooserName: ""
    property string validationError: ""
    property int selectionCount: 0
    property int totalCount: 0
    property bool allowMultiple: false
    property bool primaryEnabled: false
    property bool selectFolders: false

    readonly property alias fieldActiveFocus: saveNameRow.fieldActiveFocus

    signal nameEdited(string textValue)
    signal clearSelectionRequested()
    signal invertSelectionRequested()
    signal cancelRequested()
    signal primaryRequested()

    width: parent ? parent.width : 0
    spacing: 8

    PortalChooserSaveNameRow {
        id: saveNameRow
        width: root.width
        height: root.chooserMode === "save" ? 34 : 0
        visible: root.chooserMode === "save"
        nameText: root.chooserName
        onNameEdited: function(textValue) {
            root.nameEdited(textValue)
        }
    }

    Label {
        width: root.width
        height: root.chooserMode === "save" && root.validationError.length > 0 ? 18 : 0
        visible: root.chooserMode === "save" && root.validationError.length > 0
        text: root.validationError
        color: Theme.color("warning")
        elide: Text.ElideRight
        font.pixelSize: Theme.fontSize("small")
    }

    PortalChooserActionBar {
        width: root.width
        selectionSummaryText: {
            var c = root.selectionCount
            if (c <= 0) return ""
            return c + " selected • total " + Number(root.totalCount || 0) + " items"
        }
        showClearSelection: root.selectionCount > 0
        showInvertSelection: root.allowMultiple && Number(root.totalCount || 0) > 0
        primaryText: root.chooserMode === "save"
                     ? "Save"
                     : (root.selectFolders ? "Select" : "Open")
        primaryEnabled: root.primaryEnabled
        onClearSelectionRequested: root.clearSelectionRequested()
        onInvertSelectionRequested: root.invertSelectionRequested()
        onCancelRequested: root.cancelRequested()
        onPrimaryRequested: root.primaryRequested()
    }
}
