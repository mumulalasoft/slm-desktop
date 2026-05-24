import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle

Rectangle {
    id: root

    required property var hostRoot
    required property var tabModel

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.metric("spacingXs")
        anchors.rightMargin: Theme.metric("spacingXs")
        spacing: Theme.metric("spacingXxs")

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeightCompact
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
                    height: Theme.controlHeightCompact
                    radius: Theme.radiusControl
                    color: hostRoot.activeTabIndex === index
                           ? (Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.58))
                           : (tabHover.hovered ? Theme.color("controlBgHover") : "transparent")
                    border.width: hostRoot.activeTabIndex === index ? Theme.borderWidthThin : Theme.borderWidthNone
                    border.color: Theme.color("panelBorder")

                    HoverHandler {
                        id: tabHover
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: tabCloseButton.visible ? tabCloseButton.left : parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: tabCloseButton.visible ? 4 : 10
                        text: hostRoot.tabTitleFromPath(String(path || "~"))
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                    }

                    Rectangle {
                        id: tabCloseButton
                        visible: tabModel.count > 1
                        opacity: (hostRoot.activeTabIndex === index || tabHover.hovered) ? 1.0 : 0.0
                        z: 3
                        width: 16
                        height: 16
                        radius: Theme.radiusMdPlus
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        color: tabCloseMouse.containsMouse
                               ? Theme.color("fileManagerTabCloseHover")
                               : (Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08))

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "\u00d7"
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("xs")
                            font.weight: Theme.fontWeight("medium")
                            color: Theme.color("textSecondary")
                        }

                        MouseArea {
                            id: tabCloseMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: function(mouse) {
                                mouse.accepted = true
                                hostRoot.closeTab(index)
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.rightMargin: tabModel.count > 1 ? (tabCloseButton.width + 10) : 0
                        hoverEnabled: false
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

        Rectangle {
            id: addTabPill
            Layout.preferredWidth: 24
            Layout.preferredHeight: Theme.controlHeightCompact
            radius: Theme.radiusMd
            color: addTabMouse.containsMouse ? Theme.color("controlBgHover") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: addTabMouse.containsMouse
                          ? Theme.color("panelBorder")
                          : (Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.10))

            Text {
                anchors.centerIn: parent
                text: "+"
                color: addTabMouse.containsMouse ? Theme.color("textPrimary") : Theme.color("textSecondary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("subtitle")
                font.weight: Theme.fontWeight("regular")
            }

            MouseArea {
                id: addTabMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: hostRoot.addTab(
                               hostRoot.fileModel
                               && hostRoot.fileModel.currentPath ? String(hostRoot.fileModel.currentPath) : "~",
                               true)
            }
        }
    }
}
