import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

Rectangle {
    id: root

    required property var hostRoot
    required property var tabModel

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 4

        Rectangle {
            id: addTabPill
            Layout.preferredWidth: 24
            Layout.preferredHeight: 20
            radius: Theme.radiusMd
            color: addTabMouse.containsMouse ? Theme.color("fileManagerTabCloseHover") : Theme.color("fileManagerControlBg")
            border.width: Theme.borderWidthNone
            border.color: Theme.color("fileManagerTabsBorder")

            Text {
                anchors.centerIn: parent
                text: "+"
                color: Theme.color("textPrimary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("subtitle")
                font.weight: Theme.fontWeight("medium")
            }

            MouseArea {
                id: addTabMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: hostRoot.addTab(
                               hostRoot.fileModel
                               && hostRoot.fileModel.currentPath ? String(hostRoot.fileModel.currentPath) : "~",
                               true)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            color: "transparent"
            clip: true

            ListView {
                id: contentTabsView
                anchors.fill: parent
                orientation: ListView.Horizontal
                spacing: 6
                clip: true
                model: tabModel
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    required property int index
                    required property string path
                    readonly property int tabCountValue: Math.max(1, Number(tabModel.count || 1))
                    readonly property real availableTabsWidth: Math.max(
                                                                   120, contentTabsView.width - Math.max(0, (tabCountValue - 1) * contentTabsView.spacing))
                    width: Math.max(96, Math.floor(availableTabsWidth / tabCountValue))
                    height: 22
                    radius: Theme.radiusControl
                    color: hostRoot.activeTabIndex === index ? Theme.color("fileManagerTabActive") : "transparent"
                    border.width: Theme.borderWidthNone
                    border.color: Theme.color("fileManagerTabsBorder")

                    Text {
                        anchors.centerIn: parent
                        text: hostRoot.tabTitleFromPath(String(path || "~"))
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width - (tabCloseButton.visible ? 24 : 12)
                    }

                    Rectangle {
                        id: tabCloseButton
                        visible: (tabModel.count > 1)
                                 && (hostRoot.activeTabIndex === index)
                        width: 12
                        height: 12
                        radius: Theme.radiusMdPlus
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        color: tabCloseMouse.containsMouse ? Theme.color("fileManagerTabCloseHover") : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "\u00d7"
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("xs")
                            color: Theme.color("textPrimary")
                        }

                        MouseArea {
                            id: tabCloseMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: tabCloseButton.visible
                            onClicked: function(mouse) {
                                mouse.accepted = true
                                hostRoot.closeTab(index)
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.MiddleButton) {
                                hostRoot.closeTab(index)
                                return
                            }
                            hostRoot.switchToTab(index)
                        }
                    }
                }
            }
        }
    }
}
