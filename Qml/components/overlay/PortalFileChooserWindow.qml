import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import SlmStyle as DSStyle
import "../portalchooser" as PortalChooserComp

Window {
    id: root

    property var rootWindow: null
    property var chooserApi: null
    property var entriesModel: null
    property var placesModel: null

    function wireChooserContext() {
        if (!chooserApi || !chooserUiLoader.item) {
            return
        }
        chooserApi.listPane = chooserUiLoader.item.listPane
        chooserApi.pathBar = chooserUiLoader.item.pathBar
        chooserApi.saveNameRow = chooserUiLoader.item.saveNameRow
    }

    Component.onCompleted: wireChooserContext()
    onChooserApiChanged: wireChooserContext()

    visible: !!rootWindow
             && !!chooserApi
             && !!chooserApi.shellRoot
             && !!rootWindow.visible
             && !!chooserApi.shellRoot.portalFileChooserVisible
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    transientParent: rootWindow
    title: (chooserApi && chooserApi.shellRoot) ? String(chooserApi.shellRoot.portalChooserTitle || "") : ""
    width: 900
    height: 560
    x: (rootWindow ? rootWindow.x : 0) + Math.round(((rootWindow ? rootWindow.width : width) - width) / 2)
    y: (rootWindow ? rootWindow.y : 0) + Math.round(((rootWindow ? rootWindow.height : height) - height) / 2)

    onVisibleChanged: {
        if (visible) {
            wireChooserContext()
            width = Math.max(760, Number((chooserApi.shellRoot && chooserApi.shellRoot.portalChooserWindowWidth)
                                         || chooserApi.portalChooserDefaultWidth()))
            height = Math.max(500, Number((chooserApi.shellRoot && chooserApi.shellRoot.portalChooserWindowHeight)
                                          || chooserApi.portalChooserDefaultHeight()))
        }
    }
    onClosing: function(close) {
        if (chooserApi && chooserApi.shellRoot && chooserApi.shellRoot.portalFileChooserVisible) {
            close.accepted = false
            chooserApi.finishPortalFileChooser(false, true, "canceled")
        }
    }
    onWidthChanged: {
        if (chooserApi && chooserApi.shellRoot) {
            chooserApi.shellRoot.portalChooserWindowWidth = Number(width)
        }
        chooserApi.portalChooserClampColumns(width - 20)
        chooserApi.portalChooserSaveWindowSize()
    }
    onHeightChanged: {
        if (chooserApi && chooserApi.shellRoot) {
            chooserApi.shellRoot.portalChooserWindowHeight = Number(height)
        }
        chooserApi.portalChooserSaveWindowSize()
    }

    Shortcut {
        sequence: "Ctrl+H"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: {
            chooserApi.shellRoot.portalChooserShowHidden = !chooserApi.shellRoot.portalChooserShowHidden
            chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
        }
    }

    Shortcut {
        sequence: "Backspace"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: chooserApi.portalChooserHandleGoUpShortcut(true)
    }

    Shortcut {
        sequence: "Alt+Up"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: chooserApi.portalChooserHandleGoUpShortcut(false)
    }

    Shortcut {
        sequence: "Ctrl+L"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: chooserApi.portalChooserFocusPathEditor()
    }

    Shortcut {
        sequence: "F6"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: chooserApi.portalChooserFocusPathEditor()
    }

    PortalChooserComp.PortalChooserShortcuts {
        active: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        chooserApi: chooserApi
    }

    Shortcut {
        sequence: "Return"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: chooserApi.portalChooserHandleEnterShortcut(portalPathBar ? portalPathBar.editorText : "")
    }

    Shortcut {
        sequence: "Enter"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: chooserApi.portalChooserHandleEnterShortcut(portalPathBar ? portalPathBar.editorText : "")
    }

    Shortcut {
        sequence: "Escape"
        enabled: !!chooserApi && !!chooserApi.shellRoot && !!chooserApi.shellRoot.portalFileChooserVisible
        onActivated: chooserApi.portalChooserHandleEscapeShortcut()
    }

    Loader {
        id: chooserUiLoader
        anchors.fill: parent
        active: !!chooserApi
        sourceComponent: chooserUiComponent
        onLoaded: root.wireChooserContext()
    }

    Component {
        id: chooserUiComponent
        Item {
            anchors.fill: parent
            property var listPane: portalChooserListPane
            property var pathBar: portalPathBar
            property var saveNameRow: portalSaveNameRow

        Rectangle {
            id: portalChooserCard
            anchors.fill: parent
            radius: Theme.radiusWindow
            color: Theme.color("fileManagerWindowBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerWindowBorder")
            clip: true

            Column {
                anchors.fill: parent
                anchors.margins: Theme.metric("spacingLg")
                spacing: 0

                Rectangle {
                    id: portalChooserToolbar
                    readonly property real contentHeight: portalChooserTitleRow.height
                                                      + portalChooserToolbarColumn.spacing
                                                      + portalPathBar.height

                    width: parent.width
                    height: contentHeight
                    color: "transparent"
                    border.width: Theme.borderWidthNone

                    Column {
                        id: portalChooserToolbarColumn
                        anchors.fill: parent
                        spacing: Theme.metric("spacingXxs")

                        Row {
                            id: portalChooserTitleRow
                            width: parent.width
                            height: Theme.metric("controlHeightRegular")
                            spacing: Theme.metric("spacingSm")

                            DSStyle.Label {
                                width: Math.max(0, parent.width - portalChooserSortControls.width - parent.spacing)
                                height: parent.height
                                text: chooserApi.shellRoot ? chooserApi.shellRoot.portalChooserTitle : ""
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("title")
                                font.family: Theme.fontFamilyDisplay
                                font.weight: Theme.fontWeight("semibold")
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            PortalChooserComp.PortalChooserHeaderControls {
                                id: portalChooserSortControls
                                width: 218
                                height: parent.height
                                showTitle: false
                                sortKey: chooserApi.shellRoot ? chooserApi.shellRoot.portalChooserSortKey : "name"
                                showHidden: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserShowHidden : false
                                sortDescending: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserSortDescending : false
                                onSortKeySelected: function(key) {
                                    chooserApi.shellRoot.portalChooserSortKey = key
                                    chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
                                }
                                onToggleHiddenRequested: {
                                    chooserApi.shellRoot.portalChooserShowHidden = !chooserApi.shellRoot.portalChooserShowHidden
                                    chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
                                }
                                onToggleSortDirectionRequested: {
                                    chooserApi.shellRoot.portalChooserSortDescending = !chooserApi.shellRoot.portalChooserSortDescending
                                    chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
                                }
                            }
                        }

                        PortalChooserComp.PortalChooserPathBar {
                            id: portalPathBar
                            width: parent.width
                            height: Theme.metric("controlHeightLarge")
                            pathEditMode: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserPathEditMode : false
                            currentDir: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserCurrentDir || "") : ""
                            breadcrumbSegments: chooserApi.portalChooserBreadcrumbSegments()
                            iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                           ? ThemeIconController.revision : 0)
                            onPathEditModeChangeRequested: function(value) {
                                chooserApi.shellRoot.portalChooserPathEditMode = value
                                if (!value) {
                                    portalChooserCard.forceActiveFocus()
                                }
                            }
                            onLoadDirectoryRequested: function(path) {
                                chooserApi.portalChooserLoadDirectory(String(path || ""))
                            }
                        }
                    }
                }

                PortalChooserComp.PortalChooserContentPane {
                    id: portalChooserListPane
                    width: parent.width
                    height: Math.max(280, parent.height - portalChooserToolbar.height - portalSaveNameRow.height)
                    previewPath: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserPreviewPath || "") : ""
                    placesModel: root.placesModel
                    currentDir: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserCurrentDir || "") : ""
                    filters: chooserApi.shellRoot ? chooserApi.shellRoot.portalChooserFilters : []
                    filterIndex: chooserApi.shellRoot ? Number(chooserApi.shellRoot.portalChooserFilterIndex || 0) : 0
                    selectFolders: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserSelectFolders : false
                    sortKey: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserSortKey || "name") : "name"
                    sortDescending: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserSortDescending : false
                    showHidden: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserShowHidden : false
                    entriesModel: root.entriesModel
                    selectedPaths: chooserApi.shellRoot ? chooserApi.shellRoot.portalChooserSelectedPaths : ({})
                    allowMultiple: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserAllowMultiple : false
                    chooserMode: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserMode || "open") : "open"
                    dateColumnWidth: chooserApi.shellRoot ? Number(chooserApi.shellRoot.portalChooserDateColumnWidth || 180) : 180
                    kindColumnWidth: chooserApi.shellRoot ? Number(chooserApi.shellRoot.portalChooserKindColumnWidth || 160) : 160
                    iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                   ? ThemeIconController.revision : 0)
                    errorText: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserErrorText || "") : ""
                    onDirectoryRequested: function(path) {
                        chooserApi.portalChooserLoadDirectory(String(path || ""))
                    }
                    onMountRequested: function(device) {
                        var fm = (chooserApi && chooserApi.shellRoot) ? chooserApi.shellRoot.fileManagerApiRef : null
                        if (!fm && typeof FileManagerApi !== "undefined" && FileManagerApi) {
                            fm = FileManagerApi
                        }
                        if (!fm || !fm.startMountStorageDevice) {
                            return
                        }
                        var dev = String(device || "")
                        if (dev.length <= 0) {
                            return
                        }
                        fm.startMountStorageDevice(dev)
                        chooserApi.portalChooserRefreshStoragePlaces(true)
                    }
                    onUnmountRequested: function(device) {
                        var fm = (chooserApi && chooserApi.shellRoot) ? chooserApi.shellRoot.fileManagerApiRef : null
                        if (!fm && typeof FileManagerApi !== "undefined" && FileManagerApi) {
                            fm = FileManagerApi
                        }
                        if (!fm || !fm.startUnmountStorageDevice) {
                            return
                        }
                        var dev = String(device || "")
                        if (dev.length <= 0) {
                            return
                        }
                        fm.startUnmountStorageDevice(dev)
                        chooserApi.portalChooserRefreshStoragePlaces(true)
                    }
                    onFilterIndexChangedByUser: function(index) {
                        chooserApi.shellRoot.portalChooserFilterIndex = index
                        chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
                    }
                    onSortKeyChangedByUser: function(key) {
                        chooserApi.shellRoot.portalChooserSortKey = String(key || "name")
                        chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
                    }
                    onSortDirectionToggled: {
                        chooserApi.shellRoot.portalChooserSortDescending = !chooserApi.shellRoot.portalChooserSortDescending
                        chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
                    }
                    onHiddenToggled: {
                        chooserApi.shellRoot.portalChooserShowHidden = !chooserApi.shellRoot.portalChooserShowHidden
                        chooserApi.portalChooserLoadDirectory(chooserApi.shellRoot.portalChooserCurrentDir)
                    }
                    onDateColumnWidthDragged: function(value, rowWidth) {
                        chooserApi.shellRoot.portalChooserDateColumnWidth = value
                        chooserApi.portalChooserClampColumns(rowWidth)
                    }
                    onKindColumnWidthDragged: function(value, rowWidth) {
                        chooserApi.shellRoot.portalChooserKindColumnWidth = value
                        chooserApi.portalChooserClampColumns(rowWidth)
                    }
                    onResizeReleased: chooserApi.portalChooserSaveUiPreferences()
                    onRowClicked: function(rowIndex, rowPath, rowIsDir, rowName, modifiers) {
                        if (chooserApi.shellRoot.portalChooserAllowMultiple) {
                            if (modifiers & Qt.ShiftModifier) {
                                chooserApi.portalChooserSelectRangeTo(rowIndex)
                                return
                            }
                            if (modifiers & Qt.ControlModifier) {
                                chooserApi.portalChooserToggleIndexSelection(rowIndex)
                                return
                            }
                            chooserApi.portalChooserSetSingleSelection(rowPath, rowIsDir, rowName)
                            chooserApi.shellRoot.portalChooserSelectedIndex = rowIndex
                            chooserApi.shellRoot.portalChooserAnchorIndex = rowIndex
                            chooserApi.portalChooserUpdatePreviewPath()
                            return
                        }
                        chooserApi.portalChooserSetSingleSelection(rowPath, rowIsDir, rowName)
                        chooserApi.shellRoot.portalChooserSelectedIndex = rowIndex
                        chooserApi.shellRoot.portalChooserAnchorIndex = rowIndex
                    }
                    onRowDoubleClicked: function(rowIndex, rowPath, rowIsDir, rowName) {
                        if (rowIsDir) {
                            if (chooserApi.shellRoot.portalChooserSelectFolders
                                    && chooserApi.shellRoot.portalChooserMode !== "save") {
                                chooserApi.portalChooserSetSingleSelection(rowPath, rowIsDir, rowName)
                                chooserApi.finishPortalFileChooser(true, false, "")
                                return
                            }
                            chooserApi.portalChooserLoadDirectory(rowPath)
                            return
                        }
                        if (chooserApi.shellRoot.portalChooserMode !== "save"
                                && !chooserApi.shellRoot.portalChooserSelectFolders) {
                            chooserApi.portalChooserSetSingleSelection(rowPath, rowIsDir, rowName)
                            chooserApi.finishPortalFileChooser(true, false, "")
                        }
                    }
                    onClearSelectionRequested: chooserApi.portalChooserClearSelectionAndPreview()
                }

                PortalChooserComp.PortalChooserFooterPane {
                    id: portalSaveNameRow
                    width: parent.width
                    height: contentHeight
                    chooserMode: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserMode || "open") : "open"
                    chooserName: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserName || "") : ""
                    validationError: chooserApi.shellRoot ? String(chooserApi.shellRoot.portalChooserValidationError || "") : ""
                    selectionCount: chooserApi.portalChooserSelectionCount()
                    totalCount: Number(root.entriesModel.count || 0)
                    allowMultiple: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserAllowMultiple : false
                    primaryEnabled: chooserApi.portalChooserPrimaryActionEnabled()
                    selectFolders: chooserApi.shellRoot ? !!chooserApi.shellRoot.portalChooserSelectFolders : false
                    onNameEdited: function(textValue) {
                        chooserApi.shellRoot.portalChooserName = textValue
                        chooserApi.shellRoot.portalChooserValidationError = ""
                    }
                    onClearSelectionRequested: chooserApi.portalChooserClearSelectionAndPreview()
                    onInvertSelectionRequested: chooserApi.portalChooserInvertSelection()
                    onCancelRequested: chooserApi.finishPortalFileChooser(false, true, "canceled")
                    onPrimaryRequested: chooserApi.finishPortalFileChooser(true, false, "")
                }
            }
        }
    }
    }
}
