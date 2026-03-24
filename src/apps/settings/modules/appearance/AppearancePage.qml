import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 48
    clip: true

    readonly property var accentPresets: [
        "#0a84ff", "#34c759", "#ff9500", "#ff3b30", "#af52de", "#ff6b81", "#5ac8fa"
    ]

    // Build a ComboBox model list with a leading "(System default)" sentinel
    // that maps to an empty stored value.
    function themeModel(list) {
        return [qsTr("(System default)")].concat(list)
    }

    // Return the ComboBox index for a stored theme name.
    // Index 0 = sentinel (no override / empty string stored).
    function themeIndex(list, stored) {
        if (!stored || stored.length === 0) return 0
        const idx = list.indexOf(stored)
        return idx >= 0 ? idx + 1 : 0
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 24

        SettingGroup {
            title: qsTr("Appearance")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Color Theme")
                description: qsTr("Choose how the system looks.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8

                    Repeater {
                        model: [
                            { key: "light",  label: qsTr("Light") },
                            { key: "dark",   label: qsTr("Dark")  },
                            { key: "auto",   label: qsTr("Auto")  }
                        ]
                        delegate: Button {
                            required property var modelData
                            text: modelData.label
                            highlighted: UIPreferences.themeMode === modelData.key
                            onClicked: UIPreferences.setThemeMode(modelData.key)
                        }
                    }
                }
            }

            SettingCard {
                label: qsTr("Accent Color")
                description: qsTr("Change the system accent color.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8

                    Repeater {
                        model: root.accentPresets
                        delegate: Rectangle {
                            required property string modelData
                            width: 26; height: 26; radius: 13
                            color: modelData
                            border.color: UIPreferences.accentColor === modelData
                                ? Theme.color("textPrimary") : "transparent"
                            border.width: 2

                            MouseArea {
                                anchors.fill: parent
                                onClicked: UIPreferences.setAccentColor(modelData)
                            }
                        }
                    }
                }
            }

            SettingCard {
                label: qsTr("Font Size")
                description: qsTr("Scale the interface text size.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 12

                    Text {
                        text: qsTr("A")
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                    }

                    Slider {
                        id: fontScaleSlider
                        from: 0.8
                        to: 1.5
                        stepSize: 0.05
                        value: UIPreferences.fontScale
                        Layout.preferredWidth: 180
                        onMoved: UIPreferences.setFontScale(value)
                    }

                    Text {
                        text: qsTr("A")
                        font.pixelSize: Theme.fontSize("title")
                        color: Theme.color("textSecondary")
                    }

                    Text {
                        text: Math.round(UIPreferences.fontScale * 100) + "%"
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                        Layout.preferredWidth: 36
                    }
                }
            }
        }

        SettingGroup {
            title: qsTr("Theme")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("GTK Theme — Light")
                description: qsTr("Applied to GTK 3/4 applications when light mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.gtkThemes)
                    currentIndex: root.themeIndex(ThemeManager.gtkThemes, ThemeManager.gtkThemeLight)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.gtkThemes[currentIndex - 1]
                        ThemeManager.setGtkThemeLight(name)
                    }
                }
            }

            SettingCard {
                label: qsTr("GTK Theme — Dark")
                description: qsTr("Applied to GTK 3/4 applications when dark mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.gtkThemes)
                    currentIndex: root.themeIndex(ThemeManager.gtkThemes, ThemeManager.gtkThemeDark)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.gtkThemes[currentIndex - 1]
                        ThemeManager.setGtkThemeDark(name)
                    }
                }
            }

            SettingCard {
                label: qsTr("KDE Color Scheme — Light")
                description: qsTr("Applied to KDE / Qt applications when light mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.kdeColorSchemes)
                    currentIndex: root.themeIndex(ThemeManager.kdeColorSchemes, ThemeManager.kdeColorSchemeLight)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.kdeColorSchemes[currentIndex - 1]
                        ThemeManager.setKdeColorSchemeLight(name)
                    }
                }
            }

            SettingCard {
                label: qsTr("KDE Color Scheme — Dark")
                description: qsTr("Applied to KDE / Qt applications when dark mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.kdeColorSchemes)
                    currentIndex: root.themeIndex(ThemeManager.kdeColorSchemes, ThemeManager.kdeColorSchemeDark)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.kdeColorSchemes[currentIndex - 1]
                        ThemeManager.setKdeColorSchemeDark(name)
                    }
                }
            }

            Label {
                visible: ThemeManager.gtkThemes.length === 0 && ThemeManager.kdeColorSchemes.length === 0
                text: qsTr("No themes found. Install GTK or KDE themes in an XDG data directory.")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 4
            }
        }

        SettingGroup {
            title: qsTr("Icon Theme")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("GTK Icon Theme — Light")
                description: qsTr("Applied to GTK/system icons when light mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.iconThemes)
                    currentIndex: root.themeIndex(ThemeManager.iconThemes, ThemeManager.gtkIconThemeLight)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                        ThemeManager.setGtkIconThemeLight(name)
                    }
                }
            }

            SettingCard {
                label: qsTr("GTK Icon Theme — Dark")
                description: qsTr("Applied to GTK/system icons when dark mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.iconThemes)
                    currentIndex: root.themeIndex(ThemeManager.iconThemes, ThemeManager.gtkIconThemeDark)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                        ThemeManager.setGtkIconThemeDark(name)
                    }
                }
            }

            SettingCard {
                label: qsTr("KDE Icon Theme — Light")
                description: qsTr("Applied to KDE / Qt applications when light mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.iconThemes)
                    currentIndex: root.themeIndex(ThemeManager.iconThemes, ThemeManager.kdeIconThemeLight)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                        ThemeManager.setKdeIconThemeLight(name)
                    }
                }
            }

            SettingCard {
                label: qsTr("KDE Icon Theme — Dark")
                description: qsTr("Applied to KDE / Qt applications when dark mode is active.")
                Layout.fillWidth: true

                ComboBox {
                    model: root.themeModel(ThemeManager.iconThemes)
                    currentIndex: root.themeIndex(ThemeManager.iconThemes, ThemeManager.kdeIconThemeDark)
                    Layout.preferredWidth: 220
                    onActivated: {
                        const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                        ThemeManager.setKdeIconThemeDark(name)
                    }
                }
            }

            Label {
                visible: ThemeManager.iconThemes.length === 0
                text: qsTr("No icon themes found. Install icon themes in an XDG data directory.")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 4
            }
        }

        SettingGroup {
            id: fontsGroup
            title: qsTr("Fonts")
            Layout.fillWidth: true

            // ── Font picker popup ────────────────────────────────────────────
            FontPickerDialog {
                id: fontPicker
                property string _role: ""
                onFontPicked: function(spec) {
                    if      (_role === "default")   FontManager.setDefaultFont(spec)
                    else if (_role === "document")  FontManager.setDocumentFont(spec)
                    else if (_role === "monospace") FontManager.setMonospaceFont(spec)
                    else if (_role === "titlebar")  FontManager.setTitlebarFont(spec)
                }
            }

            function openPicker(role, spec) {
                fontPicker._role = role
                fontPicker.initialSpec = spec
                fontPicker.open()
            }

            function fontLabel(spec) {
                if (!spec || spec.length === 0) return qsTr("(System default)")
                return FontManager.displayLabel(spec)
            }

            SettingCard {
                label: qsTr("Default Font")
                description: qsTr("Used by most applications for UI text.")
                Layout.fillWidth: true

                Button {
                    text: fontsGroup.fontLabel(FontManager.defaultFont)
                    font.pixelSize: Theme.fontSize("small")
                    Layout.preferredWidth: 220
                    onClicked: fontsGroup.openPicker("default", FontManager.defaultFont)
                }
            }

            SettingCard {
                label: qsTr("Document Font")
                description: qsTr("Used by document and text-editing applications.")
                Layout.fillWidth: true

                Button {
                    text: fontsGroup.fontLabel(FontManager.documentFont)
                    font.pixelSize: Theme.fontSize("small")
                    Layout.preferredWidth: 220
                    onClicked: fontsGroup.openPicker("document", FontManager.documentFont)
                }
            }

            SettingCard {
                label: qsTr("Monospace Font")
                description: qsTr("Used by terminals and code editors.")
                Layout.fillWidth: true

                Button {
                    text: fontsGroup.fontLabel(FontManager.monospaceFont)
                    font.family: FontManager.monospaceFont.length > 0
                                 ? FontManager.monospaceFont.split(",")[0] : Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("small")
                    Layout.preferredWidth: 220
                    onClicked: fontsGroup.openPicker("monospace", FontManager.monospaceFont)
                }
            }

            SettingCard {
                label: qsTr("Titlebar Font")
                description: qsTr("Used for window title bars.")
                Layout.fillWidth: true

                Button {
                    text: fontsGroup.fontLabel(FontManager.titlebarFont)
                    font.pixelSize: Theme.fontSize("small")
                    Layout.preferredWidth: 220
                    onClicked: fontsGroup.openPicker("titlebar", FontManager.titlebarFont)
                }
            }

            SettingCard {
                label: qsTr("Font Scale")
                description: qsTr("Scale factor applied to all UI fonts.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 12

                    Slider {
                        id: fontScaleSlider2
                        from: 0.9
                        to: 1.5
                        stepSize: 0.05
                        value: UIPreferences.fontScale
                        Layout.preferredWidth: 180
                        onMoved: UIPreferences.setFontScale(value)
                    }

                    Text {
                        text: Math.round(UIPreferences.fontScale * 100) + "%"
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                        Layout.preferredWidth: 40
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.rightMargin: 4
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Reset to Defaults")
                    onClicked: FontManager.resetToDefaults()
                }
            }
        }

        SettingGroup {
            title: qsTr("Dock")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Animation Style")
                description: qsTr("Controls how dock icons animate when launching apps.")
                Layout.fillWidth: true

                ComboBox {
                    model: [qsTr("Lively"), qsTr("Subtle")]
                    currentIndex: UIPreferences.dockMotionPreset === "subtle" ? 1 : 0
                    Layout.preferredWidth: 160
                    onActivated: UIPreferences.setDockMotionPreset(
                        currentIndex === 0 ? "macos-lively" : "subtle")
                }
            }

            SettingCard {
                label: qsTr("Auto-hide")
                description: qsTr("Automatically hide the dock when not in use.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: UIPreferences.dockAutoHideEnabled
                    onToggled: UIPreferences.setDockAutoHideEnabled(checked)
                }
            }

            SettingCard {
                label: qsTr("Drop Pulse")
                description: qsTr("Animate dock items when a file is dragged over them.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: UIPreferences.dockDropPulseEnabled
                    onToggled: UIPreferences.setDockDropPulseEnabled(checked)
                }
            }

            SettingCard {
                label: qsTr("Drag Sensitivity (Mouse)")
                description: qsTr("Minimum pointer movement before a dock drag begins.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    Slider {
                        from: 2; to: 24; stepSize: 1
                        value: UIPreferences.dockDragThresholdMouse
                        Layout.preferredWidth: 160
                        onMoved: UIPreferences.setDockDragThresholdMouse(Math.round(value))
                    }
                    Text {
                        text: UIPreferences.dockDragThresholdMouse + qsTr(" px")
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                        Layout.preferredWidth: 40
                    }
                }
            }

            SettingCard {
                label: qsTr("Drag Sensitivity (Touchpad)")
                description: qsTr("Minimum movement before a dock drag begins on touchpad.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    Slider {
                        from: 2; to: 24; stepSize: 1
                        value: UIPreferences.dockDragThresholdTouchpad
                        Layout.preferredWidth: 160
                        onMoved: UIPreferences.setDockDragThresholdTouchpad(Math.round(value))
                    }
                    Text {
                        text: UIPreferences.dockDragThresholdTouchpad + qsTr(" px")
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                        Layout.preferredWidth: 40
                    }
                }
            }
        }

        SettingGroup {
            title: qsTr("Desktop")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Wallpaper")
                description: qsTr("Personalize your desktop background.")
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 12
                    Layout.fillWidth: true

                    Rectangle {
                        width: 192
                        height: 108
                        radius: Theme.radiusSm
                        color: Theme.color("controlDisabledBg")
                        clip: true
                        visible: currentUri.length > 0

                        property string currentUri: WallpaperManager.currentWallpaperUri

                        Image {
                            anchors.fill: parent
                            source: parent.currentUri
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                            mipmap: true
                        }
                    }

                    RowLayout {
                        spacing: 8

                        Button {
                            text: WallpaperManager.loading
                                ? qsTr("Opening…")
                                : qsTr("Choose Picture…")
                            enabled: !WallpaperManager.loading
                            onClicked: WallpaperManager.openFilePicker()
                        }

                        Label {
                            text: {
                                const uri = WallpaperManager.currentWallpaperUri
                                if (!uri) return ""
                                const parts = uri.split('/')
                                return parts[parts.length - 1]
                            }
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Label {
                        id: wallpaperError
                        visible: false
                        color: Theme.color("error")
                        font.pixelSize: Theme.fontSize("small")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    Connections {
        target: WallpaperManager
        function onErrorOccurred(message) {
            wallpaperError.text = message
            wallpaperError.visible = true
        }
        function onWallpaperChanged() {
            wallpaperError.visible = false
        }
    }
}
