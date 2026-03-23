import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "."

ApplicationWindow {
    id: window
    visible: true
    width: 1120
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    title: qsTr("System Settings")
    color: "#f4f5f7"

    property string searchText: SearchEngine.searchQuery

    Shortcut {
        sequence: StandardKey.Find
        onActivated: sidebar.forceSearchFocus()
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: SettingsApp.commandPaletteVisible = !SettingsApp.commandPaletteVisible
    }

    Shortcut {
        sequence: "Escape"
        onActivated: SettingsApp.commandPaletteVisible = false
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Sidebar {
            id: sidebar
            SplitView.preferredWidth: 280
            SplitView.minimumWidth: 240
            SplitView.maximumWidth: 360
            query: window.searchText
            moduleModel: query.trim().length > 0 ? SearchEngine.sidebarModules : ModuleLoader.modules
            currentModuleId: SettingsApp.currentModuleId
            onQueryChangedByUser: function(text) {
                SearchEngine.searchQuery = text
            }
            onModuleSelected: function(id) {
                SettingsApp.openModule(id)
            }
        }

        ContentPanel {
            id: contentPanel
            SplitView.fillWidth: true
            query: window.searchText
            onQueryChangedByUser: function(text) {
                SearchEngine.searchQuery = text
            }
        }
    }

    Label {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 12
        anchors.bottomMargin: 10
        z: 90
        text: "search " + SearchEngine.lastSearchLatencyMs + "ms • results " + SearchEngine.lastSearchResultCount
              + " • module " + SettingsApp.lastModuleOpenLatencyMs + "ms"
              + " • deeplink " + SettingsApp.lastDeepLinkLatencyMs + "ms"
        color: "#6b7280"
        font.pixelSize: 11
    }

    Rectangle {
        anchors.fill: parent
        color: "#55000000"
        visible: SettingsApp.commandPaletteVisible
        z: 100

        MouseArea {
            anchors.fill: parent
            onClicked: SettingsApp.commandPaletteVisible = false
        }

        Pane {
            id: commandPalette
            width: Math.min(window.width * 0.72, 760)
            height: Math.min(window.height * 0.62, 520)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 80
            z: 101
            padding: 14

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                TextField {
                    id: commandSearch
                    Layout.fillWidth: true
                    placeholderText: qsTr("Command palette")
                    text: window.searchText
                    selectByMouse: true
                    onTextChanged: SearchEngine.searchQuery = text
                    Component.onCompleted: forceActiveFocus()
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: SearchEngine.results
                    currentIndex: 0
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        text: modelData.title
                        highlighted: ListView.isCurrentItem
                        onClicked: {
                            SettingsApp.commandPaletteVisible = false
                            SettingsApp.openDeepLink(modelData.action)
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            Image {
                                source: "image://icon/" + (modelData.icon || "preferences-system")
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                            }
                            ColumnLayout {
                                spacing: 0
                                Text {
                                    text: modelData.title
                                    font.pixelSize: 14
                                    color: "#1f2937"
                                }
                                Text {
                                    text: modelData.subtitle
                                    font.pixelSize: 11
                                    color: "#6b7280"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
