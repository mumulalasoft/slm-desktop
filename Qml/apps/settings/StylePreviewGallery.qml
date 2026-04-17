import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

ApplicationWindow {
    id: window
    visible: true
    width: 980
    height: 700
    minimumWidth: 860
    minimumHeight: 620
    title: qsTr("Style Preview Gallery")
    color: Theme.color("windowBg")

    property int comboIndex: 0
    property bool swOn: true
    property bool checkOn: true
    property bool busy: true
    property real sliderValue: 58
    property string fieldValue: "Big Sur style"
    property int tabIndex: 0
    property int navTabIndex: 1

    function applyThemeMode(mode) {
        if (!DesktopSettings || !DesktopSettings.setThemeMode) {
            return
        }
        DesktopSettings.setThemeMode(mode)
        syncThemePreferences()
    }

    function syncThemePreferences() {
        Theme.applyModeString(DesktopSettings.themeMode)
        Theme.userAccentColor = DesktopSettings.accentColor
        Theme.userFontScale = DesktopSettings.fontScale
        DSStyle.Theme.applyModeString(DesktopSettings.themeMode)
        DSStyle.Theme.userAccentColor = DesktopSettings.accentColor
        DSStyle.Theme.userFontScale = DesktopSettings.fontScale
    }

    Component.onCompleted: {
        syncThemePreferences()
    }

    Connections {
        target: DesktopSettings
        function onThemeModeChanged() { window.syncThemePreferences() }
        function onAccentColorChanged() { window.syncThemePreferences() }
        function onFontScaleChanged() { window.syncThemePreferences() }
    }

    header: ToolBar {
        contentHeight: 52
        background: Rectangle {
            color: Theme.color("panelBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            DSStyle.Label {
                text: qsTr("Style Preview")
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("semibold")
            }

            Item { Layout.fillWidth: true }

            DSStyle.Button {
                text: qsTr("Light")
                defaultAction: DesktopSettings.themeMode === "light"
                onClicked: window.applyThemeMode("light")
            }
            DSStyle.Button {
                text: qsTr("Dark")
                defaultAction: DesktopSettings.themeMode === "dark"
                onClicked: window.applyThemeMode("dark")
            }
            DSStyle.Button {
                text: qsTr("Auto")
                defaultAction: DesktopSettings.themeMode !== "light" && DesktopSettings.themeMode !== "dark"
                onClicked: window.applyThemeMode("auto")
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 14

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    DSStyle.Label {
                        text: qsTr("Theme State")
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("semibold")
                    }

                    DSStyle.Label {
                        text: qsTr("Mode: %1 | Dark active: %2")
                                .arg(DesktopSettings.themeMode)
                                .arg(Theme.darkMode ? "yes" : "no")
                        color: Theme.color("textSecondary")
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    DSStyle.Label {
                        text: qsTr("Controls")
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("semibold")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        DSStyle.TextField {
                            Layout.fillWidth: true
                            text: window.fieldValue
                            placeholderText: qsTr("Input text")
                            onTextChanged: window.fieldValue = text
                        }
                        DSStyle.ComboBox {
                            Layout.preferredWidth: 180
                            model: [qsTr("Compact"), qsTr("Regular"), qsTr("Large")]
                            currentIndex: window.comboIndex
                            onActivated: window.comboIndex = currentIndex
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        DSStyle.Button { text: qsTr("Cancel") }
                        DSStyle.Button {
                            text: qsTr("OK")
                            defaultAction: true
                        }
                        Item { Layout.fillWidth: true }
                        DSStyle.Switch {
                            checked: window.swOn
                            onToggled: window.swOn = checked
                        }
                        DSStyle.CheckBox {
                            text: qsTr("Enabled")
                            checked: window.checkOn
                            onToggled: window.checkOn = checked
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        DSStyle.Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: window.sliderValue
                            onMoved: window.sliderValue = value
                        }
                        DSStyle.Label {
                            text: Math.round(window.sliderValue).toString() + "%"
                            color: Theme.color("textSecondary")
                        }
                    }

                    DSStyle.ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: window.sliderValue
                        indeterminate: window.busy
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    DSStyle.Label {
                        text: qsTr("Navigation")
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("semibold")
                    }

                    Breadcrumb {
                        Layout.fillWidth: true
                        model: [qsTr("System"), qsTr("Settings"), qsTr("Appearance")]
                        currentIndex: 2
                    }

                    DSStyle.TabBar {
                        Layout.fillWidth: true
                        currentIndex: window.navTabIndex
                        onCurrentIndexChanged: window.navTabIndex = currentIndex

                        DSStyle.TabButton { text: qsTr("General") }
                        DSStyle.TabButton { text: qsTr("Display") }
                        DSStyle.TabButton { text: qsTr("Advanced") }
                    }

                    DSStyle.TabBar {
                        Layout.fillWidth: true
                        currentIndex: window.tabIndex
                        onCurrentIndexChanged: window.tabIndex = currentIndex

                        DSStyle.TabButton { text: qsTr("General") }
                        DSStyle.TabButton { text: qsTr("Appearance") }
                        DSStyle.TabButton { text: qsTr("About") }
                    }

                    DSStyle.Label {
                        text: [qsTr("General page preview"),
                               qsTr("Appearance page preview"),
                               qsTr("About page preview")][window.tabIndex]
                        color: Theme.color("textSecondary")
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    DSStyle.Label {
                        text: qsTr("State Matrix")
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("semibold")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        DSStyle.Button { text: qsTr("Normal") }
                        DSStyle.Button { text: qsTr("Default"); defaultAction: true }
                        DSStyle.Button { text: qsTr("Disabled"); enabled: false }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        DSStyle.TextField { Layout.fillWidth: true; placeholderText: qsTr("Enabled input") }
                        DSStyle.TextField { Layout.fillWidth: true; placeholderText: qsTr("Disabled input"); enabled: false }
                    }
                }
            }
        }
    }
}
