import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: sheet
    property bool sheetVisible: false
    property string targetName: ""
    signal targetNameRequested(string name)
    signal cancelRequested()
    signal applyRequested()

    function focusEditor() {
        renameField.forceActiveFocus()
        renameField.selectAll()
    }

    visible: sheetVisible

    Rectangle {
        anchors.fill: parent
        color: Theme.color("overlay")
        MouseArea {
            anchors.fill: parent
            onClicked: sheet.cancelRequested()
        }
    }

    Rectangle {
        id: renameSheet
        width: 360
        height: 150
        radius: Theme.radiusCard
        color: Theme.color("menuBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("menuBorder")
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            DSStyle.Label {
                text: "Rename"
                color: Theme.color("textPrimary")
                font.weight: Theme.fontWeight("bold")
            }
            DSStyle.TextField {
                id: renameField
                Layout.fillWidth: true
                text: sheet.targetName
                onTextChanged: sheet.targetNameRequested(text)
                onAccepted: sheet.applyRequested()
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                DSStyle.Button {
                    text: "Cancel"
                    onClicked: sheet.cancelRequested()
                }
                DSStyle.Button {
                    text: "Apply"
                    onClicked: sheet.applyRequested()
                }
            }
        }
    }
}
