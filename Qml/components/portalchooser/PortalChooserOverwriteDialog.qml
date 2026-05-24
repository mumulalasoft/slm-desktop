import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Window {
    id: dialog

    property var parentWindow: null
    property var transientParentWindow: null
    property string targetPath: ""
    property bool escapeEnabled: false

    signal cancelRequested()
    signal replaceRequested()
    signal escapeRequested()

    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal
    transientParent: null
    title: "Replace File?"
    width: 460
    height: 182
    x: parentWindow ? parentWindow.x + Math.round((parentWindow.width - width) / 2) : 0
    y: parentWindow ? parentWindow.y + Math.round((parentWindow.height - height) / 2) : 0

    Shortcut {
        sequence: "Escape"
        enabled: dialog.escapeEnabled
        onActivated: dialog.escapeRequested()
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusWindow
        color: Theme.color("fileManagerWindowBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerWindowBorder")

        Column {
            anchors.fill: parent
            anchors.margins: Theme.metric("spacingXl")
            spacing: Theme.metric("spacingMd")

            DSStyle.Label {
                text: "Replace existing file?"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("semibold")
            }

            DSStyle.Label {
                width: parent.width
                text: String(dialog.targetPath || "")
                color: Theme.color("textSecondary")
                elide: Text.ElideMiddle
            }

            Item {
                width: parent.width
                height: 1
            }

            Row {
                width: parent.width
                spacing: 8
                Item {
                    width: Math.max(0, parent.width - 224)
                    height: 1
                }
                DSStyle.Button {
                    width: 104
                    height: Theme.metric("controlHeightRegular")
                    text: "Cancel"
                    onClicked: dialog.cancelRequested()
                }
                DSStyle.Button {
                    width: 112
                    height: Theme.metric("controlHeightRegular")
                    text: "Replace"
                    defaultAction: true
                    onClicked: dialog.replaceRequested()
                }
            }
        }
    }
}
