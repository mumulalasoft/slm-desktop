import QtQuick 2.15
import Slm_Desktop

Item {
    id: root

    required property var hostRoot

    property alias connectServerDialogRef: connectServerDialog
    property alias batchOverlayPopupRef: batchOverlayPopup
    property alias fileManagerMenusRef: fileManagerMenus
    property alias sidebarContextMenuRef: sidebarContextMenu
    property alias clearRecentsDialogRef: clearRecentsDialog
    property alias quickPreviewDialogRef: quickPreviewDialog
    property alias renameDialogRef: renameDialog
    property alias propertiesDialogRef: propertiesDialog
    property alias compressDialogRef: compressDialog
    property alias openWithDialogRef: openWithDialog
    property alias folderShareDialogRef: folderShareDialog
    property alias storageVolumeSelectorDialogRef: storageVolumeSelectorDialog
    property var shareSheetRef: shareSheet


    FileManagerConnectServerDialog {
        id: connectServerDialog
        hostRoot: root.hostRoot
    }

    FileManagerBatchOverlayPopup {
        id: batchOverlayPopup
        hostRoot: root.hostRoot
    }

    FileManagerMenus {
        id: fileManagerMenus
        hostRoot: root.hostRoot
    }

    FileManagerSidebarContextMenu {
        id: sidebarContextMenu
        hostRoot: root.hostRoot
    }

    FileManagerClearRecentsDialog {
        id: clearRecentsDialog
        hostRoot: root.hostRoot
    }

    FileManagerQuickPreviewDialog {
        id: quickPreviewDialog
        hostRoot: root.hostRoot
    }

    FileManagerRenameDialog {
        id: renameDialog
        hostRoot: root.hostRoot
    }

    FileManagerPropertiesDialog {
        id: propertiesDialog
        hostRoot: root.hostRoot
    }

    FileManagerCompressDialog {
        id: compressDialog
        hostRoot: root.hostRoot
    }

    FileManagerOpenWithDialog {
        id: openWithDialog
        hostRoot: root.hostRoot
    }

    FileManagerFolderShareDialog {
        id: folderShareDialog
        hostRoot: root.hostRoot
    }

    FileManagerStorageVolumeSelectorDialog {
        id: storageVolumeSelectorDialog
        hostRoot: root.hostRoot
    }

    Loader {
        id: shareSheetLoader
        active: true
        sourceComponent: shareSheetComponent
    }

    Component {
        id: shareSheetComponent
        FileManagerShareSheet {
            hostRoot: root.hostRoot
        }
    }

    readonly property var shareSheet: shareSheetLoader.item
}
