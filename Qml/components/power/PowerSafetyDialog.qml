import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

FocusScope {
    id: root

    property string action: "shutdown"
    property var apps: []
    property bool busy: false
    property string titleText: qsTr("Some applications are still open")
    property string descriptionText: qsTr("Unsaved changes may be lost if you continue.")

    signal cancelRequested()
    signal reviewRequested()
    signal forceRequested()

    implicitWidth: 520
    implicitHeight: card.implicitHeight

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 48)
        radius: Theme.radiusWindow
        color: Theme.color("surface")
        border.color: Theme.color("panelBorder")
        border.width: Theme.borderWidthThin

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.titleText
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSize("subtitle")
                font.weight: Theme.fontWeight("semibold")
                wrapMode: Text.WordWrap
            }

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.descriptionText
                horizontalAlignment: Text.AlignHCenter
                color: Theme.color("textSecondary")
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                visible: (root.apps || []).length > 0
                implicitHeight: Math.min(190, appList.implicitHeight + 16)
                radius: Theme.radiusControl
                color: Theme.color("panelBg")
                border.color: Theme.color("panelBorder")
                clip: true

                Column {
                    id: appList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 4

                    Repeater {
                        model: root.apps || []
                        delegate: RowLayout {
                            width: appList.width
                            height: 34
                            spacing: 8
                            Image {
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                source: "image://themeicon/" + String(modelData.icon || "application-x-executable")
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                DSStyle.Label {
                                    Layout.fillWidth: true
                                    text: String(modelData.name || qsTr("Application"))
                                    elide: Text.ElideRight
                                }
                                DSStyle.Label {
                                    Layout.fillWidth: true
                                    visible: String(modelData.title || "").length > 0
                                    text: String(modelData.title || "")
                                    color: Theme.color("textSecondary")
                                    font.pixelSize: Theme.fontSize("caption")
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                DSStyle.Button {
                    text: qsTr("Cancel")
                    focus: true
                    enabled: !root.busy
                    Accessible.name: qsTr("Cancel")
                    onClicked: root.cancelRequested()
                }

                DSStyle.Button {
                    text: qsTr("Review Apps")
                    enabled: !root.busy
                    Accessible.name: qsTr("Review open applications")
                    onClicked: root.reviewRequested()
                }

                Item { Layout.fillWidth: true }

                DSStyle.Button {
                    text: root.action === "restart" ? qsTr("Force Restart")
                          : root.action === "logout" ? qsTr("Force Logout")
                          : qsTr("Force Shutdown")
                    enabled: !root.busy
                    Accessible.name: text
                    onClicked: root.forceRequested()
                }
            }
        }
    }
}
