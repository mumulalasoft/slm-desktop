import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Item {
    id: root
    anchors.fill: parent

    // ── Helpers ────────────────────────────────────────────────────────────

    readonly property var accentPresets: [
        "#0a84ff", "#34c759", "#ff9500", "#ff3b30", "#af52de", "#ff6b81", "#5ac8fa"
    ]

    function themeModel(list) {
        return [qsTr("(System default)")].concat(list)
    }
    function themeIndex(list, stored) {
        if (!stored || stored.length === 0) return 0
        const idx = list.indexOf(stored)
        return idx >= 0 ? idx + 1 : 0
    }

    // ── Nav model ──────────────────────────────────────────────────────────

    readonly property var pages: [
        { id: "appearance", label: qsTr("Appearance"), icon: "preferences-desktop-theme"    },
        { id: "fonts",      label: qsTr("Fonts"),      icon: "preferences-desktop-font"     },
        { id: "theme",      label: qsTr("Theme"),      icon: "applications-graphics"        },
        { id: "icons",      label: qsTr("Icons"),      icon: "preferences-desktop-icons"    },
        { id: "dock",       label: qsTr("Dock"),        icon: "user-desktop"                 },
        { id: "desktop",    label: qsTr("Desktop"),    icon: "video-display"                },
    ]

    property int currentIndex: 0

    // ── Layout ─────────────────────────────────────────────────────────────

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Left sidebar ───────────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: Theme.borderWidthThin

            ListView {
                id: navList
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                spacing: 2
                model: root.pages

                delegate: ItemDelegate {
                    width: navList.width
                    height: 34
                    padding: 0
                    highlighted: root.currentIndex === index

                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: highlighted
                            ? Theme.color("accent")
                            : (parent.hovered ? Theme.color("controlBgHover") : "transparent")
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 8

                        Image {
                            source: "image://icon/" + modelData.icon
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            smooth: true
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.label
                            font.pixelSize: Theme.fontSize("sm")
                            font.weight: highlighted ? Font.DemiBold : Font.Normal
                            color: highlighted ? Theme.color("accentText") : Theme.color("textPrimary")
                            elide: Text.ElideRight
                            Behavior on color { ColorAnimation { duration: 100 } }
                        }
                    }

                    onClicked: root.currentIndex = index
                }
            }
        }

        // ── Right content ──────────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentIndex

            // ── 0: Appearance ──────────────────────────────────────────────
            Flickable {
                contentHeight: appearanceCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: appearanceCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
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
                                        { key: "light", label: qsTr("Light") },
                                        { key: "dark",  label: qsTr("Dark")  },
                                        { key: "auto",  label: qsTr("Auto")  }
                                    ]
                                    delegate: Button {
                                        required property var modelData
                                        text: modelData.label
                                        highlighted: DesktopSettings.themeMode === modelData.key
                                        onClicked: DesktopSettings.setThemeMode(modelData.key)
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("High Contrast")
                            description: qsTr("Increase contrast for text and UI elements.")
                            Layout.fillWidth: true
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Enable high contrast mode")
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                }
                                SettingToggle {
                                    checked: DesktopSettings.highContrast
                                    onToggled: DesktopSettings.setHighContrast(checked)
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
                                        width: 26; height: 26; radius: width / 2
                                        color: modelData
                                        border.color: DesktopSettings.accentColor === modelData
                                            ? Theme.color("textPrimary") : "transparent"
                                        border.width: Theme.borderWidthThick
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: DesktopSettings.setAccentColor(modelData)
                                        }
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Adaptive UI Automation")
                            description: qsTr("Allow context-aware performance tuning based on power and system load.")
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Auto reduce animations")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: DesktopSettings.contextAutoReduceAnimation
                                        onToggled: DesktopSettings.setContextAutoReduceAnimation(checked)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Auto disable blur")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: DesktopSettings.contextAutoDisableBlur
                                        onToggled: DesktopSettings.setContextAutoDisableBlur(checked)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Auto disable heavy effects")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: DesktopSettings.contextAutoDisableHeavyEffects
                                        onToggled: DesktopSettings.setContextAutoDisableHeavyEffects(checked)
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 2
                                    height: Theme.borderWidthThin
                                    color: Theme.color("panelBorder")
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Time period source")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    ComboBox {
                                        id: contextTimeModeCombo
                                        Layout.preferredWidth: 160
                                        textRole: "label"
                                        model: [
                                            { key: "local", label: qsTr("Local Time") },
                                            { key: "sun", label: qsTr("Sunrise/Sunset") }
                                        ]
                                        currentIndex: DesktopSettings.contextTimeMode === "sun" ? 1 : 0
                                        onActivated: {
                                            const entry = model[currentIndex]
                                            if (entry && entry.key) {
                                                DesktopSettings.setContextTimeMode(entry.key)
                                            }
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    enabled: DesktopSettings.contextTimeMode === "sun"
                                    opacity: enabled ? 1.0 : Theme.opacityHint

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Sunrise hour")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: sunriseHourSpin
                                        from: 0
                                        to: 23
                                        editable: true
                                        value: DesktopSettings.contextTimeSunriseHour
                                        Layout.preferredWidth: 100
                                        onValueModified: DesktopSettings.setContextTimeSunriseHour(value)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    enabled: DesktopSettings.contextTimeMode === "sun"
                                    opacity: enabled ? 1.0 : Theme.opacityHint

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Sunset hour")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: sunsetHourSpin
                                        from: 0
                                        to: 23
                                        editable: true
                                        value: DesktopSettings.contextTimeSunsetHour
                                        Layout.preferredWidth: 100
                                        onValueModified: DesktopSettings.setContextTimeSunsetHour(value)
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Window Controls Position")
                            description: qsTr("Choose whether titlebar buttons appear on the left or right side.")
                            Layout.fillWidth: true
                            ComboBox {
                                Layout.preferredWidth: 220
                                model: [
                                    { key: "left",  label: qsTr("Left (macOS-like)") },
                                    { key: "right", label: qsTr("Right")             }
                                ]
                                textRole: "label"
                                currentIndex: ThemeManager.windowControlsSide === "left" ? 0 : 1
                                onActivated: ThemeManager.setWindowControlsSide(model[currentIndex].key)
                            }
                        }
                    }
                }
            }

            // ── 1: Fonts ───────────────────────────────────────────────────
            Flickable {
                contentHeight: fontsCol.implicitHeight + 48
                clip: true

                // Font picker popup — scoped to this page
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

                ColumnLayout {
                    id: fontsCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
                    spacing: 24

                    SettingGroup {
                        id: fontsGroup
                        title: qsTr("Fonts")
                        Layout.fillWidth: true

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
                                    from: 0.8; to: 1.5; stepSize: 0.05
                                    value: DesktopSettings.fontScale
                                    Layout.preferredWidth: 180
                                    onMoved: DesktopSettings.setFontScale(value)
                                }
                                Text {
                                    text: Math.round(DesktopSettings.fontScale * 100) + "%"
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
                }
            }

            // ── 2: Theme ───────────────────────────────────────────────────
            Flickable {
                contentHeight: themeCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: themeCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
                    spacing: 24

                    SettingGroup {
                        title: qsTr("Theme")
                        Layout.fillWidth: true

                        SettingCard {
                            label: qsTr("GTK Theme — Light")
                            description: qsTr("Applied to GTK 3/4 applications when light mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.gtkThemes)
                                currentIndex: root.themeIndex(ThemeManager.gtkThemes, DesktopSettings.gtkThemeLight)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.gtkThemes[currentIndex - 1]
                                    DesktopSettings.setGtkThemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("GTK Theme — Dark")
                            description: qsTr("Applied to GTK 3/4 applications when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.gtkThemes)
                                currentIndex: root.themeIndex(ThemeManager.gtkThemes, DesktopSettings.gtkThemeDark)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.gtkThemes[currentIndex - 1]
                                    DesktopSettings.setGtkThemeDark(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Color Scheme — Light")
                            description: qsTr("Applied to KDE / Qt applications when light mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.kdeColorSchemes)
                                currentIndex: root.themeIndex(ThemeManager.kdeColorSchemes, DesktopSettings.kdeColorSchemeLight)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.kdeColorSchemes[currentIndex - 1]
                                    DesktopSettings.setKdeColorSchemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Color Scheme — Dark")
                            description: qsTr("Applied to KDE / Qt applications when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.kdeColorSchemes)
                                currentIndex: root.themeIndex(ThemeManager.kdeColorSchemes, DesktopSettings.kdeColorSchemeDark)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.kdeColorSchemes[currentIndex - 1]
                                    DesktopSettings.setKdeColorSchemeDark(name)
                                }
                            }
                        }

                        Label {
                            visible: ThemeManager.gtkThemes.length === 0 && ThemeManager.kdeColorSchemes.length === 0
                            text: qsTr("No themes found. Install GTK or KDE themes in an XDG data directory.")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true; Layout.leftMargin: 4
                        }
                    }
                }
            }

            // ── 3: Icons ───────────────────────────────────────────────────
            Flickable {
                contentHeight: iconsCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: iconsCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
                    spacing: 24

                    SettingGroup {
                        title: qsTr("Icon Theme")
                        Layout.fillWidth: true

                        SettingCard {
                            label: qsTr("Icon Theme — Light")
                            description: qsTr("Applied to system icons when light mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.iconThemes)
                                currentIndex: root.themeIndex(ThemeManager.iconThemes, DesktopSettings.gtkIconThemeLight)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                                    DesktopSettings.setGtkIconThemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Icon Theme — Dark")
                            description: qsTr("Applied to system icons when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.iconThemes)
                                currentIndex: root.themeIndex(ThemeManager.iconThemes, DesktopSettings.gtkIconThemeDark)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                                    DesktopSettings.setGtkIconThemeDark(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Icon Theme — Light")
                            description: qsTr("Applied to KDE / Qt applications when light mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.iconThemes)
                                currentIndex: root.themeIndex(ThemeManager.iconThemes, DesktopSettings.kdeIconThemeLight)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                                    DesktopSettings.setKdeIconThemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Icon Theme — Dark")
                            description: qsTr("Applied to KDE / Qt applications when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: root.themeModel(ThemeManager.iconThemes)
                                currentIndex: root.themeIndex(ThemeManager.iconThemes, DesktopSettings.kdeIconThemeDark)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    const name = currentIndex === 0 ? "" : ThemeManager.iconThemes[currentIndex - 1]
                                    DesktopSettings.setKdeIconThemeDark(name)
                                }
                            }
                        }

                        Label {
                            visible: ThemeManager.iconThemes.length === 0
                            text: qsTr("No icon themes found. Install icon themes in an XDG data directory.")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true; Layout.leftMargin: 4
                        }
                    }
                }
            }

            // ── 4: Dock ────────────────────────────────────────────────────
            Flickable {
                contentHeight: dockCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: dockCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
                    spacing: 24

                    SettingGroup {
                        title: qsTr("Dock")
                        Layout.fillWidth: true

                        SettingCard {
                            label: qsTr("Animation Style")
                            description: qsTr("Controls how dock icons animate when launching apps.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: [qsTr("Lively"), qsTr("Subtle")]
                                currentIndex: DesktopSettings.dockMotionPreset === "subtle" ? 1 : 0
                                Layout.preferredWidth: 160
                                onActivated: DesktopSettings.setDockMotionPreset(
                                    currentIndex === 0 ? "macos-lively" : "subtle")
                            }
                        }

                        SettingCard {
                            label: qsTr("Auto-hide")
                            description: qsTr("Automatically hide the dock when not in use.")
                            Layout.fillWidth: true
                            SettingToggle {
                                checked: DesktopSettings.dockAutoHideEnabled
                                onToggled: DesktopSettings.setDockAutoHideEnabled(checked)
                            }
                        }

                        SettingCard {
                            label: qsTr("Drop Pulse")
                            description: qsTr("Animate dock items when a file is dragged over them.")
                            Layout.fillWidth: true
                            SettingToggle {
                                checked: DesktopSettings.dockDropPulseEnabled
                                onToggled: DesktopSettings.setDockDropPulseEnabled(checked)
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
                                    value: DesktopSettings.dockDragThresholdMouse
                                    Layout.preferredWidth: 160
                                    onMoved: DesktopSettings.setDockDragThresholdMouse(Math.round(value))
                                }
                                Text {
                                    text: DesktopSettings.dockDragThresholdMouse + qsTr(" px")
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
                                    value: DesktopSettings.dockDragThresholdTouchpad
                                    Layout.preferredWidth: 160
                                    onMoved: DesktopSettings.setDockDragThresholdTouchpad(Math.round(value))
                                }
                                Text {
                                    text: DesktopSettings.dockDragThresholdTouchpad + qsTr(" px")
                                    font.pixelSize: Theme.fontSize("small")
                                    color: Theme.color("textSecondary")
                                    Layout.preferredWidth: 40
                                }
                            }
                        }
                    }
                }
            }

            // ── 5: Desktop ─────────────────────────────────────────────────
            Flickable {
                id: desktopFlickable
                contentHeight: desktopCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: desktopCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
                    spacing: 24

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
                                    width: 192; height: 108
                                    radius: Theme.radiusSm
                                    color: Theme.color("controlDisabledBg")
                                    clip: true
                                    visible: WallpaperManager.currentWallpaperUri.length > 0

                                    Image {
                                        anchors.fill: parent
                                        source: WallpaperManager.currentWallpaperUri
                                        fillMode: Image.PreserveAspectCrop
                                        smooth: true; mipmap: true
                                    }
                                }

                                RowLayout {
                                    spacing: 8
                                    Button {
                                        text: WallpaperManager.loading
                                            ? qsTr("Opening…") : qsTr("Choose Picture…")
                                        enabled: !WallpaperManager.loading
                                        onClicked: WallpaperManager.openFilePicker()
                                    }
                                    Label {
                                        text: {
                                            const uri = WallpaperManager.currentWallpaperUri
                                            if (!uri) return ""
                                            return uri.split('/').pop()
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
