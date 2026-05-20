import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

Button {
    id: root

    // Renamed from `action` to avoid colliding with QQC2 Button.action
    // (QtQuick.Controls.Action, marked FINAL — cannot be shadowed by a
    // derived type's property).
    property string actionKey: "shutdown"
    property string subtitle: ""
    property string iconName: "system-shutdown"
    property bool destructive: actionKey === "shutdown"

    signal actionInvoked(string action)

    Accessible.name: text
    Accessible.description: subtitle
    focusPolicy: Qt.StrongFocus
    implicitWidth: 126
    implicitHeight: 112
    padding: 0
    text: ""

    background: Rectangle {
        radius: 8
        color: root.down || root.visualFocus
               ? Theme.color("accentSoft")
               : (root.hovered ? Theme.color("panelBg") : Theme.color("surface"))
        border.color: root.visualFocus ? Theme.color("accent") : Theme.color("panelBorder")
        border.width: root.visualFocus ? 2 : Theme.borderWidthThin

        Behavior on color {
            ColorAnimation {
                duration: MotionController.preset("hover").duration
                easing.type: MotionController.preset("hover").easing
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 8
        anchors.fill: parent
        anchors.margins: 12

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 42
            height: 42
            radius: 21
            color: root.destructive ? Qt.rgba(0.82, 0.18, 0.16, 0.12)
                                    : Theme.color("accentSoft")

            Image {
                id: icon
                anchors.centerIn: parent
                width: 22
                height: 22
                source: "image://themeicon/" + root.iconName
                smooth: true
                fillMode: Image.PreserveAspectFit
            }

            DSStyle.Label {
                anchors.centerIn: parent
                visible: icon.status !== Image.Ready
                text: root.actionKey === "restart" ? "R"
                      : root.actionKey === "sleep" ? "S"
                      : root.actionKey === "logout" ? "L" : "P"
                color: root.destructive ? Qt.rgba(0.82, 0.18, 0.16, 1)
                                        : Theme.color("accent")
                font.weight: Theme.fontWeight("semibold")
            }
        }

        DSStyle.Label {
            Layout.fillWidth: true
            text: root.actionKey === "shutdown" ? qsTr("Shut Down") : root.text
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.weight: Theme.fontWeight("semibold")
        }

        DSStyle.Label {
            Layout.fillWidth: true
            text: root.subtitle
            visible: text.length > 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("caption")
        }
    }

    onClicked: root.actionInvoked(root.actionKey)
}
