import QtQuick 2.15
import Slm_Desktop

Row {
    id: root

    property string previewPath: ""
    property var placesModel: null
    property string currentDir: ""
    property var filters: []
    property int filterIndex: 0
    property bool selectFolders: false
    property string sortKey: "name"
    property bool sortDescending: false
    property bool showHidden: false
    property var entriesModel: null
    property var selectedPaths: ({})
    property bool allowMultiple: false
    property string chooserMode: "open"
    property real dateColumnWidth: 140
    property real kindColumnWidth: 92
    property int iconRevision: 0
    property string errorText: ""

    readonly property alias listView: listPane.listView
    readonly property real placesPaneWidth: Math.min(220, Math.max(174, Math.round(root.width * 0.24)))
    readonly property bool showPreview: root.previewPath.length > 0 && root.width >= 760
    readonly property real previewPaneWidth: showPreview ? 250 : 0
    readonly property bool showListControls: !root.selectFolders
                                             && root.filters
                                             && root.filters.length > 0

    signal directoryRequested(string path)
    signal mountRequested(string device)
    signal unmountRequested(string device)
    signal filterIndexChangedByUser(int index)
    signal sortKeyChangedByUser(string key)
    signal sortDirectionToggled()
    signal hiddenToggled()
    signal dateColumnWidthDragged(real value, real rowWidth)
    signal kindColumnWidthDragged(real value, real rowWidth)
    signal resizeReleased()
    signal rowClicked(int rowIndex, string rowPath, bool rowIsDir, string rowName, int modifiers)
    signal rowDoubleClicked(int rowIndex, string rowPath, bool rowIsDir, string rowName)
    signal clearSelectionRequested()

    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    spacing: Theme.metric("spacingSm")

    PortalChooserPlacesPane {
        width: root.placesPaneWidth
        height: root.height
        placesModel: root.placesModel
        currentDir: root.currentDir
        iconRevision: root.iconRevision
        onDirectoryRequested: function(path) { root.directoryRequested(path) }
        onMountRequested: function(device) { root.mountRequested(device) }
        onUnmountRequested: function(device) { root.unmountRequested(device) }
    }

    Column {
        width: Math.max(280, root.width - root.placesPaneWidth - root.previewPaneWidth - (root.showPreview ? 2 : 1) * root.spacing)
        height: root.height
        spacing: Theme.metric("spacingXs")

        PortalChooserListControls {
            id: listControls
            width: parent.width
            height: root.showListControls ? Theme.metric("controlHeightRegular") : 0
            visible: root.showListControls
            filters: root.filters
            filterIndex: root.filterIndex
            selectFolders: root.selectFolders
            sortKey: root.sortKey
            sortDescending: root.sortDescending
            showHidden: root.showHidden
            onFilterIndexChangedByUser: function(index) {
                root.filterIndexChangedByUser(index)
            }
            onSortKeyChangedByUser: function(key) {
                root.sortKeyChangedByUser(key)
            }
            onSortDirectionToggled: root.sortDirectionToggled()
            onHiddenToggled: root.hiddenToggled()
        }

        PortalChooserListPane {
            id: listPane
            width: parent.width
            height: parent.height - (listControls.visible ? (listControls.height + parent.spacing) : 0)
            entriesModel: root.entriesModel
            selectedPaths: root.selectedPaths
            allowMultiple: root.allowMultiple
            selectFolders: root.selectFolders
            chooserMode: root.chooserMode
            nameColumnWidth: Math.max(220, width - 20 - Math.max(90, Number(root.dateColumnWidth || 140)) - Math.max(80, Number(root.kindColumnWidth || 92)) - 8)
            dateColumnWidth: root.dateColumnWidth
            kindColumnWidth: root.kindColumnWidth
            iconRevision: root.iconRevision
            errorText: root.errorText
            onDateColumnWidthDragged: function(value, rowWidth) {
                root.dateColumnWidthDragged(value, rowWidth)
            }
            onKindColumnWidthDragged: function(value, rowWidth) {
                root.kindColumnWidthDragged(value, rowWidth)
            }
            onResizeReleased: root.resizeReleased()
            onRowClicked: function(rowIndex, rowPath, rowIsDir, rowName, modifiers) {
                root.rowClicked(rowIndex, rowPath, rowIsDir, rowName, modifiers)
            }
            onRowDoubleClicked: function(rowIndex, rowPath, rowIsDir, rowName) {
                root.rowDoubleClicked(rowIndex, rowPath, rowIsDir, rowName)
            }
            onClearSelectionRequested: root.clearSelectionRequested()
        }
    }

    PortalChooserPreviewPanel {
        visible: root.showPreview
        width: root.previewPaneWidth
        height: root.height
        previewPath: root.previewPath
    }
}
