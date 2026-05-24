import QtQuick 2.15
import Slm_Desktop
import "qrc:/qt/qml/Slm_Desktop/Qml/components/desktop" as DesktopComp

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
    property alias nearbySendSheetRef: nearbySendSheet


    DesktopComp.ConnectServerDialog {
        id: connectServerDialog
        onConnectRequested: function(details) {
            hostRoot.connectServerHost = details.host
            hostRoot.connectServerPort = details.port
            hostRoot.connectServerShare = details.share
            hostRoot.connectServerFolder = details.folder
            hostRoot.connectServerDomain = details.domain
            hostRoot.connectServerUser = details.user
            hostRoot.connectServerPassword = details.password
            hostRoot.submitConnectServer()
        }
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

    FileManagerNearbySendSheet {
        id: nearbySendSheet
        hostRoot: root.hostRoot
    }
}
