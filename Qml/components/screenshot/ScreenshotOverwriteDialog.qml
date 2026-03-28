import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Window {
    id: dialog

    property var parentWindow: null
    property var transientParentWindow: null
    property string targetPath: ""
    property bool alwaysReplaceInSession: false

    signal cancelRequested()
    signal replaceRequested()
    signal alwaysReplaceToggled(bool checked)

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

    Component {
        id: screenshotOverwriteBodyComponent

        Column {
            width: dialog.width - (Theme.metric("spacingMd") * 2)
            spacing: 10

            DSStyle.Label {
                text: String(dialog.targetPath || "")
                color: Theme.color("textSecondary")
                elide: Text.ElideMiddle
            }

            DSStyle.CheckBox {
                text: "Always replace in this session"
                checked: dialog.alwaysReplaceInSession
                onToggled: dialog.alwaysReplaceToggled(checked)
            }
        }
    }

    Component {
        id: screenshotOverwriteFooterComponent

        Row {
            width: dialog.width - (Theme.metric("spacingMd") * 2)
            spacing: 8

            Item {
                width: Math.max(0, parent.width - 224)
                height: 1
            }
            DSStyle.Button {
                width: 104
                height: 34
                text: "Cancel"
                onClicked: dialog.cancelRequested()
            }
            DSStyle.Button {
                width: 112
                height: 34
                text: "Replace"
                defaultAction: true
                onClicked: dialog.replaceRequested()
            }
        }
    }

    DSStyle.WindowDialogSurface {
        anchors.fill: parent
        title: "Replace existing file?"
        bodyComponent: screenshotOverwriteBodyComponent
        footerComponent: screenshotOverwriteFooterComponent
    }
}
