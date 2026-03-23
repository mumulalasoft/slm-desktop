import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style
import "FileManagerContentHandlers.js" as FileManagerContentHandlers

Rectangle {
    id: root

    required property var hostRoot
    required property var tabModel
    property var fileManagerDialogsRef: null
    readonly property alias contentViewRef: contentView

    color: "transparent"
    border.width: Theme.borderWidthNone

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        FileManagerTabBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            hostRoot: root.hostRoot
            tabModel: root.tabModel
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            border.width: Theme.borderWidthNone

            FileManagerContentView {
                id: contentView
                anchors.fill: parent
                fileModel: root.hostRoot.fileModel
                viewMode: root.hostRoot.viewMode
                searchText: root.hostRoot.toolbarSearchText
                contentLoading: root.hostRoot.contentLoading
                trashView: root.hostRoot.trashView
                selectedIndex: root.hostRoot.selectedEntryIndex
                selectedIndexes: root.hostRoot.selectedEntryIndexes
                thumbnailsEnabled: true
                hostItem: root.hostRoot
                onSelectedIndexRequested: function(index, modifiers) {
                    root.hostRoot.applySelectionRequest(index, modifiers)
                    root.hostRoot.forceActiveFocus()
                }
                onContextMenuRequested: function(index, x, y) {
                    FileManagerContentHandlers.handleContextMenuRequested(
                                root.hostRoot, index, x, y)
                }
                onActivateRequested: function(index) {
                    FileManagerContentHandlers.handleActivateRequested(
                                root.hostRoot, index)
                }
                onRenameRequested: function(index) {
                    FileManagerContentHandlers.handleRenameRequested(
                                root.hostRoot, index)
                }
                onBackgroundContextRequested: function(x, y) {
                    FileManagerContentHandlers.handleBackgroundContextRequested(
                                root.hostRoot,
                                root.fileManagerDialogsRef
                                ? root.fileManagerDialogsRef.fileManagerMenusRef : null,
                                x,
                                y)
                }
                onClearSelectionRequested: {
                    root.hostRoot.clearSelection()
                }
                onSelectionRectRequested: function(indexes, modifiers, anchorIndex) {
                    root.hostRoot.applySelectionRect(indexes, modifiers, anchorIndex)
                }
                onGoUpRequested: {
                    if (root.hostRoot.fileModel && root.hostRoot.fileModel.goUp) {
                        root.hostRoot.fileModel.goUp()
                    }
                }
                onClearSearchRequested: {
                    root.hostRoot.toolbarSearchText = ""
                    root.hostRoot.closeToolbarSearch(true)
                }
                onCreateFolderRequested: {
                    root.hostRoot.createNewFolder()
                }
                onOpenHomeRequested: {
                    root.hostRoot.openHome()
                }
                onClearRecentsRequested: {
                    root.hostRoot.requestClearRecentFiles()
                }
                onDragStartRequested: function(index, pathValue, nameValue, isDirValue, copyMode, sceneX, sceneY) {
                    root.hostRoot.beginContentDrag(index, pathValue, nameValue,
                                                   copyMode, sceneX, sceneY)
                }
                onDragMoveRequested: function(sceneX, sceneY, hoverIndex, copyMode) {
                    root.hostRoot.updateContentDrag(sceneX, sceneY, copyMode)
                }
                onDragEndRequested: function(targetIndex) {
                    root.hostRoot.finishContentDrag()
                }
            }
        }

        FileManagerFooterBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.bottomMargin: 4
            hostRoot: root.hostRoot
        }
    }
}
