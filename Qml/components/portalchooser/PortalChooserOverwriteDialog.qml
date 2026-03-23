import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: dialog

    property var parentWindow: null
    property var transientParentWindow: null
    property string targetPath: ""
    property bool escapeEnabled: false

    signal cancelRequested()
    signal replaceRequested()
    signal escapeRequested()

    color: Theme.color("menuBg")
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    transientParent: transientParentWindow ? transientParentWindow : parentWindow
    title: "Replace File?"
    width: 460
    height: 182
    x: parentWindow ? parentWindow.x + Math.round((parentWindow.width - width) / 2) : 0
    y: parentWindow ? parentWindow.y + Math.round((parentWindow.height - height) / 2) : 0

    onVisibleChanged: {
        if (visible) {
            requestActivate()
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: dialog.escapeEnabled
        onActivated: dialog.escapeRequested()
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusCard
        color: Theme.color("menuBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("menuBorder")

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Label {
                text: "Replace existing file?"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("bold")
            }

            Label {
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
                Button {
                    width: 104
                    height: 34
                    text: "Cancel"
                    onClicked: dialog.cancelRequested()
                }
                Button {
                    width: 112
                    height: 34
                    text: "Replace"
                    onClicked: dialog.replaceRequested()
                }
            }
        }
    }
}
