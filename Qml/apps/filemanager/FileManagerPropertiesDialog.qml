import QtQuick 2.15
import QtQuick.Controls 2.15
import Desktop_Shell

AppDialog {
    id: root

    required property var hostRoot

    title: "Properties"
    standardButtons: Dialog.NoButton
    dialogWidth: 420
    height: Math.min(hostRoot.height - 36, 500)
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)

    bodyPadding: 0
    footerPadding: 0

    bodyComponent: Component {
        FileManagerPropertiesDialogContent {
            hostRoot: root.hostRoot
            dialogRef: root
        }
    }
}
