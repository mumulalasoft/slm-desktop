import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Rectangle {
    id: root

    property var entriesModel: null
    property var selectedPaths: ({})
    property bool allowMultiple: false
    property bool selectFolders: false
    property string chooserMode: "open"
    property real dateColumnWidth: 140
    property real kindColumnWidth: 92
    property int iconRevision: 0
    property string errorText: ""
    property real nameColumnWidth: 220

    readonly property alias listView: portalChooserList

    signal dateColumnWidthDragged(real value, real rowWidth)
    signal kindColumnWidthDragged(real value, real rowWidth)
    signal resizeReleased()
    signal rowClicked(int rowIndex, string rowPath, bool rowIsDir, string rowName, int modifiers)
    signal rowDoubleClicked(int rowIndex, string rowPath, bool rowIsDir, string rowName)
    signal clearSelectionRequested()

    width: parent ? parent.width : 0
    height: parent ? (parent.height - 42) : 0
    radius: Theme.radiusControlLarge
    color: Theme.color("fileManagerWindowBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("fileManagerWindowBorder")
    clip: true

    ListView {
        id: portalChooserList
        anchors.fill: parent
        anchors.margins: 0
        model: root.entriesModel
        spacing: 0

        header: PortalChooserListHeader {
            listWidth: portalChooserList.width
            nameColumnWidth: root.nameColumnWidth
            dateColumnWidth: root.dateColumnWidth
            kindColumnWidth: root.kindColumnWidth
            onDateColumnWidthDragged: function(value, rowWidth) {
                root.dateColumnWidthDragged(value, rowWidth)
            }
            onKindColumnWidthDragged: function(value, rowWidth) {
                root.kindColumnWidthDragged(value, rowWidth)
            }
            onResizeReleased: root.resizeReleased()
        }

        delegate: PortalChooserListRow {
            selectedPaths: root.selectedPaths
            allowMultiple: root.allowMultiple
            selectFolders: root.selectFolders
            chooserMode: root.chooserMode
            listWidth: portalChooserList.width
            nameColumnWidth: root.nameColumnWidth
            dateColumnWidth: root.dateColumnWidth
            kindColumnWidth: root.kindColumnWidth
            iconRevision: root.iconRevision
            onRowClicked: function(rowIndex, modifiers) {
                root.rowClicked(rowIndex, path, isDir, name, modifiers)
            }
            onRowDoubleClicked: function(rowIndex) {
                root.rowDoubleClicked(rowIndex, path, isDir, name)
            }
        }
    }

    PortalChooserListBackgroundArea {
        listView: portalChooserList
        onClearSelectionRequested: root.clearSelectionRequested()
    }

    PortalChooserEmptyState {
        showEmptyState: Number(portalChooserList.count || 0) <= 0
        errorText: root.errorText
    }
}
