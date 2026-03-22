import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Desktop_Shell

AppDialog {
    id: root

    required property var hostRoot

    property string originalName: ""
    property bool originalIsDir: false
    property var renameFieldRef: null

    function openAndFocus(nameValue: string, isDirValue: bool) {
        originalName = String(nameValue || "")
        originalIsDir = !!isDirValue
        open()
        Qt.callLater(function() {
            var field = root.renameFieldRef
            if (!field) {
                return
            }
            field.forceActiveFocus()
            if (!originalIsDir) {
                var dot = originalName.lastIndexOf(".")
                if (dot > 0) {
                    field.select(0, dot)
                    return
                }
            }
            field.selectAll()
        })
    }

    title: "Rename"
    standardButtons: Dialog.NoButton
    dialogWidth: 500
    height: 238
    bodyPadding: 14
    footerPadding: 12
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)

    bodyComponent: Component {
        ColumnLayout {
            spacing: 8
            implicitHeight: 140

            Label {
                Layout.fillWidth: true
                text: String(hostRoot.contextEntryName || "")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                elide: Text.ElideMiddle
            }

            Label {
                text: "New name"
                color: Theme.color("textPrimary")
            }

            TextField {
                id: renameField
                Layout.fillWidth: true
                text: hostRoot.renameInputText
                placeholderText: "Enter new name"
                color: Theme.color("textPrimary")
                selectionColor: Theme.color("selectedItem")
                selectedTextColor: Theme.color("selectedItemText")
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: Theme.color("fileManagerSearchBg")
                    border.width: Theme.borderWidthThin
                    border.color: renameField.activeFocus
                                  ? Theme.color("accent")
                                  : Theme.color("fileManagerControlBorder")
                }
                onTextChanged: {
                    hostRoot.renameDialogError = ""
                    hostRoot.renameInputText = text
                }
                onAccepted: hostRoot.applyRenameContextEntry()
                Component.onCompleted: root.renameFieldRef = renameField
            }

            Label {
                Layout.fillWidth: true
                visible: String(hostRoot.renameDialogError || "").length > 0
                text: String(hostRoot.renameDialogError || "")
                color: Theme.color("error")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.Wrap
            }
        }
    }

    footerComponent: Component {
        RowLayout {
            implicitHeight: Math.max(32, Theme.metric("controlHeightRegular"))
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Cancel"
                onClicked: root.close()
            }

            Button {
                text: "Rename"
                highlighted: true
                enabled: String(hostRoot.renameInputText || "").trim().length > 0
                onClicked: hostRoot.applyRenameContextEntry()
            }
        }
    }

    onClosed: {
        hostRoot.renameDialogError = ""
        hostRoot.renameInputText = ""
        originalName = ""
        originalIsDir = false
        renameFieldRef = null
    }
}
