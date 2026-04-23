import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    property string title: "AppHub"
    property string searchText: ""
    property int totalCount: 0
    property int filteredCount: 0
    readonly property real headerRadius: (typeof Theme !== "undefined"
                                          && Theme
                                          && Theme.radiusWindow !== undefined)
                                         ? Math.min(8, Number(Theme.radiusWindow)) : 8
    readonly property real fieldRadius: (typeof Theme !== "undefined"
                                         && Theme
                                         && Theme.radiusCard !== undefined)
                                        ? Math.min(8, Number(Theme.radiusCard)) : 8

    signal queryChanged(string text)
    signal collapseRequested()
    signal focusGridRequested()

    implicitHeight: 72

    Rectangle {
        anchors.fill: parent
        radius: root.headerRadius
        color: "transparent"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 14

        ColumnLayout {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 168
            spacing: 2

            Label {
                text: root.title
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("bold")
                color: Theme.color("textPrimary")
            }

            Label {
                text: root.searchText.length > 0
                      ? (String(root.filteredCount) + " results")
                      : (String(root.totalCount) + " apps")
                font.pixelSize: Theme.fontSize("small")
                color: Theme.color("textSecondary")
                opacity: Theme.opacityMuted
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 520
            Layout.preferredHeight: 40
            radius: root.fieldRadius
            color: Theme.color("apphubSearchBg")
            border.width: Theme.borderWidthThin
            border.color: searchField.activeFocus ? Theme.color("focusRing") : Theme.color("apphubSearchBorder")

            Item {
                width: 16
                height: 16
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                opacity: Theme.opacityMuted

                Rectangle {
                    width: 10
                    height: 10
                    radius: Theme.radiusSmPlus
                    anchors.left: parent.left
                    anchors.top: parent.top
                    color: "transparent"
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("textSecondary")
                }

                Rectangle {
                    width: 6
                    height: 1
                    radius: Theme.radiusHairline
                    x: 9
                    y: 11
                    rotation: 45
                    color: Theme.color("textSecondary")
                }
            }

            TextField {
                id: searchField
                anchors.fill: parent
                anchors.leftMargin: 34
                anchors.rightMargin: 34
                placeholderText: "Search apps"
                placeholderTextColor: Theme.color("textSecondary")
                color: Theme.color("textPrimary")
                text: root.searchText
                background: null
                selectByMouse: true

                onTextChanged: {
                    if (root.searchText === text) {
                        return
                    }
                    root.searchText = text
                    root.queryChanged(text)
                }

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Down) {
                        root.focusGridRequested()
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Escape) {
                        root.collapseRequested()
                        event.accepted = true
                    }
                }
            }

            ToolButton {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 4
                visible: searchField.text.length > 0
                text: "x"
                onClicked: {
                    searchField.text = ""
                    searchField.forceActiveFocus()
                }
            }
        }

        ToolButton {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            text: "x"
            onClicked: root.collapseRequested()
        }
    }
}
