import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#eceff3"
    border.color: "#d8dde4"
    border.width: 1

    property alias query: searchField.text
    property var moduleModel: []
    property string currentModuleId: ""
    signal moduleSelected(string id)
    signal queryChangedByUser(string text)

    function forceSearchFocus() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search settings")
            selectByMouse: true
            onTextChanged: root.queryChangedByUser(text)
            background: Rectangle {
                radius: 10
                color: "#ffffff"
                border.color: searchField.activeFocus ? "#4f8ff7" : "#cad2dc"
                border.width: searchField.activeFocus ? 2 : 1
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: root.moduleModel
            section.property: "group"
            section.criteria: ViewSection.FullString
            section.delegate: Rectangle {
                width: listView.width
                height: 28
                color: "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: section
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: "#6b7280"
                    textFormat: Text.PlainText
                }
            }

            delegate: ItemDelegate {
                width: listView.width
                height: 42
                highlighted: root.currentModuleId === (modelData.id || "")
                background: Rectangle {
                    radius: 8
                    color: highlighted ? "#3076f3" : (hovered ? "#dce3ec" : "transparent")
                    Behavior on color {
                        ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
                    }
                }

                contentItem: RowLayout {
                    spacing: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10

                    Image {
                        source: "image://icon/" + (modelData.icon || "preferences-system")
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.name || ""
                        font.pixelSize: 13
                        font.weight: highlighted ? Font.DemiBold : Font.Normal
                        color: highlighted ? "white" : "#111827"
                        elide: Text.ElideRight
                        Behavior on color {
                            ColorAnimation { duration: 120; easing.type: Easing.OutQuad }
                        }
                    }
                }

                onClicked: root.moduleSelected(modelData.id || "")
            }
        }
    }
}
