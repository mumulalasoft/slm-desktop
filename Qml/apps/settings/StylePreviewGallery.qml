import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

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

    function applyThemeMode(mode) {
        if (!UIPreferences) {
            return
        }
        UIPreferences.setThemeMode(mode)
        Theme.applyModeString(UIPreferences.themeMode)
    }

    Component.onCompleted: {
        Theme.applyModeString(UIPreferences.themeMode)
        Theme.userAccentColor = UIPreferences.accentColor
        Theme.userFontScale = UIPreferences.fontScale
    }

    Connections {
        target: UIPreferences
        function onThemeModeChanged() { Theme.applyModeString(UIPreferences.themeMode) }
        function onAccentColorChanged() { Theme.userAccentColor = UIPreferences.accentColor }
        function onFontScaleChanged() { Theme.userFontScale = UIPreferences.fontScale }
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

            Label {
                text: qsTr("Style Preview")
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("semibold")
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Light")
                defaultAction: UIPreferences.themeMode === "light"
                onClicked: window.applyThemeMode("light")
            }
            Button {
                text: qsTr("Dark")
                defaultAction: UIPreferences.themeMode === "dark"
                onClicked: window.applyThemeMode("dark")
            }
            Button {
                text: qsTr("Auto")
                defaultAction: UIPreferences.themeMode !== "light" && UIPreferences.themeMode !== "dark"
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

                    Label {
                        text: qsTr("Theme State")
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("semibold")
                    }

                    Label {
                        text: qsTr("Mode: %1 | Dark active: %2")
                                .arg(UIPreferences.themeMode)
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

                    Label {
                        text: qsTr("Controls")
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("semibold")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        TextField {
                            Layout.fillWidth: true
                            text: window.fieldValue
                            placeholderText: qsTr("Input text")
                            onTextChanged: window.fieldValue = text
                        }
                        ComboBox {
                            Layout.preferredWidth: 180
                            model: [qsTr("Compact"), qsTr("Regular"), qsTr("Large")]
                            currentIndex: window.comboIndex
                            onActivated: window.comboIndex = currentIndex
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Button { text: qsTr("Cancel") }
                        Button {
                            text: qsTr("OK")
                            defaultAction: true
                        }
                        Item { Layout.fillWidth: true }
                        Switch {
                            checked: window.swOn
                            onToggled: window.swOn = checked
                        }
                        CheckBox {
                            text: qsTr("Enabled")
                            checked: window.checkOn
                            onToggled: window.checkOn = checked
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: window.sliderValue
                            onMoved: window.sliderValue = value
                        }
                        Label {
                            text: Math.round(window.sliderValue).toString() + "%"
                            color: Theme.color("textSecondary")
                        }
                    }

                    ProgressBar {
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

                    Label {
                        text: qsTr("Tabs")
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("semibold")
                    }

                    TabBar {
                        Layout.fillWidth: true
                        currentIndex: window.tabIndex
                        onCurrentIndexChanged: window.tabIndex = currentIndex

                        TabButton { text: qsTr("General") }
                        TabButton { text: qsTr("Appearance") }
                        TabButton { text: qsTr("About") }
                    }

                    Label {
                        text: [qsTr("General page preview"),
                               qsTr("Appearance page preview"),
                               qsTr("About page preview")][window.tabIndex]
                        color: Theme.color("textSecondary")
                    }
                }
            }
        }
    }
}
