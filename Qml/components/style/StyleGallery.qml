import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: root

    signal closeRequested()
    signal dockHidePrefsApplied()

    property int comboIndex: 0
    property int tabIndex: 0
    property bool switchOn: true
    property bool checkOn: true
    property bool radioA: true
    property real sliderValue: 42
    property int spinValue: 8
    property string fieldText: "macOS-like style"
    property string iconThemeLightSelection: ""
    property string iconThemeDarkSelection: ""
    property bool windowShadowEnabled: true
    property int windowShadowStrength: 100
    property bool windowRoundedEnabled: true
    property int windowRoundedRadius: 10
    property bool windowAnimationEnabled: true
    property int windowAnimationSpeed: 100
    property bool windowSceneFxEnabled: false
    property bool windowSceneFxRoundedClipEnabled: false
    property real windowSceneFxDimAlpha: 0.38
    property int windowSceneFxAnimBoost: 115
    property int workspaceDragEdgeDwellMs: 350
    property int workspaceDragEdgeRepeatMs: 260
    property string dockHideModeSelection: "duration_hide"
    property int dockHideDurationSelection: 450
    property string bindMinimize: "Alt+F9"
    property string bindRestore: "Alt+F10"
    property string bindSwitchNext: "Alt+F11"
    property string bindSwitchPrev: "Alt+F12"
    property string bindWorkspace: "Alt+F3"
    property string bindClose: "Alt+F4"
    property string bindMaximize: "Alt+Up"
    property string bindTileLeft: "Alt+Left"
    property string bindTileRight: "Alt+Right"
    property string bindUntile: "Alt+Down"
    property string bindQuarterTopLeft: "Alt+Shift+Left"
    property string bindQuarterTopRight: "Alt+Shift+Right"
    property string bindQuarterBottomLeft: "Alt+Shift+Down"
    property string bindQuarterBottomRight: "Alt+Shift+Up"
    property var globalMenuDiag: ({})
    property string globalMenuDiagCopyStatus: ""
    property string windowBindingValidationError: ""
    property bool windowBindingDirty: false
    readonly property var windowFxCommandMap: ({
        "windowing.shadowEnabled": "shadowEnabled",
        "windowing.shadowStrength": "shadowStrength",
        "windowing.roundedEnabled": "roundedEnabled",
        "windowing.roundedRadius": "roundedRadius",
        "windowing.animationEnabled": "animationEnabled",
        "windowing.animationSpeed": "animationSpeed",
        "windowing.sceneFxEnabled": "sceneFxEnabled",
        "windowing.sceneFxRoundedClipEnabled": "sceneFxRoundedClipEnabled",
        "windowing.sceneFxDimAlpha": "sceneFxDimAlpha",
        "windowing.sceneFxAnimBoost": "sceneFxAnimBoost"
    })
    readonly property var windowBindCommandMap: ({
        "windowing.bindMinimize": "minimize",
        "windowing.bindRestore": "restore",
        "windowing.bindSwitchNext": "switchNext",
        "windowing.bindSwitchPrev": "switchPrev",
        "windowing.bindWorkspace": "workspace",
        "windowing.bindClose": "close",
        "windowing.bindMaximize": "maximize",
        "windowing.bindTileLeft": "tileLeft",
        "windowing.bindTileRight": "tileRight",
        "windowing.bindUntile": "untile",
        "windowing.bindQuarterTopLeft": "quarterTopLeft",
        "windowing.bindQuarterTopRight": "quarterTopRight",
        "windowing.bindQuarterBottomLeft": "quarterBottomLeft",
        "windowing.bindQuarterBottomRight": "quarterBottomRight"
    })

    radius: Theme.radiusXxl
    color: Theme.color("styleGalleryBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("windowCardBorder")
    clip: true

    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 44
        color: Theme.color("styleGalleryHeaderBg")
        border.width: Theme.borderWidthNone

        Label {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "Style Gallery"
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("body")
            font.weight: Theme.fontWeight("bold")
        }

        ToolButton {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "x"
            onClicked: root.closeRequested()
        }
    }

    ScrollView {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 14

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Icon Theme"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "Light"
                            color: Theme.color("textSecondary")
                            Layout.preferredWidth: 56
                        }
                        ComboBox {
                            id: lightThemeCombo
                            Layout.fillWidth: true
                            model: (typeof ThemeIconController !== "undefined" && ThemeIconController
                                    && ThemeIconController.availableThemes)
                                   ? ThemeIconController.availableThemes : []
                            onActivated: {
                                root.iconThemeLightSelection = currentText
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "Dark"
                            color: Theme.color("textSecondary")
                            Layout.preferredWidth: 56
                        }
                        ComboBox {
                            id: darkThemeCombo
                            Layout.fillWidth: true
                            model: (typeof ThemeIconController !== "undefined" && ThemeIconController
                                    && ThemeIconController.availableThemes)
                                   ? ThemeIconController.availableThemes : []
                            onActivated: {
                                root.iconThemeDarkSelection = currentText
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            text: "Apply"
                            enabled: root.iconThemeLightSelection.length > 0 && root.iconThemeDarkSelection.length > 0
                            onClicked: {
                                if (typeof UIPreferences !== "undefined" && UIPreferences &&
                                        UIPreferences.setIconThemeMapping) {
                                    UIPreferences.setIconThemeMapping(root.iconThemeLightSelection,
                                                                      root.iconThemeDarkSelection)
                                }
                                if (typeof ThemeIconController !== "undefined" && ThemeIconController) {
                                    ThemeIconController.setThemeMapping(root.iconThemeLightSelection,
                                                                        root.iconThemeDarkSelection)
                                    ThemeIconController.applyForDarkMode(Theme.darkMode)
                                }
                            }
                        }
                        Button {
                            text: "Reset Auto"
                            onClicked: {
                                if (typeof UIPreferences !== "undefined" && UIPreferences) {
                                    UIPreferences.resetPreference("iconTheme.light")
                                    UIPreferences.resetPreference("iconTheme.dark")
                                }
                                if (typeof ThemeIconController !== "undefined" && ThemeIconController) {
                                    ThemeIconController.useAutoDetectedMapping()
                                    ThemeIconController.applyForDarkMode(Theme.darkMode)
                                    root.iconThemeLightSelection = ThemeIconController.lightThemeName || ""
                                    root.iconThemeDarkSelection = ThemeIconController.darkThemeName || ""
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Dock Hide"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Mode"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        ComboBox {
                            id: dockHideModeCombo
                            Layout.fillWidth: true
                            model: [
                                { label: "No Hide", value: "no_hide" },
                                { label: "Duration Hide", value: "duration_hide" },
                                { label: "Smart Hide", value: "smart_hide" }
                            ]
                            textRole: "label"
                            onActivated: {
                                var entry = model[currentIndex]
                                root.dockHideModeSelection = entry ? String(entry.value || "duration_hide") : "duration_hide"
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Duration"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            Layout.fillWidth: true
                            from: 100
                            to: 5000
                            stepSize: 50
                            value: root.dockHideDurationSelection
                            onMoved: root.dockHideDurationSelection = Math.round(value)
                        }
                        Label {
                            text: String(root.dockHideDurationSelection) + " ms"
                            color: Theme.color("textSecondary")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            text: "Apply"
                            onClicked: {
                                root.setPref("dock.hideMode", root.dockHideModeSelection)
                                root.setPref("dock.hideDurationMs", root.dockHideDurationSelection)
                                root.syncDockHidePrefs()
                                root.dockHidePrefsApplied()
                            }
                        }
                        Button {
                            text: "Reset"
                            onClicked: {
                                if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.resetPreference) {
                                    UIPreferences.resetPreference("dock.hideMode")
                                    UIPreferences.resetPreference("dock.hideDurationMs")
                                }
                                root.syncDockHidePrefs()
                                root.dockHidePrefsApplied()
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Buttons"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }
                    RowLayout {
                        spacing: 10
                        Button { text: "Primary" }
                        Button { text: "Secondary"; enabled: false }
                        ToolButton {
                            text: "?"
                            ToolTip.visible: hovered
                            ToolTip.text: "ToolTip style preview"
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Text Inputs"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }
                    TextField {
                        Layout.fillWidth: true
                        text: root.fieldText
                        placeholderText: "Type here..."
                        onTextChanged: root.fieldText = text
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["Option A", "Option B", "Option C"]
                            currentIndex: root.comboIndex
                            onCurrentIndexChanged: root.comboIndex = currentIndex
                        }
                        SpinBox {
                            from: 0
                            to: 20
                            value: root.spinValue
                            onValueChanged: root.spinValue = value
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Selectors"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }
                    RowLayout {
                        spacing: 16
                        Switch {
                            text: "Enable"
                            checked: root.switchOn
                            onToggled: root.switchOn = checked
                        }
                        CheckBox {
                            text: "Check"
                            checked: root.checkOn
                            onToggled: root.checkOn = checked
                        }
                        RadioButton {
                            text: "Radio A"
                            checked: root.radioA
                            onToggled: root.radioA = checked
                        }
                        RadioButton {
                            text: "Radio B"
                            checked: !root.radioA
                            onToggled: {
                                if (checked) {
                                    root.radioA = false
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Range + Progress"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }
                    Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: root.sliderValue
                        onValueChanged: root.sliderValue = value
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: root.sliderValue
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Tabs + Dialog Buttons"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }
                    TabBar {
                        Layout.fillWidth: true
                        currentIndex: root.tabIndex
                        onCurrentIndexChanged: root.tabIndex = currentIndex
                        TabButton { text: "General" }
                        TabButton { text: "Display" }
                        TabButton { text: "Advanced" }
                    }
                    DialogButtonBox {
                        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Debug"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }
                    Switch {
                        text: "Verbose Logging"
                        checked: (typeof UIPreferences !== "undefined"
                                  && UIPreferences
                                  && UIPreferences.verboseLogging !== undefined)
                                 ? UIPreferences.verboseLogging : false
                        onToggled: {
                            if (typeof UIPreferences !== "undefined" && UIPreferences &&
                                UIPreferences.setVerboseLogging) {
                                UIPreferences.setVerboseLogging(checked)
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radiusMd
                        color: Theme.color("panelBg")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("windowCardBorder")

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: "Global Menu Diagnostics"
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    font.weight: Theme.fontWeight("bold")
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Refresh"
                                    onClicked: root.refreshGlobalMenuDiag()
                                }
                                Button {
                                    text: "Copy Snapshot"
                                    onClicked: root.copyGlobalMenuDiagSnapshot()
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "available=" + (root.globalMenuDiag.available ? "1" : "0")
                                      + " source=" + String(root.globalMenuDiag.selectionSource || "-")
                                      + " score=" + String(root.globalMenuDiag.selectionScore || 0)
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "service=" + String(root.globalMenuDiag.activeMenuService || "-")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "menus=" + String(root.globalMenuDiag.topLevelMenuCount || 0)
                                      + " registered=" + String(root.globalMenuDiag.registeredMenuCount || 0)
                                      + " activeAppToken=" + String(root.globalMenuDiag.lastActiveAppToken || "-")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: String(root.globalMenuDiagCopyStatus || "").length > 0
                                text: String(root.globalMenuDiagCopyStatus || "")
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Window Effects"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }

                    Switch {
                        text: "Shadow"
                        checked: root.windowShadowEnabled
                        onToggled: root.setPref("windowing.shadowEnabled", checked)
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Shadow Strength"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            id: shadowStrengthSlider
                            Layout.fillWidth: true
                            from: 20
                            to: 180
                            stepSize: 1
                            value: 100
                            onMoved: root.windowShadowStrength = Math.round(value)
                            onPressedChanged: {
                                if (!pressed) {
                                    root.setPref("windowing.shadowStrength", root.windowShadowStrength)
                                }
                            }
                        }
                        Label { text: String(root.windowShadowStrength); color: Theme.color("textSecondary") }
                    }

                    Switch {
                        text: "Rounded Corner"
                        checked: root.windowRoundedEnabled
                        onToggled: root.setPref("windowing.roundedEnabled", checked)
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Corner Radius"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            id: cornerRadiusSlider
                            Layout.fillWidth: true
                            from: 4
                            to: 24
                            stepSize: 1
                            value: 10
                            onMoved: root.windowRoundedRadius = Math.round(value)
                            onPressedChanged: {
                                if (!pressed) {
                                    root.setPref("windowing.roundedRadius", root.windowRoundedRadius)
                                }
                            }
                        }
                        Label { text: String(root.windowRoundedRadius); color: Theme.color("textSecondary") }
                    }

                    Switch {
                        text: "Window Animation"
                        checked: root.windowAnimationEnabled
                        onToggled: root.setPref("windowing.animationEnabled", checked)
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Animation Speed"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            id: animationSpeedSlider
                            Layout.fillWidth: true
                            from: 50
                            to: 200
                            stepSize: 1
                            value: 100
                            onMoved: root.windowAnimationSpeed = Math.round(value)
                            onPressedChanged: {
                                if (!pressed) {
                                    root.setPref("windowing.animationSpeed", root.windowAnimationSpeed)
                                }
                            }
                        }
                        Label { text: String(root.windowAnimationSpeed) + "%"; color: Theme.color("textSecondary") }
                    }

                    Switch {
                        text: "SceneFX Profile"
                        checked: root.windowSceneFxEnabled
                        onToggled: root.setPref("windowing.sceneFxEnabled", checked)
                    }
                    Switch {
                        text: "Rounded Clip Pipeline"
                        enabled: root.windowSceneFxEnabled
                        checked: root.windowSceneFxRoundedClipEnabled
                        onToggled: root.setPref("windowing.sceneFxRoundedClipEnabled", checked)
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        enabled: root.windowSceneFxEnabled
                        Label { text: "Inactive Dim"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            id: sceneFxDimSlider
                            Layout.fillWidth: true
                            from: 0.10
                            to: 0.70
                            stepSize: 0.01
                            value: 0.38
                            onMoved: root.windowSceneFxDimAlpha = Number(value.toFixed(2))
                            onPressedChanged: {
                                if (!pressed) {
                                    root.setPref("windowing.sceneFxDimAlpha", root.windowSceneFxDimAlpha)
                                }
                            }
                        }
                        Label { text: Number(root.windowSceneFxDimAlpha).toFixed(2); color: Theme.color("textSecondary") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        enabled: root.windowSceneFxEnabled
                        Label { text: "Anim Boost"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            id: sceneFxAnimBoostSlider
                            Layout.fillWidth: true
                            from: 50
                            to: 180
                            stepSize: 1
                            value: 115
                            onMoved: root.windowSceneFxAnimBoost = Math.round(value)
                            onPressedChanged: {
                                if (!pressed) {
                                    root.setPref("windowing.sceneFxAnimBoost", root.windowSceneFxAnimBoost)
                                }
                            }
                        }
                        Label { text: String(root.windowSceneFxAnimBoost) + "%"; color: Theme.color("textSecondary") }
                    }

                    Label {
                        text: "Workspace Drag"
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("bold")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Edge Dwell"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            id: workspaceEdgeDwellSlider
                            Layout.fillWidth: true
                            from: 120
                            to: 1200
                            stepSize: 10
                            value: 350
                            onMoved: root.workspaceDragEdgeDwellMs = Math.round(value)
                            onPressedChanged: {
                                if (!pressed) {
                                    root.setPref("workspace.dragEdgeDwellMs", root.workspaceDragEdgeDwellMs)
                                }
                            }
                        }
                        Label { text: String(root.workspaceDragEdgeDwellMs) + " ms"; color: Theme.color("textSecondary") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Edge Repeat"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        Slider {
                            id: workspaceEdgeRepeatSlider
                            Layout.fillWidth: true
                            from: 80
                            to: 700
                            stepSize: 10
                            value: 260
                            onMoved: root.workspaceDragEdgeRepeatMs = Math.round(value)
                            onPressedChanged: {
                                if (!pressed) {
                                    root.setPref("workspace.dragEdgeRepeatMs", root.workspaceDragEdgeRepeatMs)
                                }
                            }
                        }
                        Label { text: String(root.workspaceDragEdgeRepeatMs) + " ms"; color: Theme.color("textSecondary") }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Label { text: "Window Shortcuts"; color: Theme.color("textPrimary"); font.weight: Theme.fontWeight("bold") }
                    Label {
                        Layout.fillWidth: true
                        text: "Click a field, then press the desired key combination. Example: Alt+F3. Delete/Backspace = Disabled, Esc = revert field, Ctrl+S = Apply, Ctrl+R = Revert all."
                        color: Theme.color("textSecondary")
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSize("small")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Minimize"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: minimizeBindField
                            Layout.fillWidth: true
                            text: root.bindMinimize
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Restore"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: restoreBindField
                            Layout.fillWidth: true
                            text: root.bindRestore
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Switch Next"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: switchNextBindField
                            Layout.fillWidth: true
                            text: root.bindSwitchNext
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Switch Prev"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: switchPrevBindField
                            Layout.fillWidth: true
                            text: root.bindSwitchPrev
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Workspace"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: workspaceBindField
                            Layout.fillWidth: true
                            text: root.bindWorkspace
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Close"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: closeBindField
                            Layout.fillWidth: true
                            text: root.bindClose
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Maximize"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: maximizeBindField
                            Layout.fillWidth: true
                            text: root.bindMaximize
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Tile Left"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: tileLeftBindField
                            Layout.fillWidth: true
                            text: root.bindTileLeft
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Tile Right"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: tileRightBindField
                            Layout.fillWidth: true
                            text: root.bindTileRight
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Untile"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: untileBindField
                            Layout.fillWidth: true
                            text: root.bindUntile
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Quarter Top Left"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: quarterTopLeftBindField
                            Layout.fillWidth: true
                            text: root.bindQuarterTopLeft
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Quarter Top Right"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: quarterTopRightBindField
                            Layout.fillWidth: true
                            text: root.bindQuarterTopRight
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Quarter Bottom Left"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: quarterBottomLeftBindField
                            Layout.fillWidth: true
                            text: root.bindQuarterBottomLeft
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Quarter Bottom Right"; color: Theme.color("textSecondary"); Layout.preferredWidth: 120 }
                        TextField {
                            id: quarterBottomRightBindField
                            Layout.fillWidth: true
                            text: root.bindQuarterBottomRight
                            Keys.onPressed: function(event) { root.captureBindingIntoField(this, event) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            id: applyWindowBindingButton
                            text: "Apply"
                            enabled: root.windowBindingValidationError.length === 0 &&
                                     root.windowBindingDirty
                            onClicked: root.applyWindowBindingPrefs()
                        }
                        Button {
                            text: "Revert"
                            enabled: root.windowBindingDirty
                            onClicked: {
                                root.revertWindowBindingEdits()
                            }
                        }
                        Button {
                            text: "Reset Default"
                            onClicked: root.resetWindowBindingDefaults()
                        }
                        Button {
                            text: "Disable All"
                            onClicked: root.disableAllWindowBindings()
                        }
                        Button {
                            text: "Default All"
                            onClicked: root.setAllWindowBindingsToDefaults()
                        }
                        Button {
                            text: "macOS Preset"
                            onClicked: root.setAllWindowBindingsToMacosPreset()
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            text: "Disable Quarter"
                            onClicked: {
                                quarterTopLeftBindField.text = "Disabled"
                                quarterTopRightBindField.text = "Disabled"
                                quarterBottomLeftBindField.text = "Disabled"
                                quarterBottomRightBindField.text = "Disabled"
                                root.validateWindowBindings()
                                root.updateWindowBindingDirty()
                            }
                        }
                        Button {
                            text: "Default Quarter"
                            onClicked: {
                                quarterTopLeftBindField.text = "Alt+Shift+Left"
                                quarterTopRightBindField.text = "Alt+Shift+Right"
                                quarterBottomLeftBindField.text = "Alt+Shift+Down"
                                quarterBottomRightBindField.text = "Alt+Shift+Up"
                                root.validateWindowBindings()
                                root.updateWindowBindingDirty()
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.windowBindingValidationError.length === 0 && root.windowBindingDirty
                        text: "Unsaved shortcut changes."
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.windowBindingValidationError.length > 0
                        text: root.windowBindingValidationError
                        color: Theme.color("error")
                        wrapMode: Text.WordWrap
                        lineHeightMode: Text.ProportionalHeight
                        lineHeight: Theme.lineHeight("normal")
                    }
                }
            }
        }
            // ── Motion & Shadow Tokens ─────────────────────────────────────
            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: Theme.radiusCard
                    color: Theme.color("styleGallerySectionBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("windowCardBorder")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Label {
                        text: "Motion & Shadow Tokens"
                        color: Theme.color("textPrimary")
                        font.weight: Theme.fontWeight("bold")
                    }

                    // ── Duration token table ──────────────────────────
                    Label {
                        text: "Duration tokens (use in Behavior/NumberAnimation):"
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 16
                        rowSpacing: 4

                        Repeater {
                            model: [
                                { name: "durationMicro", value: Theme.durationMicro },
                                { name: "durationSm",    value: Theme.durationSm },
                                { name: "durationMd",    value: Theme.durationMd },
                                { name: "durationLg",    value: Theme.durationLg },
                                { name: "durationXl",    value: Theme.durationXl }
                            ]

                            ColumnLayout {
                                required property var modelData
                                spacing: 1

                                Label {
                                    text: "Theme." + modelData.name
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    font.family: Theme.fontFamilyMonospace
                                }

                                Item {
                                    width: 120
                                    height: 6
                                    Rectangle {
                                        id: durationBar
                                        height: parent.height
                                        radius: height / 2
                                        color: Theme.color("accent")
                                        width: 0
                                        Component.onCompleted: durationBarAnim.restart()
                                        NumberAnimation on width {
                                            id: durationBarAnim
                                            from: 0
                                            to: Math.max(8, Math.min(120, 120 * (durationBar.parent.parent.modelData.value / 400.0)))
                                            duration: durationBar.parent.parent.modelData.value
                                            easing.type: Theme.easingDecelerate
                                        }
                                    }
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: parent.height / 2
                                        color: Theme.color("accent")
                                        opacity: 0.15
                                    }
                                }

                                Label {
                                    text: modelData.value + " ms"
                                    color: Theme.color("textSecondary")
                                    font.pixelSize: Theme.fontSize("xs")
                                }
                            }
                        }
                    }

                    // ── controlBgHover demo ───────────────────────────
                    Label {
                        text: "Hover state (controlBgHover / controlBgPressed): hover over the chip below"
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: 8

                        Repeater {
                            model: ["Chip A", "Chip B", "Chip C"]

                            Rectangle {
                                required property string modelData
                                required property int index
                                width: hoverLabel.implicitWidth + 24
                                height: 28
                                radius: Theme.radiusControl
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color("panelBorder")
                                color: chipHover.active
                                       ? (chipMouse.pressed
                                          ? Theme.color("controlBgPressed")
                                          : Theme.color("controlBgHover"))
                                       : "transparent"

                                Behavior on color { ColorAnimation { duration: Theme.durationSm } }

                                Label {
                                    id: hoverLabel
                                    anchors.centerIn: parent
                                    text: parent.modelData
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                }

                                HoverHandler { id: chipHover }
                                MouseArea { id: chipMouse; anchors.fill: parent; hoverEnabled: true }
                            }
                        }
                    }

                    // ── Shadow preview ────────────────────────────────
                    Label {
                        text: "Shadow tokens (shadowSm / shadowMd / shadowLg / shadowXl):"
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                    }

                    RowLayout {
                        spacing: 24

                        Repeater {
                            model: [
                                { label: "shadowSm",  shadow: Theme.shadowSm },
                                { label: "shadowMd",  shadow: Theme.shadowMd },
                                { label: "shadowLg",  shadow: Theme.shadowLg },
                                { label: "shadowXl",  shadow: Theme.shadowXl }
                            ]

                            Item {
                                required property var modelData
                                width: 80
                                height: 60

                                Rectangle {
                                    anchors.fill: shadowCard
                                    anchors.margins: -(modelData.shadow.radius * 0.22)
                                    radius: shadowCard.radius + modelData.shadow.radius * 0.22
                                    color: "transparent"
                                    border.width: modelData.shadow.radius
                                    border.color: Qt.rgba(0, 0, 0,
                                        modelData.shadow.opacity * (Theme.darkMode ? 1.6 : 1.0))
                                    z: -1
                                    opacity: 0.5
                                    scale: 1.0
                                }

                                Rectangle {
                                    id: shadowCard
                                    anchors.centerIn: parent
                                    width: 64
                                    height: 40
                                    radius: Theme.radiusMd
                                    color: Theme.color("surface")
                                    border.width: Theme.borderWidthThin
                                    border.color: Theme.color("panelBorder")

                                    Label {
                                        anchors.centerIn: parent
                                        text: parent.parent.modelData.label
                                        color: Theme.color("textSecondary")
                                        font.pixelSize: Theme.fontSize("xs")
                                        font.family: Theme.fontFamilyMonospace
                                    }
                                }
                            }
                        }
                    }
                }
            }

    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: root.windowBindingDirty && root.windowBindingValidationError.length === 0
        onActivated: root.applyWindowBindingPrefs()
    }

    Shortcut {
        sequence: "Ctrl+R"
        enabled: root.windowBindingDirty
        onActivated: root.revertWindowBindingEdits()
    }

    function indexOfTheme(themeName, list) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i] === themeName) {
                return i
            }
        }
        return -1
    }

    function syncIconThemeSelection() {
        var lightPref = (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.iconThemeLight !== undefined)
                        ? (UIPreferences.iconThemeLight || "") : ""
        var darkPref = (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.iconThemeDark !== undefined)
                       ? (UIPreferences.iconThemeDark || "") : ""
        var light = lightPref.length > 0
                    ? lightPref
                    : ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                       ? (ThemeIconController.lightThemeName || "") : "")
        var dark = darkPref.length > 0
                   ? darkPref
                   : ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                      ? (ThemeIconController.darkThemeName || "") : "")

        root.iconThemeLightSelection = light
        root.iconThemeDarkSelection = dark

        var list = (typeof ThemeIconController !== "undefined" && ThemeIconController &&
                    ThemeIconController.availableThemes) ? ThemeIconController.availableThemes : []
        var lightIdx = indexOfTheme(light, list)
        if (lightIdx >= 0) {
            lightThemeCombo.currentIndex = lightIdx
        }
        var darkIdx = indexOfTheme(dark, list)
        if (darkIdx >= 0) {
            darkThemeCombo.currentIndex = darkIdx
        }
    }

    function prefBool(key, fallback) {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return fallback
        }
        return !!UIPreferences.getPreference(key, fallback)
    }

    function prefInt(key, fallback) {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return fallback
        }
        var v = UIPreferences.getPreference(key, fallback)
        var n = Number(v)
        if (isNaN(n)) {
            return fallback
        }
        return Math.round(n)
    }

    function prefReal(key, fallback) {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return fallback
        }
        var v = UIPreferences.getPreference(key, fallback)
        var n = Number(v)
        if (isNaN(n)) {
            return fallback
        }
        return n
    }

    function prefStr(key, fallback) {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return fallback
        }
        var v = UIPreferences.getPreference(key, fallback)
        if (v === undefined || v === null) {
            return fallback
        }
        return String(v)
    }

    function setPref(key, value) {
        if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
            UIPreferences.setPreference(key, value)
        }
        if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            if (root.windowFxCommandMap[key] !== undefined) {
                var v = (typeof value === "boolean") ? (value ? "1" : "0") : String(value)
                WindowingBackend.sendCommand("windowfx " + root.windowFxCommandMap[key] + " " + v)
            } else if (root.windowBindCommandMap[key] !== undefined) {
                WindowingBackend.sendCommand("windowbind " + root.windowBindCommandMap[key] + " " + String(value))
            }
        }
        syncWindowEffectPrefs()
    }

    function parseAndNormalizeBinding(raw) {
        var s = String(raw || "").trim()
        if (s.length === 0) {
            return ""
        }
        var lowerWhole = s.toLowerCase()
        if (lowerWhole === "disabled" || lowerWhole === "none" || lowerWhole === "off") {
            return "Disabled"
        }
        var chunks = s.split("+")
        var modsSeen = { "Ctrl": false, "Alt": false, "Shift": false, "Super": false }
        var keyToken = ""
        for (var i = 0; i < chunks.length; ++i) {
            var t = String(chunks[i] || "").trim()
            if (t.length === 0) {
                return ""
            }
            var lower = t.toLowerCase()
            if (lower === "control") {
                t = "Ctrl"
            } else if (lower === "ctrl") {
                t = "Ctrl"
            } else if (lower === "alt") {
                t = "Alt"
            } else if (lower === "shift") {
                t = "Shift"
            } else if (lower === "super" || lower === "logo" || lower === "win") {
                t = "Super"
            } else if (/^f[1-9]$/.test(lower) || /^f1[0-9]$/.test(lower) || /^f2[0-4]$/.test(lower)) {
                t = lower.toUpperCase()
            } else if (lower === "left" || lower === "right" || lower === "up" || lower === "down") {
                t = lower.charAt(0).toUpperCase() + lower.slice(1)
            } else if (lower === "pageup" || lower === "prior") {
                t = "PageUp"
            } else if (lower === "pagedown" || lower === "next") {
                t = "PageDown"
            } else if (lower === "home" || lower === "end" || lower === "insert" ||
                       lower === "delete" || lower === "escape" || lower === "tab" ||
                       lower === "space") {
                t = lower.charAt(0).toUpperCase() + lower.slice(1)
            } else if (lower === "return" || lower === "enter" || lower === "kp_enter") {
                t = "Enter"
            }
            if (t === "Ctrl" || t === "Alt" || t === "Shift" || t === "Super") {
                modsSeen[t] = true
            } else {
                if (keyToken.length > 0) {
                    return ""
                }
                keyToken = t
            }
        }
        if (keyToken.length === 0) {
            return ""
        }
        var normalized = []
        if (modsSeen["Ctrl"]) {
            normalized.push("Ctrl")
        }
        if (modsSeen["Alt"]) {
            normalized.push("Alt")
        }
        if (modsSeen["Shift"]) {
            normalized.push("Shift")
        }
        if (modsSeen["Super"]) {
            normalized.push("Super")
        }
        normalized.push(keyToken)
        return normalized.join("+")
    }

    function isRecognizedKeyToken(token) {
        var t = String(token || "").trim()
        if (t.length === 0) {
            return false
        }
        var lower = t.toLowerCase()
        if (/^f[1-9]$/.test(lower) || /^f1[0-9]$/.test(lower) || /^f2[0-4]$/.test(lower)) {
            return true
        }
        if (/^[a-z0-9]$/.test(lower)) {
            return true
        }
        if (lower === "left" || lower === "right" || lower === "up" || lower === "down" ||
            lower === "pageup" || lower === "pagedown" || lower === "prior" || lower === "next" ||
            lower === "home" || lower === "end" || lower === "insert" || lower === "delete" ||
            lower === "escape" || lower === "tab" || lower === "space" ||
            lower === "return" || lower === "enter" || lower === "kp_enter") {
            return true
        }
        return false
    }

    function keyTokenFromEvent(event) {
        var k = event.key
        if (k >= Qt.Key_F1 && k <= Qt.Key_F24) {
            return "F" + String(k - Qt.Key_F1 + 1)
        }
        if (k === Qt.Key_Left) {
            return "Left"
        }
        if (k === Qt.Key_Right) {
            return "Right"
        }
        if (k === Qt.Key_Up) {
            return "Up"
        }
        if (k === Qt.Key_Down) {
            return "Down"
        }
        if (k === Qt.Key_PageUp || k === Qt.Key_Prior) {
            return "PageUp"
        }
        if (k === Qt.Key_PageDown || k === Qt.Key_Next) {
            return "PageDown"
        }
        if (k === Qt.Key_Home) {
            return "Home"
        }
        if (k === Qt.Key_End) {
            return "End"
        }
        if (k === Qt.Key_Insert) {
            return "Insert"
        }
        if (k === Qt.Key_Delete) {
            return "Delete"
        }
        if (k === Qt.Key_Escape) {
            return "Escape"
        }
        if (k === Qt.Key_Tab) {
            return "Tab"
        }
        if (k === Qt.Key_Space) {
            return "Space"
        }
        if (k === Qt.Key_Return || k === Qt.Key_Enter) {
            return "Enter"
        }
        var t = String(event.text || "")
        if (t.length === 1 && /^[A-Za-z0-9]$/.test(t)) {
            return t.toUpperCase()
        }
        return ""
    }

    function bindingFromKeyEvent(event) {
        var token = keyTokenFromEvent(event)
        if (token.length === 0) {
            return ""
        }
        var parts = []
        if (event.modifiers & Qt.ControlModifier) {
            parts.push("Ctrl")
        }
        if (event.modifiers & Qt.AltModifier) {
            parts.push("Alt")
        }
        if (event.modifiers & Qt.ShiftModifier) {
            parts.push("Shift")
        }
        if (event.modifiers & Qt.MetaModifier) {
            parts.push("Super")
        }
        parts.push(token)
        return parseAndNormalizeBinding(parts.join("+"))
    }

    function captureBindingIntoField(field, event) {
        var noMods = (event.modifiers === Qt.NoModifier)
        if (noMods && event.key === Qt.Key_Escape) {
            field.text = savedBindingForField(field)
            event.accepted = true
            return
        }
        if (noMods && (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete)) {
            field.text = "Disabled"
            event.accepted = true
            return
        }
        var b = bindingFromKeyEvent(event)
        if (b.length > 0) {
            field.text = b
            event.accepted = true
        }
    }

    function savedBindingForField(field) {
        if (field === minimizeBindField) {
            return root.bindMinimize
        }
        if (field === restoreBindField) {
            return root.bindRestore
        }
        if (field === switchNextBindField) {
            return root.bindSwitchNext
        }
        if (field === switchPrevBindField) {
            return root.bindSwitchPrev
        }
        if (field === workspaceBindField) {
            return root.bindWorkspace
        }
        if (field === closeBindField) {
            return root.bindClose
        }
        if (field === maximizeBindField) {
            return root.bindMaximize
        }
        if (field === tileLeftBindField) {
            return root.bindTileLeft
        }
        if (field === tileRightBindField) {
            return root.bindTileRight
        }
        if (field === untileBindField) {
            return root.bindUntile
        }
        if (field === quarterTopLeftBindField) {
            return root.bindQuarterTopLeft
        }
        if (field === quarterTopRightBindField) {
            return root.bindQuarterTopRight
        }
        if (field === quarterBottomLeftBindField) {
            return root.bindQuarterBottomLeft
        }
        if (field === quarterBottomRightBindField) {
            return root.bindQuarterBottomRight
        }
        return field && field.text !== undefined ? String(field.text) : ""
    }

    function isValidBindingText(raw) {
        var s = parseAndNormalizeBinding(raw)
        if (s.length === 0) {
            return false
        }
        if (s === "Disabled") {
            return true
        }
        var chunks = s.split("+")
        var nonModifierCount = 0
        var lastNonModifier = ""
        for (var i = 0; i < chunks.length; ++i) {
            var t = String(chunks[i] || "")
            if (t.indexOf(" ") >= 0) {
                return false
            }
            var lower = t.toLowerCase()
            var isModifier = (lower === "alt" || lower === "ctrl" || lower === "control" ||
                              lower === "shift" || lower === "super" || lower === "logo" ||
                              lower === "win")
            if (!isModifier) {
                nonModifierCount += 1
                lastNonModifier = t
            }
        }
        if (nonModifierCount !== 1) {
            return false
        }
        return isRecognizedKeyToken(lastNonModifier)
    }

    function validateWindowBindings() {
        var fields = [
            { label: "Minimize", value: minimizeBindField.text },
            { label: "Restore", value: restoreBindField.text },
            { label: "Switch Next", value: switchNextBindField.text },
            { label: "Switch Prev", value: switchPrevBindField.text },
            { label: "Workspace", value: workspaceBindField.text },
            { label: "Close", value: closeBindField.text },
            { label: "Maximize", value: maximizeBindField.text },
            { label: "Tile Left", value: tileLeftBindField.text },
            { label: "Tile Right", value: tileRightBindField.text },
            { label: "Untile", value: untileBindField.text },
            { label: "Quarter Top Left", value: quarterTopLeftBindField.text },
            { label: "Quarter Top Right", value: quarterTopRightBindField.text },
            { label: "Quarter Bottom Left", value: quarterBottomLeftBindField.text },
            { label: "Quarter Bottom Right", value: quarterBottomRightBindField.text }
        ]
        var seen = {}
        for (var i = 0; i < fields.length; ++i) {
            if (!isValidBindingText(fields[i].value)) {
                root.windowBindingValidationError =
                        "Invalid shortcut format at: " + fields[i].label +
                        ". Use format like Alt+F3 or Ctrl+Shift+Right."
                return false
            }
            var normalized = parseAndNormalizeBinding(fields[i].value)
            if (normalized === "Disabled") {
                continue
            }
            if (seen[normalized] !== undefined) {
                root.windowBindingValidationError =
                        "Shortcut conflict: " + fields[i].label + " and " + seen[normalized] +
                        " both use " + normalized + "."
                return false
            }
            seen[normalized] = fields[i].label
        }
        root.windowBindingValidationError = ""
        return true
    }

    function currentBindingMap() {
        return {
            minimize: parseAndNormalizeBinding(minimizeBindField.text),
            restore: parseAndNormalizeBinding(restoreBindField.text),
            switchNext: parseAndNormalizeBinding(switchNextBindField.text),
            switchPrev: parseAndNormalizeBinding(switchPrevBindField.text),
            workspace: parseAndNormalizeBinding(workspaceBindField.text),
            close: parseAndNormalizeBinding(closeBindField.text),
            maximize: parseAndNormalizeBinding(maximizeBindField.text),
            tileLeft: parseAndNormalizeBinding(tileLeftBindField.text),
            tileRight: parseAndNormalizeBinding(tileRightBindField.text),
            untile: parseAndNormalizeBinding(untileBindField.text),
            quarterTopLeft: parseAndNormalizeBinding(quarterTopLeftBindField.text),
            quarterTopRight: parseAndNormalizeBinding(quarterTopRightBindField.text),
            quarterBottomLeft: parseAndNormalizeBinding(quarterBottomLeftBindField.text),
            quarterBottomRight: parseAndNormalizeBinding(quarterBottomRightBindField.text)
        }
    }

    function savedBindingMap() {
        return {
            minimize: parseAndNormalizeBinding(root.bindMinimize),
            restore: parseAndNormalizeBinding(root.bindRestore),
            switchNext: parseAndNormalizeBinding(root.bindSwitchNext),
            switchPrev: parseAndNormalizeBinding(root.bindSwitchPrev),
            workspace: parseAndNormalizeBinding(root.bindWorkspace),
            close: parseAndNormalizeBinding(root.bindClose),
            maximize: parseAndNormalizeBinding(root.bindMaximize),
            tileLeft: parseAndNormalizeBinding(root.bindTileLeft),
            tileRight: parseAndNormalizeBinding(root.bindTileRight),
            untile: parseAndNormalizeBinding(root.bindUntile),
            quarterTopLeft: parseAndNormalizeBinding(root.bindQuarterTopLeft),
            quarterTopRight: parseAndNormalizeBinding(root.bindQuarterTopRight),
            quarterBottomLeft: parseAndNormalizeBinding(root.bindQuarterBottomLeft),
            quarterBottomRight: parseAndNormalizeBinding(root.bindQuarterBottomRight)
        }
    }

    function updateWindowBindingDirty() {
        var cur = currentBindingMap()
        var saved = savedBindingMap()
        var keys = Object.keys(saved)
        for (var i = 0; i < keys.length; ++i) {
            var k = keys[i]
            if ((cur[k] || "") !== (saved[k] || "")) {
                root.windowBindingDirty = true
                return
            }
        }
        root.windowBindingDirty = false
    }

    function revertWindowBindingEdits() {
        minimizeBindField.text = root.bindMinimize
        restoreBindField.text = root.bindRestore
        switchNextBindField.text = root.bindSwitchNext
        switchPrevBindField.text = root.bindSwitchPrev
        workspaceBindField.text = root.bindWorkspace
        closeBindField.text = root.bindClose
        maximizeBindField.text = root.bindMaximize
        tileLeftBindField.text = root.bindTileLeft
        tileRightBindField.text = root.bindTileRight
        untileBindField.text = root.bindUntile
        quarterTopLeftBindField.text = root.bindQuarterTopLeft
        quarterTopRightBindField.text = root.bindQuarterTopRight
        quarterBottomLeftBindField.text = root.bindQuarterBottomLeft
        quarterBottomRightBindField.text = root.bindQuarterBottomRight
        validateWindowBindings()
        updateWindowBindingDirty()
    }

    function disableAllWindowBindings() {
        minimizeBindField.text = "Disabled"
        restoreBindField.text = "Disabled"
        switchNextBindField.text = "Disabled"
        switchPrevBindField.text = "Disabled"
        workspaceBindField.text = "Disabled"
        closeBindField.text = "Disabled"
        maximizeBindField.text = "Disabled"
        tileLeftBindField.text = "Disabled"
        tileRightBindField.text = "Disabled"
        untileBindField.text = "Disabled"
        quarterTopLeftBindField.text = "Disabled"
        quarterTopRightBindField.text = "Disabled"
        quarterBottomLeftBindField.text = "Disabled"
        quarterBottomRightBindField.text = "Disabled"
        validateWindowBindings()
        updateWindowBindingDirty()
    }

    function setAllWindowBindingsToDefaults() {
        minimizeBindField.text = "Alt+F9"
        restoreBindField.text = "Alt+F10"
        switchNextBindField.text = "Alt+F11"
        switchPrevBindField.text = "Alt+F12"
        workspaceBindField.text = "Alt+F3"
        closeBindField.text = "Alt+F4"
        maximizeBindField.text = "Alt+Up"
        tileLeftBindField.text = "Alt+Left"
        tileRightBindField.text = "Alt+Right"
        untileBindField.text = "Alt+Down"
        quarterTopLeftBindField.text = "Alt+Shift+Left"
        quarterTopRightBindField.text = "Alt+Shift+Right"
        quarterBottomLeftBindField.text = "Alt+Shift+Down"
        quarterBottomRightBindField.text = "Alt+Shift+Up"
        validateWindowBindings()
        updateWindowBindingDirty()
    }

    function setAllWindowBindingsToMacosPreset() {
        minimizeBindField.text = "Super+M"
        restoreBindField.text = "Super+Shift+M"
        switchNextBindField.text = "Super+Tab"
        switchPrevBindField.text = "Super+Shift+Tab"
        workspaceBindField.text = "Ctrl+Up"
        closeBindField.text = "Super+W"
        maximizeBindField.text = "Ctrl+Super+F"
        tileLeftBindField.text = "Ctrl+Alt+Super+Left"
        tileRightBindField.text = "Ctrl+Alt+Super+Right"
        untileBindField.text = "Ctrl+Alt+Super+Down"
        quarterTopLeftBindField.text = "Ctrl+Alt+Super+Shift+Left"
        quarterTopRightBindField.text = "Ctrl+Alt+Super+Shift+Right"
        quarterBottomLeftBindField.text = "Ctrl+Alt+Super+Shift+Down"
        quarterBottomRightBindField.text = "Ctrl+Alt+Super+Shift+Up"
        validateWindowBindings()
        updateWindowBindingDirty()
    }

    function applyWindowBindingPrefs() {
        if (root.windowBindingValidationError.length > 0) {
            return
        }
        root.setPref("windowing.bindMinimize", root.parseAndNormalizeBinding(minimizeBindField.text))
        root.setPref("windowing.bindRestore", root.parseAndNormalizeBinding(restoreBindField.text))
        root.setPref("windowing.bindSwitchNext", root.parseAndNormalizeBinding(switchNextBindField.text))
        root.setPref("windowing.bindSwitchPrev", root.parseAndNormalizeBinding(switchPrevBindField.text))
        root.setPref("windowing.bindWorkspace", root.parseAndNormalizeBinding(workspaceBindField.text))
        root.setPref("windowing.bindClose", root.parseAndNormalizeBinding(closeBindField.text))
        root.setPref("windowing.bindMaximize", root.parseAndNormalizeBinding(maximizeBindField.text))
        root.setPref("windowing.bindTileLeft", root.parseAndNormalizeBinding(tileLeftBindField.text))
        root.setPref("windowing.bindTileRight", root.parseAndNormalizeBinding(tileRightBindField.text))
        root.setPref("windowing.bindUntile", root.parseAndNormalizeBinding(untileBindField.text))
        root.setPref("windowing.bindQuarterTopLeft", root.parseAndNormalizeBinding(quarterTopLeftBindField.text))
        root.setPref("windowing.bindQuarterTopRight", root.parseAndNormalizeBinding(quarterTopRightBindField.text))
        root.setPref("windowing.bindQuarterBottomLeft", root.parseAndNormalizeBinding(quarterBottomLeftBindField.text))
        root.setPref("windowing.bindQuarterBottomRight", root.parseAndNormalizeBinding(quarterBottomRightBindField.text))
        root.syncWindowBindingPrefs()
        root.windowBindingDirty = false
    }

    function resetWindowBindingDefaults() {
        if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.resetPreference) {
            UIPreferences.resetPreference("windowing.bindMinimize")
            UIPreferences.resetPreference("windowing.bindRestore")
            UIPreferences.resetPreference("windowing.bindSwitchNext")
            UIPreferences.resetPreference("windowing.bindSwitchPrev")
            UIPreferences.resetPreference("windowing.bindWorkspace")
            UIPreferences.resetPreference("windowing.bindClose")
            UIPreferences.resetPreference("windowing.bindMaximize")
            UIPreferences.resetPreference("windowing.bindTileLeft")
            UIPreferences.resetPreference("windowing.bindTileRight")
            UIPreferences.resetPreference("windowing.bindUntile")
            UIPreferences.resetPreference("windowing.bindQuarterTopLeft")
            UIPreferences.resetPreference("windowing.bindQuarterTopRight")
            UIPreferences.resetPreference("windowing.bindQuarterBottomLeft")
            UIPreferences.resetPreference("windowing.bindQuarterBottomRight")
        }
        root.syncWindowBindingPrefs()
        root.windowBindingDirty = false
    }

    function syncDockHidePrefs() {
        root.dockHideModeSelection = prefStr("dock.hideMode",
                                             prefBool("dock.autoHideEnabled", false) ? "duration_hide" : "no_hide")
        root.dockHideModeSelection = root.dockHideModeSelection.trim().toLowerCase()
        if (root.dockHideModeSelection === "snart_hide") {
            root.dockHideModeSelection = "smart_hide"
        }
        if (root.dockHideModeSelection !== "no_hide" &&
            root.dockHideModeSelection !== "duration_hide" &&
            root.dockHideModeSelection !== "smart_hide") {
            root.dockHideModeSelection = "duration_hide"
        }
        root.dockHideDurationSelection = prefInt("dock.hideDurationMs", 450)
        if (root.dockHideDurationSelection < 100) {
            root.dockHideDurationSelection = 100
        }
        if (root.dockHideDurationSelection > 5000) {
            root.dockHideDurationSelection = 5000
        }

        if (dockHideModeCombo && dockHideModeCombo.model) {
            var idx = 0
            for (var i = 0; i < dockHideModeCombo.model.length; ++i) {
                var m = dockHideModeCombo.model[i]
                if (m && String(m.value || "") === root.dockHideModeSelection) {
                    idx = i
                    break
                }
            }
            dockHideModeCombo.currentIndex = idx
        }
    }

    function syncWindowEffectPrefs() {
        root.windowShadowEnabled = prefBool("windowing.shadowEnabled", true)
        root.windowShadowStrength = prefInt("windowing.shadowStrength", 100)
        if (shadowStrengthSlider && !shadowStrengthSlider.pressed) {
            shadowStrengthSlider.value = root.windowShadowStrength
        }
        root.windowRoundedEnabled = prefBool("windowing.roundedEnabled", false)
        root.windowRoundedRadius = prefInt("windowing.roundedRadius", 10)
        if (cornerRadiusSlider && !cornerRadiusSlider.pressed) {
            cornerRadiusSlider.value = root.windowRoundedRadius
        }
        root.windowAnimationEnabled = prefBool("windowing.animationEnabled", true)
        root.windowAnimationSpeed = prefInt("windowing.animationSpeed", 100)
        if (animationSpeedSlider && !animationSpeedSlider.pressed) {
            animationSpeedSlider.value = root.windowAnimationSpeed
        }
        root.windowSceneFxEnabled = prefBool("windowing.sceneFxEnabled", false)
        root.windowSceneFxRoundedClipEnabled = prefBool("windowing.sceneFxRoundedClipEnabled", false)
        root.windowSceneFxDimAlpha = prefReal("windowing.sceneFxDimAlpha", 0.38)
        if (root.windowSceneFxDimAlpha < 0.10) {
            root.windowSceneFxDimAlpha = 0.10
        }
        if (root.windowSceneFxDimAlpha > 0.70) {
            root.windowSceneFxDimAlpha = 0.70
        }
        if (sceneFxDimSlider && !sceneFxDimSlider.pressed) {
            sceneFxDimSlider.value = root.windowSceneFxDimAlpha
        }
        root.windowSceneFxAnimBoost = prefInt("windowing.sceneFxAnimBoost", 115)
        if (root.windowSceneFxAnimBoost < 50) {
            root.windowSceneFxAnimBoost = 50
        }
        if (root.windowSceneFxAnimBoost > 180) {
            root.windowSceneFxAnimBoost = 180
        }
        if (sceneFxAnimBoostSlider && !sceneFxAnimBoostSlider.pressed) {
            sceneFxAnimBoostSlider.value = root.windowSceneFxAnimBoost
        }

        root.workspaceDragEdgeDwellMs = prefInt("workspace.dragEdgeDwellMs", 350)
        if (root.workspaceDragEdgeDwellMs < 120) {
            root.workspaceDragEdgeDwellMs = 120
        }
        if (root.workspaceDragEdgeDwellMs > 1200) {
            root.workspaceDragEdgeDwellMs = 1200
        }
        if (workspaceEdgeDwellSlider && !workspaceEdgeDwellSlider.pressed) {
            workspaceEdgeDwellSlider.value = root.workspaceDragEdgeDwellMs
        }

        root.workspaceDragEdgeRepeatMs = prefInt("workspace.dragEdgeRepeatMs", 260)
        if (root.workspaceDragEdgeRepeatMs < 80) {
            root.workspaceDragEdgeRepeatMs = 80
        }
        if (root.workspaceDragEdgeRepeatMs > 700) {
            root.workspaceDragEdgeRepeatMs = 700
        }
        if (workspaceEdgeRepeatSlider && !workspaceEdgeRepeatSlider.pressed) {
            workspaceEdgeRepeatSlider.value = root.workspaceDragEdgeRepeatMs
        }
    }

    function syncWindowBindingPrefs() {
        root.bindMinimize = prefStr("windowing.bindMinimize", "Alt+F9")
        root.bindRestore = prefStr("windowing.bindRestore", "Alt+F10")
        root.bindSwitchNext = prefStr("windowing.bindSwitchNext", "Alt+F11")
        root.bindSwitchPrev = prefStr("windowing.bindSwitchPrev", "Alt+F12")
        root.bindWorkspace = prefStr("windowing.bindWorkspace", "Alt+F3")
        root.bindClose = prefStr("windowing.bindClose", "Alt+F4")
        root.bindMaximize = prefStr("windowing.bindMaximize", "Alt+Up")
        root.bindTileLeft = prefStr("windowing.bindTileLeft", "Alt+Left")
        root.bindTileRight = prefStr("windowing.bindTileRight", "Alt+Right")
        root.bindUntile = prefStr("windowing.bindUntile", "Alt+Down")
        root.bindQuarterTopLeft = prefStr("windowing.bindQuarterTopLeft", "Alt+Shift+Left")
        root.bindQuarterTopRight = prefStr("windowing.bindQuarterTopRight", "Alt+Shift+Right")
        root.bindQuarterBottomLeft = prefStr("windowing.bindQuarterBottomLeft", "Alt+Shift+Down")
        root.bindQuarterBottomRight = prefStr("windowing.bindQuarterBottomRight", "Alt+Shift+Up")
        minimizeBindField.text = root.bindMinimize
        restoreBindField.text = root.bindRestore
        switchNextBindField.text = root.bindSwitchNext
        switchPrevBindField.text = root.bindSwitchPrev
        workspaceBindField.text = root.bindWorkspace
        closeBindField.text = root.bindClose
        maximizeBindField.text = root.bindMaximize
        tileLeftBindField.text = root.bindTileLeft
        tileRightBindField.text = root.bindTileRight
        untileBindField.text = root.bindUntile
        quarterTopLeftBindField.text = root.bindQuarterTopLeft
        quarterTopRightBindField.text = root.bindQuarterTopRight
        quarterBottomLeftBindField.text = root.bindQuarterBottomLeft
        quarterBottomRightBindField.text = root.bindQuarterBottomRight
        validateWindowBindings()
        updateWindowBindingDirty()
    }

    function refreshGlobalMenuDiag() {
        if (typeof GlobalMenuManager === "undefined" || !GlobalMenuManager ||
                !GlobalMenuManager.diagnosticSnapshot) {
            root.globalMenuDiag = ({})
            return
        }
        var snap = GlobalMenuManager.diagnosticSnapshot()
        root.globalMenuDiag = snap ? snap : ({})
    }

    function copyGlobalMenuDiagSnapshot() {
        refreshGlobalMenuDiag()
        var payload = ""
        try {
            payload = JSON.stringify(root.globalMenuDiag || {}, null, 2)
        } catch (e) {
            payload = String(root.globalMenuDiag || "")
        }
        var copied = false
        if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.copyTextToClipboard) {
            copied = !!FileManagerApi.copyTextToClipboard(payload)
        }
        if (copied) {
            root.globalMenuDiagCopyStatus = "Snapshot copied to clipboard"
        } else {
            root.globalMenuDiagCopyStatus = "Clipboard unavailable"
        }
        globalMenuDiagCopyStatusTimer.restart()
    }

    Component.onCompleted: {
        syncIconThemeSelection()
        syncWindowEffectPrefs()
        syncWindowBindingPrefs()
        syncDockHidePrefs()
        refreshGlobalMenuDiag()
    }

    Timer {
        interval: 1200
        repeat: true
        running: root.visible
        onTriggered: root.refreshGlobalMenuDiag()
    }

    Timer {
        id: globalMenuDiagCopyStatusTimer
        interval: 2200
        repeat: false
        onTriggered: root.globalMenuDiagCopyStatus = ""
    }

    Connections { target: minimizeBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: restoreBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: switchNextBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: switchPrevBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: workspaceBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: closeBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: maximizeBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: tileLeftBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: tileRightBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: untileBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: quarterTopLeftBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: quarterTopRightBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: quarterBottomLeftBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }
    Connections { target: quarterBottomRightBindField; function onTextChanged() { root.validateWindowBindings(); root.updateWindowBindingDirty() } }

    Connections {
        target: ThemeIconController
        function onThemeMappingChanged() {
            root.syncIconThemeSelection()
        }
        function onAvailableThemesChanged() {
            root.syncIconThemeSelection()
        }
    }

    Connections {
        target: UIPreferences
        function onIconThemeLightChanged() {
            root.syncIconThemeSelection()
        }
        function onIconThemeDarkChanged() {
            root.syncIconThemeSelection()
        }
    }
}
