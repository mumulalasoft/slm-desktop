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
                                         ? Number(Theme.radiusWindow) : 16
    readonly property real fieldRadius: (typeof Theme !== "undefined"
                                         && Theme
                                         && Theme.radiusCard !== undefined)
                                        ? Number(Theme.radiusCard) : 10

    signal queryChanged(string text)
    signal collapseRequested()
    signal focusGridRequested()

    implicitHeight: 74

    Rectangle {
        anchors.fill: parent
        radius: root.headerRadius
        color: Theme.color("windowCard")
        // border.width: Theme.borderWidthThin
        // border.color: Theme.color("windowCardBorder")
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        spacing: 12

        Label {
            text: root.title
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("semibold")
            color: Theme.color("textPrimary")
        }

        // ColumnLayout {
        //     Layout.alignment: Qt.AlignVCenter
        //     spacing: 1



            // Label {
            //     text: root.searchText.length > 0
            //           ? (String(root.filteredCount) + " hasil")
            //           : (String(root.totalCount) + " aplikasi")
            //     font.pixelSize: Theme.fontSize("small")
            //     color: Theme.color("textSecondary")
            //     opacity: Theme.opacityMuted
            // }
        // }

        // Item { Layout.fillWidth: true }

        Rectangle {
            Layout.preferredWidth: Math.min(380, Math.max(210, root.width * 0.34))
            Layout.preferredHeight: 42
            radius: root.fieldRadius
            color: Theme.color("panelBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            TextField {
                id: searchField
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 34
                placeholderText: "Filter aplikasi..."
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

        // ToolButton {
        //     Layout.preferredWidth: 36
        //     Layout.preferredHeight: 36
        //     text: "x"
        //     onClicked: root.collapseRequested()
        // }
    }
}
