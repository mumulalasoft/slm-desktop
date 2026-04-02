import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

// AppPickerDialog — lets the user type or select an application ID
// (e.g. "firefox", "org.gnome.Nautilus") to configure per-app overrides.

Dialog {
    id: root
    title: qsTr("Select Application")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 360
    padding: 20

    property string selectedAppId: ""

    signal accepted(string appId)

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.color("surface")
        border.color: Theme.color("panelBorder")
        border.width: 1
    }

    header: Item {
        implicitHeight: headerText.implicitHeight + 24
        Text {
            id: headerText
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
            anchors.leftMargin: 20
            text: root.title
            font.pixelSize: Theme.fontSize("body")
            font.weight: Font.DemiBold
            color: Theme.color("textPrimary")
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 12

        Text {
            text: qsTr("Enter an application ID or pick from the list of apps that already have overrides.")
            font.pixelSize: Theme.fontSize("sm")
            color: Theme.color("textSecondary")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        TextField {
            id: appIdField
            Layout.fillWidth: true
            placeholderText: qsTr("e.g. firefox  or  org.gnome.Nautilus")
            font.pixelSize: Theme.fontSize("sm")
            font.family: "monospace"
            text: root.selectedAppId
            onTextChanged: root.selectedAppId = text.trim()
            background: Rectangle {
                radius: Theme.radiusControl
                color: Theme.color("surface")
                border.color: parent.activeFocus ? Theme.color("accent") : Theme.color("panelBorder")
                border.width: parent.activeFocus ? 2 : 1
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        // Existing apps with overrides
        ListView {
            id: appListView
            Layout.fillWidth: true
            implicitHeight: Math.min(contentHeight, 180)
            clip: true
            visible: PerAppOverrides.appList.length > 0
            model: PerAppOverrides.appList

            delegate: ItemDelegate {
                required property string modelData
                width: appListView.width
                implicitHeight: 36
                text: modelData
                font.family: "monospace"
                font.pixelSize: Theme.fontSize("sm")
                highlighted: modelData === root.selectedAppId
                onClicked: {
                    root.selectedAppId = modelData
                    appIdField.text    = modelData
                }
                background: Rectangle {
                    color: parent.highlighted
                           ? Theme.color("accent")
                           : (parent.hovered ? Theme.color("hoverOverlay") : "transparent")
                    radius: Theme.radiusControl
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.highlighted ? Theme.color("accentText") : Theme.color("textPrimary")
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                implicitHeight: 32
                onClicked: root.reject()
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.hovered ? Theme.color("hoverOverlay") : "transparent"
                    border.color: Theme.color("panelBorder"); border.width: 1
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textPrimary")
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
            Button {
                text: qsTr("Select")
                implicitHeight: 32
                enabled: root.selectedAppId.length > 0
                onClicked: {
                    root.accepted(root.selectedAppId)
                    root.close()
                }
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.enabled
                           ? (parent.hovered ? Theme.color("accentHover") : Theme.color("accent"))
                           : Theme.color("panelBorder")
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: Theme.fontSize("sm")
                    color: parent.enabled ? Theme.color("accentText") : Theme.color("textDisabled")
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
