import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop

ApplicationWindow {
    id: window
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    width: 1120
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    title: qsTr("System Settings")
    color: Theme.color("windowBg")

    property string searchText: SearchEngine.searchQuery

    // Wire UIPreferences → Theme on startup and on changes.
    Component.onCompleted: {
        Theme.applyModeString(UIPreferences.themeMode)
        Theme.userAccentColor = UIPreferences.accentColor
        Theme.userFontScale   = UIPreferences.fontScale
    }

    Connections {
        target: UIPreferences
        function onThemeModeChanged()  { Theme.applyModeString(UIPreferences.themeMode) }
        function onAccentColorChanged() { Theme.userAccentColor = UIPreferences.accentColor }
        function onFontScaleChanged()  { Theme.userFontScale   = UIPreferences.fontScale  }
    }

    Shortcut {
        sequences: [StandardKey.Find]
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
        color: Theme.color("textDisabled")
        font.pixelSize: Theme.fontSize("xs")
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color("overlay")
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
                                    font.pixelSize: Theme.fontSize("body")
                                    color: Theme.color("textPrimary")
                                }
                                Text {
                                    text: modelData.subtitle
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textSecondary")
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
