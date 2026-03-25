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
    color: "transparent"

    property string searchText: SearchEngine ? SearchEngine.searchQuery : ""

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
        onActivated: if (SettingsApp) SettingsApp.commandPaletteVisible = !SettingsApp.commandPaletteVisible
    }

    Shortcut {
        sequence: "Escape"
        onActivated: if (SettingsApp) SettingsApp.commandPaletteVisible = false
    }

    // Outer shadow layers (same pattern as DetachedFileManagerWindow).
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Theme.radiusWindow + 4
        color: Qt.rgba(0, 0, 0, Theme.windowShadowOuterOpacity)
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 3
        radius: Theme.radiusWindow + 2
        color: Qt.rgba(0, 0, 0, Theme.windowShadowInnerOpacity)
    }

    // Rounded window surface — clips content to rounded corners same as filemanager.
    Rectangle {
        id: windowSurface
        anchors.fill: parent
        anchors.margins: Theme.windowShadowMargin
        color: Theme.color("windowBg")
        radius: Theme.radiusWindow
        clip: true
        antialiasing: true
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")

        SplitView {
            anchors.fill: parent
            orientation: Qt.Horizontal

            Sidebar {
                id: sidebar
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 240
                SplitView.maximumWidth: 360
                query: window.searchText
                moduleModel: (SearchEngine && query.trim().length > 0) ? SearchEngine.sidebarModules
                                                                       : (ModuleLoader ? ModuleLoader.modules : [])
                currentModuleId: SettingsApp ? SettingsApp.currentModuleId : ""
                onQueryChangedByUser: function(text) {
                    if (SearchEngine) SearchEngine.searchQuery = text
                }
                onModuleSelected: function(id) {
                    if (SettingsApp) SettingsApp.openModule(id)
                }
            }

            ContentPanel {
                id: contentPanel
                SplitView.fillWidth: true
                query: window.searchText
                onQueryChangedByUser: function(text) {
                    if (SearchEngine) SearchEngine.searchQuery = text
                }
            }
        }

        Label {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 12
            anchors.bottomMargin: 10
            z: 90
            text: SearchEngine
                  ? ("search " + SearchEngine.lastSearchLatencyMs + "ms • results " + SearchEngine.lastSearchResultCount
                     + " • module " + (SettingsApp ? SettingsApp.lastModuleOpenLatencyMs : 0) + "ms"
                     + " • deeplink " + (SettingsApp ? SettingsApp.lastDeepLinkLatencyMs : 0) + "ms")
                  : ""
            color: Theme.color("textDisabled")
            font.pixelSize: Theme.fontSize("xs")
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.color("overlay")
            visible: SettingsApp ? SettingsApp.commandPaletteVisible : false
            z: 100

            MouseArea {
                anchors.fill: parent
                onClicked: if (SettingsApp) SettingsApp.commandPaletteVisible = false
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
                        onTextChanged: if (SearchEngine) SearchEngine.searchQuery = text
                        Component.onCompleted: forceActiveFocus()
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: SearchEngine ? SearchEngine.results : null
                        currentIndex: 0
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            text: modelData.title
                            highlighted: ListView.isCurrentItem
                            onClicked: {
                                if (SettingsApp) {
                                    SettingsApp.commandPaletteVisible = false
                                    SettingsApp.openDeepLink(modelData.action)
                                }
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
}
