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
    function themeRows(list) {
        var rows = [{ "label": qsTr("(System default)"), "value": "" }]
        for (var i = 0; i < list.length; ++i) {
            var name = String(list[i] || "")
            rows.push({ "label": name, "value": name })
        }
        return rows
    }
    function themeRowsForCurrent(list, currentValue) {
        var rows = [{ "label": qsTr("(System default)"), "value": "" }]
        var current = String(currentValue || "").trim()
        var foundCurrent = false
        for (var i = 0; i < list.length; ++i) {
            var name = String(list[i] || "").trim()
            if (name.length <= 0) {
                continue
            }
            if (current.length > 0 && name === current) {
                foundCurrent = true
            }
            rows.push({ "label": name, "value": name })
        }
        if (current.length > 0 && !foundCurrent) {
            rows.splice(1, 0, {
                "label": current + qsTr(" (current)"),
                "value": current
            })
        }
        return rows
    }
    function themeIndexFromRows(rows, stored) {
        var s = String(stored || "").trim()
        if (s.length <= 0) {
            return 0
        }
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            if (String((row && row.value) ? row.value : "") === s) {
                return i
            }
        }
        return 0
    }
    function themeIndex(list, stored) {
        if (!stored || stored.length === 0) return 0
        const idx = list.indexOf(stored)
        return idx >= 0 ? idx + 1 : 0
    }
    function toBool(value, fallback) {
        if (value === undefined || value === null) {
            return !!fallback
        }
        if (typeof value === "boolean") {
            return value
        }
        if (typeof value === "number") {
            return value !== 0
        }
        var normalized = String(value).trim().toLowerCase()
        return normalized === "1"
                || normalized === "true"
                || normalized === "yes"
                || normalized === "on"
    }
    function settingBool(path, fallback) {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
            return !!fallback
        }
        return root.toBool(DesktopSettings.settingValue(path, fallback), fallback)
    }
    function settingString(path, fallback) {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
            return String(fallback || "")
        }
        var value = DesktopSettings.settingValue(path, fallback)
        return String(value === undefined || value === null ? fallback : value)
    }
    function pulseSettingBool(path, fallback) {
        return root.settingBool(path, fallback)
    }
    function pulseProfileIndex() {
        var profile = root.settingString("pulse.searchProfile", "balanced").trim().toLowerCase()
        if (profile === "apps-first") {
            return 1
        }
        if (profile === "files-first") {
            return 2
        }
        return 0
    }
    function pulseNormalizeDirectoryToken(token) {
        return String(token || "").trim()
    }
    function pulseDirectoryListFromSetting() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
            return []
        }
        var raw = DesktopSettings.settingValue("pulse.excludeScanDirectories", "")
        if (raw === undefined || raw === null) {
            return []
        }
        if (typeof raw === "string") {
            raw = raw.trim()
            if (raw.length <= 0) {
                return []
            }
            var parts = raw.split(/[;\n,]+/)
            var out = []
            for (var i = 0; i < parts.length; ++i) {
                var token = pulseNormalizeDirectoryToken(parts[i])
                if (token.length > 0) {
                    out.push(token)
                }
            }
            return out
        }
        if (raw && raw.length !== undefined) {
            var outList = []
            for (var j = 0; j < raw.length; ++j) {
                var tokenList = pulseNormalizeDirectoryToken(raw[j])
                if (tokenList.length > 0) {
                    outList.push(tokenList)
                }
            }
            return outList
        }
        var fallback = pulseNormalizeDirectoryToken(raw)
        return fallback.length > 0 ? [fallback] : []
    }
    function pulseDirectorySettingValue(entries) {
        var out = []
        var seen = {}
        for (var i = 0; i < entries.length; ++i) {
            var token = pulseNormalizeDirectoryToken(entries[i])
            var key = token.toLowerCase()
            if (token.length > 0 && !seen[key]) {
                seen[key] = true
                out.push(token)
            }
        }
        return out.join("; ")
    }
    function pulseUniqueEntries(entries) {
        var out = []
        var seen = {}
        for (var i = 0; i < entries.length; ++i) {
            var token = pulseNormalizeDirectoryToken(entries[i])
            var key = token.toLowerCase()
            if (token.length > 0 && !seen[key]) {
                seen[key] = true
                out.push(token)
            }
        }
        return out
    }
    function addPulseExcludeEntry() {
        var token = pulseNormalizeDirectoryToken(root.pulseExcludeDraft)
        if (token.length <= 0) {
            return
        }
        var next = root.pulseUniqueEntries(root.pulseExcludeEntries.concat([token]))
        root.pulseExcludeEntries = next
        root.pulseExcludeDraft = ""
        root.setPulseSetting("pulse.excludeScanDirectories", root.pulseDirectorySettingValue(next))
    }
    function setPulseSetting(path, value) {
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.setSettingValue) {
            DesktopSettings.setSettingValue(path, value)
        }
    }
    function syncPulseSourceSettings() {
        root.pulseIncludeApps = root.settingBool("pulse.includeApps", true)
        root.pulseIncludeRecent = root.settingBool("pulse.includeRecent", true)
        root.pulseIncludeClipboard = root.settingBool("pulse.includeClipboard", true)
        root.pulseIncludeTracker = root.settingBool("pulse.includeTracker", true)
        root.pulseIncludeActions = root.settingBool("pulse.includeActions", true)
        root.pulseIncludeSettings = root.settingBool("pulse.includeSettings", true)
        root.pulseEnablePreview = root.settingBool("pulse.enablePreview", true)
    }
    function syncPulseQuerySettings() {
        root.pulseResultLimit = Number(root.settingString("pulse.resultLimit", "24"))
        if (isNaN(root.pulseResultLimit) || root.pulseResultLimit < 8) {
            root.pulseResultLimit = 24
        } else if (root.pulseResultLimit > 256) {
            root.pulseResultLimit = 256
        }
        root.pulseAutoFocusOnOpen = root.settingBool("pulse.autoFocusOnOpen", true)
        root.pulseEnterOpensFirstResult = root.settingBool("pulse.enterOpensFirstResult", true)
        root.pulseAutoCloseAfterLaunch = root.settingBool("pulse.autoCloseAfterLaunch", true)
    }
    function syncPulseRankingSettings() {
        root.pulseBoostApps = Number(root.settingString("pulse.rankBoostApps", "0"))
        root.pulseBoostRecent = Number(root.settingString("pulse.rankBoostRecent", "0"))
        root.pulseBoostClipboard = Number(root.settingString("pulse.rankBoostClipboard", "0"))
        root.pulseBoostTracker = Number(root.settingString("pulse.rankBoostTracker", "0"))
        root.pulseBoostActions = Number(root.settingString("pulse.rankBoostActions", "0"))
        root.pulseBoostSettings = Number(root.settingString("pulse.rankBoostSettings", "0"))
        if (isNaN(root.pulseBoostApps)) root.pulseBoostApps = 0
        if (isNaN(root.pulseBoostRecent)) root.pulseBoostRecent = 0
        if (isNaN(root.pulseBoostClipboard)) root.pulseBoostClipboard = 0
        if (isNaN(root.pulseBoostTracker)) root.pulseBoostTracker = 0
        if (isNaN(root.pulseBoostActions)) root.pulseBoostActions = 0
        if (isNaN(root.pulseBoostSettings)) root.pulseBoostSettings = 0
    }
    function syncPulseTrackerSettings() {
        root.pulseTrackerInitialDelaySec = Number(root.settingString("pulse.trackerInitialDelaySec", "120"))
        root.pulseTrackerCpuLimit = Number(root.settingString("pulse.trackerCpuLimit", "15"))
        root.pulseTrackerIdleOnly = root.settingBool("pulse.trackerIdleOnly", true)
        root.pulseTrackerChargingOnly = root.settingBool("pulse.trackerChargingOnly", true)
        if (isNaN(root.pulseTrackerInitialDelaySec) || root.pulseTrackerInitialDelaySec < 0) {
            root.pulseTrackerInitialDelaySec = 120
        }
        if (isNaN(root.pulseTrackerCpuLimit) || root.pulseTrackerCpuLimit < 1) {
            root.pulseTrackerCpuLimit = 15
        }
    }

    Component.onCompleted: {
        root.pulseExcludeEntries = root.pulseDirectoryListFromSetting()
        root.syncPulseSourceSettings()
        root.syncPulseQuerySettings()
        root.syncPulseRankingSettings()
        root.syncPulseTrackerSettings()
        requestThemeAndIconComboSync()
        if (comboDebugEnabled) {
            console.warn("[appearance-combo-debug] loaded rev=" + comboDebugRevision)
            logThemeAndIconComboState("component-completed", true)
        }
    }

    Connections {
        target: DesktopSettings
        function onSettingChanged(path) {
            if (String(path || "") === "pulse.excludeScanDirectories") {
                root.pulseExcludeEntries = root.pulseDirectoryListFromSetting()
            } else if (String(path || "").indexOf("pulse.include") === 0) {
                root.syncPulseSourceSettings()
            } else if (String(path || "") === "pulse.enablePreview") {
                root.syncPulseSourceSettings()
            } else if (String(path || "") === "pulse.resultLimit") {
                root.syncPulseQuerySettings()
            } else if (String(path || "") === "pulse.autoFocusOnOpen"
                    || String(path || "") === "pulse.enterOpensFirstResult"
                    || String(path || "") === "pulse.autoCloseAfterLaunch") {
                root.syncPulseQuerySettings()
            } else if (String(path || "").indexOf("pulse.rankBoost") === 0) {
                root.syncPulseRankingSettings()
            } else if (String(path || "").indexOf("pulse.tracker") === 0) {
                root.syncPulseTrackerSettings()
            }
        }
    }

    // ── Nav model ──────────────────────────────────────────────────────────
    // Branding map (user-facing):
    // - AppHub  => app launcher overlay (internal: apphub)
    // - AppDeck => bottom app bar (internal: appdeck)
    // - Pulse   => quick search overlay (internal: pulse)
    // - Crown   => top deskcrown (internal: crown)

    readonly property var pages: [
        { id: "appearance", label: qsTr("Appearance"), icon: "preferences-desktop-theme"    },
        { id: "fonts",      label: qsTr("Fonts"),      icon: "preferences-desktop-font"     },
        { id: "theme",      label: qsTr("Theme"),      icon: "applications-graphics"        },
        { id: "icons",      label: qsTr("Icons"),      icon: "preferences-desktop-icons"    },
        { id: "appdeck",       label: qsTr("AppDeck"),     icon: "user-desktop"                 },
        { id: "desktop",    label: qsTr("Desktop"),    icon: "video-display"                },
        { id: "crown",     label: qsTr("Crown"),      icon: "go-top"                       },
    ]

    property int currentIndex: 0
    property bool _themeComboSyncPending: false
    property int _themeComboSyncRetries: 0
    property bool _ignoreComboActivated: false
    property var pulseExcludeEntries: []
    property string pulseExcludeDraft: ""
    property bool pulseIncludeApps: true
    property bool pulseIncludeRecent: true
    property bool pulseIncludeClipboard: true
    property bool pulseIncludeTracker: true
    property bool pulseIncludeActions: true
    property bool pulseIncludeSettings: true
    property bool pulseEnablePreview: true
    property int pulseResultLimit: 24
    property bool pulseAutoFocusOnOpen: true
    property bool pulseEnterOpensFirstResult: true
    property bool pulseAutoCloseAfterLaunch: true
    property int pulseBoostApps: 0
    property int pulseBoostRecent: 0
    property int pulseBoostClipboard: 0
    property int pulseBoostTracker: 0
    property int pulseBoostActions: 0
    property int pulseBoostSettings: 0
    property int pulseTrackerInitialDelaySec: 120
    property int pulseTrackerCpuLimit: 15
    property bool pulseTrackerIdleOnly: true
    property bool pulseTrackerChargingOnly: true
    readonly property bool comboDebugEnabled: false
    readonly property string comboDebugRevision: "appearance-combo-debug-r1"
    property string _lastComboDebugSnapshot: ""

    function comboTextIndex(combo, storedValue) {
        if (!combo) {
            return 0
        }
        var stored = String(storedValue || "").trim().toLowerCase()
        if (stored.length <= 0) {
            return 0
        }
        var n = Number(combo.count || 0)
        for (var i = 0; i < n; ++i) {
            var t = String(combo.textAt(i) || "").trim().toLowerCase()
            if (t === stored) {
                return i
            }
        }
        return 0
    }

    function syncComboToStoredValue(combo, storedValue) {
        if (!combo) {
            return
        }
        var idx = comboTextIndex(combo, storedValue)
        if (combo.currentIndex !== idx) {
            combo.currentIndex = idx
        }
    }

    function syncThemeAndIconCombos() {
        _ignoreComboActivated = true
        syncComboToStoredValue(gtkThemeLightCombo, DesktopSettings.gtkThemeLight)
        syncComboToStoredValue(gtkThemeDarkCombo, DesktopSettings.gtkThemeDark)
        syncComboToStoredValue(kdeColorSchemeLightCombo, DesktopSettings.kdeColorSchemeLight)
        syncComboToStoredValue(kdeColorSchemeDarkCombo, DesktopSettings.kdeColorSchemeDark)
        syncComboToStoredValue(gtkIconThemeLightCombo, DesktopSettings.gtkIconThemeLight)
        syncComboToStoredValue(gtkIconThemeDarkCombo, DesktopSettings.gtkIconThemeDark)
        syncComboToStoredValue(kdeIconThemeLightCombo, DesktopSettings.kdeIconThemeLight)
        syncComboToStoredValue(kdeIconThemeDarkCombo, DesktopSettings.kdeIconThemeDark)
        logThemeAndIconComboState("sync", false)
        Qt.callLater(function() {
            root._ignoreComboActivated = false
        })
    }

    function requestThemeAndIconComboSync() {
        if (_themeComboSyncPending) {
            return
        }
        _themeComboSyncPending = true
        Qt.callLater(function() {
            _themeComboSyncPending = false
            root.syncThemeAndIconCombos()
            root._themeComboSyncRetries = 4
            themeComboSyncTimer.restart()
        })
    }

    onCurrentIndexChanged: {
        if (currentIndex === 2 || currentIndex === 3) {
            requestThemeAndIconComboSync()
            logThemeAndIconComboState("tab-open-" + currentIndex, true)
        }
    }

    Timer {
        id: themeComboSyncTimer
        interval: 140
        repeat: true
        running: false
        onTriggered: {
            root.syncThemeAndIconCombos()
            root._themeComboSyncRetries = Math.max(0, root._themeComboSyncRetries - 1)
            if (root._themeComboSyncRetries <= 0) {
                stop()
            }
        }
    }

    Connections {
        target: DesktopSettings
        function onGtkThemeLightChanged() { root.requestThemeAndIconComboSync() }
        function onGtkThemeDarkChanged() { root.requestThemeAndIconComboSync() }
        function onKdeColorSchemeLightChanged() { root.requestThemeAndIconComboSync() }
        function onKdeColorSchemeDarkChanged() { root.requestThemeAndIconComboSync() }
        function onGtkIconThemeLightChanged() { root.requestThemeAndIconComboSync() }
        function onGtkIconThemeDarkChanged() { root.requestThemeAndIconComboSync() }
        function onKdeIconThemeLightChanged() { root.requestThemeAndIconComboSync() }
        function onKdeIconThemeDarkChanged() { root.requestThemeAndIconComboSync() }
    }

    Connections {
        target: ThemeManager
        function onGtkThemesChanged() { root.requestThemeAndIconComboSync() }
        function onKdeColorSchemesChanged() { root.requestThemeAndIconComboSync() }
        function onIconThemesChanged() { root.requestThemeAndIconComboSync() }
    }

    function logThemeAndIconComboState(reason, force) {
        if (!comboDebugEnabled) {
            return
        }
        var snapshot = String(DesktopSettings.gtkThemeLight || "")
        if (!force && snapshot === _lastComboDebugSnapshot) {
            return
        }
        _lastComboDebugSnapshot = snapshot
        var idx = gtkThemeLightCombo ? Number(gtkThemeLightCombo.currentIndex || 0) : -1
        var txt = gtkThemeLightCombo ? String(gtkThemeLightCombo.currentText || "") : "<null>"
        var cnt = gtkThemeLightCombo ? Number(gtkThemeLightCombo.count || 0) : -1
        console.warn("[appearance-combo-debug] reason=" + String(reason || "unknown")
                     + " rev=" + comboDebugRevision
                     + " pageIndex=" + Number(root.currentIndex || 0))
        console.warn("[appearance-combo-debug] gtkThemeLight stored=\"" + snapshot
                     + "\" idx=" + idx
                     + " text=\"" + txt
                     + "\" count=" + cnt)
    }

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
                        Behavior on color {
                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                        }
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
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: highlighted ? Theme.fontWeight("semibold") : Theme.fontWeight("normal")
                            color: highlighted ? Theme.color("accentText") : Theme.color("textPrimary")
                            elide: Text.ElideRight
                            Behavior on color {
                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                            }
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
                            SettingToggle {
                                checked: DesktopSettings.highContrast
                                onToggled: DesktopSettings.setHighContrast(checked)
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
                                id: gtkThemeLightCombo
                                model: root.themeModel(ThemeManager.gtkThemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
                                    DesktopSettings.setGtkThemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("GTK Theme — Dark")
                            description: qsTr("Applied to GTK 3/4 applications when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: gtkThemeDarkCombo
                                model: root.themeModel(ThemeManager.gtkThemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
                                    DesktopSettings.setGtkThemeDark(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Color Scheme — Light")
                            description: qsTr("Applied to KDE / Qt applications when light mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: kdeColorSchemeLightCombo
                                model: root.themeModel(ThemeManager.kdeColorSchemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
                                    DesktopSettings.setKdeColorSchemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Color Scheme — Dark")
                            description: qsTr("Applied to KDE / Qt applications when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: kdeColorSchemeDarkCombo
                                model: root.themeModel(ThemeManager.kdeColorSchemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
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
                                id: gtkIconThemeLightCombo
                                model: root.themeModel(ThemeManager.iconThemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
                                    DesktopSettings.setGtkIconThemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Icon Theme — Dark")
                            description: qsTr("Applied to system icons when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: gtkIconThemeDarkCombo
                                model: root.themeModel(ThemeManager.iconThemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
                                    DesktopSettings.setGtkIconThemeDark(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Icon Theme — Light")
                            description: qsTr("Applied to KDE / Qt applications when light mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: kdeIconThemeLightCombo
                                model: root.themeModel(ThemeManager.iconThemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
                                    DesktopSettings.setKdeIconThemeLight(name)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("KDE Icon Theme — Dark")
                            description: qsTr("Applied to KDE / Qt applications when dark mode is active.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: kdeIconThemeDarkCombo
                                model: root.themeModel(ThemeManager.iconThemes)
                                Layout.preferredWidth: 220
                                onActivated: {
                                    if (root._ignoreComboActivated) {
                                        return
                                    }
                                    const name = currentIndex === 0 ? "" : String(currentText || "")
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

            // ── 4: AppDeck ────────────────────────────────────────────────────
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
                        title: qsTr("AppDeck")
                        Layout.fillWidth: true

                        SettingCard {
                            label: qsTr("Icon Size")
                            description: qsTr("Set the size of AppDeck icons.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: [qsTr("Small"), qsTr("Medium"), qsTr("Large")]
                                readonly property var sizeKeys: ["small", "medium", "large"]
                                currentIndex: {
                                    var sz = DesktopSettings.dockIconSize || "medium"
                                    var idx = sizeKeys.indexOf(sz)
                                    return idx >= 0 ? idx : 1
                                }
                                onActivated: DesktopSettings.setDockIconSize(sizeKeys[currentIndex])
                                Layout.preferredWidth: 160
                            }
                        }

                        SettingCard {
                            label: qsTr("Appearance")
                            description: qsTr("Choose how AppDeck appears when idle.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: dockAppearanceCombo
                                model: [qsTr("Autohide"), qsTr("Duration"), qsTr("Always show")]
                                currentIndex: {
                                    var legacyAuto = !!DesktopSettings.settingValue("appdeck.autoHideEnabled", false)
                                    var mode = String(DesktopSettings.settingValue("appdeck.hideMode",
                                                                                   legacyAuto ? "duration_hide" : "no_hide"))
                                            .trim().toLowerCase()
                                    if (mode === "snart_hide") {
                                        mode = "smart_hide"
                                    }
                                    if (mode === "smart_hide") {
                                        return 0
                                    }
                                    if (mode === "duration_hide") {
                                        return 1
                                    }
                                    return 2
                                }
                                Layout.preferredWidth: 180
                                onActivated: {
                                    var mode = "duration_hide"
                                    if (currentIndex === 0) {
                                        mode = "smart_hide"
                                    } else if (currentIndex === 2) {
                                        mode = "no_hide"
                                    }
                                    DesktopSettings.setSettingValue("appdeck.hideMode", mode)
                                    DesktopSettings.setSettingValue("appdeck.autoHideEnabled", mode !== "no_hide")
                                    if (mode === "duration_hide") {
                                        var duration = Number(DesktopSettings.settingValue("appdeck.hideDurationMs", 450))
                                        if (isNaN(duration) || duration < 100 || duration > 5000) {
                                            DesktopSettings.setSettingValue("appdeck.hideDurationMs", 450)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── 5: Pulse ──────────────────────────────────────────────────────
            Flickable {
                contentHeight: pulseCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: pulseCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
                    spacing: 24

                    SettingGroup {
                        title: qsTr("Pulse")
                        Layout.fillWidth: true

                        SettingCard {
                            label: qsTr("Search Profile")
                            description: qsTr("Choose how Pulse ranks results.")
                            Layout.fillWidth: true
                            ComboBox {
                                model: [qsTr("Balanced"), qsTr("Apps First"), qsTr("Files First")]
                                currentIndex: root.pulseProfileIndex()
                                Layout.preferredWidth: 180
                                onActivated: function(index) {
                                    var profile = "balanced"
                                    if (index === 1) {
                                        profile = "apps-first"
                                    } else if (index === 2) {
                                        profile = "files-first"
                                    }
                                    root.setPulseSetting("pulse.searchProfile", profile)
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Search Sources")
                            description: qsTr("Choose which data sources Pulse includes in search results.")
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Apps")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseIncludeApps
                                        onToggled: {
                                            root.pulseIncludeApps = checked
                                            root.setPulseSetting("pulse.includeApps", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Recent files")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseIncludeRecent
                                        onToggled: {
                                            root.pulseIncludeRecent = checked
                                            root.setPulseSetting("pulse.includeRecent", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Clipboard")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseIncludeClipboard
                                        onToggled: {
                                            root.pulseIncludeClipboard = checked
                                            root.setPulseSetting("pulse.includeClipboard", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Tracker")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseIncludeTracker
                                        onToggled: {
                                            root.pulseIncludeTracker = checked
                                            root.setPulseSetting("pulse.includeTracker", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Actions")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseIncludeActions
                                        onToggled: {
                                            root.pulseIncludeActions = checked
                                            root.setPulseSetting("pulse.includeActions", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Settings")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseIncludeSettings
                                        onToggled: {
                                            root.pulseIncludeSettings = checked
                                            root.setPulseSetting("pulse.includeSettings", checked)
                                        }
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Preview Behavior")
                            description: qsTr("Control whether Pulse shows inline previews for results.")
                            Layout.fillWidth: true
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Enable preview")
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                }
                                SettingToggle {
                                    checked: root.pulseEnablePreview
                                    onToggled: {
                                        root.pulseEnablePreview = checked
                                        root.setPulseSetting("pulse.enablePreview", checked)
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Result Limit")
                            description: qsTr("Maximum number of results Pulse shows for a query.")
                            Layout.fillWidth: true
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Maximum results")
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                }
                                SpinBox {
                                    id: pulseResultLimitSpin
                                    from: 8
                                    to: 256
                                    stepSize: 4
                                    editable: true
                                    value: root.pulseResultLimit
                                    Layout.preferredWidth: 140
                                    onValueModified: {
                                        root.pulseResultLimit = value
                                        root.setPulseSetting("pulse.resultLimit", value)
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Search Behavior")
                            description: qsTr("Control how Pulse focuses and launches results.")
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Auto-focus on open")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseAutoFocusOnOpen
                                        onToggled: {
                                            root.pulseAutoFocusOnOpen = checked
                                            root.setPulseSetting("pulse.autoFocusOnOpen", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Enter opens first result")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseEnterOpensFirstResult
                                        onToggled: {
                                            root.pulseEnterOpensFirstResult = checked
                                            root.setPulseSetting("pulse.enterOpensFirstResult", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Auto-close after launch")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseAutoCloseAfterLaunch
                                        onToggled: {
                                            root.pulseAutoCloseAfterLaunch = checked
                                            root.setPulseSetting("pulse.autoCloseAfterLaunch", checked)
                                        }
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Ranking Tuning")
                            description: qsTr("Adjust how strongly each result source is boosted.")
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Apps")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseBoostAppsSpin
                                        from: -40
                                        to: 80
                                        stepSize: 1
                                        editable: true
                                        value: root.pulseBoostApps
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseBoostApps = value
                                            root.setPulseSetting("pulse.rankBoostApps", value)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Recent files")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseBoostRecentSpin
                                        from: -40
                                        to: 80
                                        stepSize: 1
                                        editable: true
                                        value: root.pulseBoostRecent
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseBoostRecent = value
                                            root.setPulseSetting("pulse.rankBoostRecent", value)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Clipboard")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseBoostClipboardSpin
                                        from: -40
                                        to: 80
                                        stepSize: 1
                                        editable: true
                                        value: root.pulseBoostClipboard
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseBoostClipboard = value
                                            root.setPulseSetting("pulse.rankBoostClipboard", value)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Tracker")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseBoostTrackerSpin
                                        from: -40
                                        to: 80
                                        stepSize: 1
                                        editable: true
                                        value: root.pulseBoostTracker
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseBoostTracker = value
                                            root.setPulseSetting("pulse.rankBoostTracker", value)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Actions")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseBoostActionsSpin
                                        from: -40
                                        to: 80
                                        stepSize: 1
                                        editable: true
                                        value: root.pulseBoostActions
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseBoostActions = value
                                            root.setPulseSetting("pulse.rankBoostActions", value)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Settings")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseBoostSettingsSpin
                                        from: -40
                                        to: 80
                                        stepSize: 1
                                        editable: true
                                        value: root.pulseBoostSettings
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseBoostSettings = value
                                            root.setPulseSetting("pulse.rankBoostSettings", value)
                                        }
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Tracker Policy")
                            description: qsTr("Tune when indexed file search is allowed to run.")
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Initial delay")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseTrackerDelaySpin
                                        from: 0
                                        to: 3600
                                        stepSize: 10
                                        editable: true
                                        value: root.pulseTrackerInitialDelaySec
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseTrackerInitialDelaySec = value
                                            root.setPulseSetting("pulse.trackerInitialDelaySec", value)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("CPU limit")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SpinBox {
                                        id: pulseTrackerCpuSpin
                                        from: 1
                                        to: 100
                                        editable: true
                                        value: root.pulseTrackerCpuLimit
                                        textFromValue: function(v) { return String(v) + "%" }
                                        valueFromText: function(t) {
                                            var n = parseInt(String(t).replace("%", ""))
                                            return isNaN(n) ? 15 : n
                                        }
                                        Layout.preferredWidth: 140
                                        onValueModified: {
                                            root.pulseTrackerCpuLimit = value
                                            root.setPulseSetting("pulse.trackerCpuLimit", value)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Idle only")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseTrackerIdleOnly
                                        onToggled: {
                                            root.pulseTrackerIdleOnly = checked
                                            root.setPulseSetting("pulse.trackerIdleOnly", checked)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Charging only")
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                    }
                                    SettingToggle {
                                        checked: root.pulseTrackerChargingOnly
                                        onToggled: {
                                            root.pulseTrackerChargingOnly = checked
                                            root.setPulseSetting("pulse.trackerChargingOnly", checked)
                                        }
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Exclude scan directory")
                            description: qsTr("Directories ignored by Pulse tracker scans.")
                            Layout.fillWidth: true
                            ColumnLayout {
                                spacing: 8
                                Layout.fillWidth: true

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Repeater {
                                        model: root.pulseExcludeEntries
                                        delegate: Rectangle {
                                            radius: Theme.radiusPill
                                            color: Theme.color("panelBorder")
                                            border.color: Theme.color("panelBorderStrong")
                                            border.width: Theme.borderWidthThin
                                            implicitHeight: 32
                                            implicitWidth: chipRow.implicitWidth + 18

                                            RowLayout {
                                                id: chipRow
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 8
                                                spacing: 8

                                                Text {
                                                    text: String(modelData || "")
                                                    color: Theme.color("textPrimary")
                                                    font.pixelSize: Theme.fontSize("small")
                                                    elide: Text.ElideRight
                                                }

                                                ToolButton {
                                                    text: "x"
                                                    Layout.preferredWidth: 22
                                                    Layout.preferredHeight: 22
                                                    onClicked: {
                                                        var next = root.pulseExcludeEntries.slice(0)
                                                        next.splice(index, 1)
                                                        root.pulseExcludeEntries = next
                                                        root.setPulseSetting("pulse.excludeScanDirectories",
                                                                             root.pulseDirectorySettingValue(next))
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    TextField {
                                        id: pulseExcludeField
                                        text: root.pulseExcludeDraft
                                        placeholderText: qsTr("/home/user/Downloads")
                                        selectByMouse: true
                                        Layout.fillWidth: true
                                        onTextEdited: root.pulseExcludeDraft = text
                                        onAccepted: root.addPulseExcludeEntry()
                                    }

                                    Button {
                                        text: qsTr("Add")
                                        onClicked: root.addPulseExcludeEntry()
                                    }
                                }

                                Label {
                                    text: qsTr("You can use absolute paths or simple directory names.")
                                    color: Theme.color("textSecondary")
                                    font.pixelSize: Theme.fontSize("small")
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Notify Clipboard Resolve Success")
                            description: qsTr("Show a success hint when Pulse resolves a clipboard item.")
                            Layout.fillWidth: true
                            SettingToggle {
                                checked: root.settingBool("pulse.notifyClipboardResolveSuccess", true)
                                onToggled: root.setPulseSetting("pulse.notifyClipboardResolveSuccess", checked)
                            }
                        }
                    }
                }
            }

            // ── 6: Desktop ─────────────────────────────────────────────────
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

            // ── 7: Topbar ──────────────────────────────────────────────────
            Flickable {
                contentHeight: crownCol.implicitHeight + 48
                clip: true

                ColumnLayout {
                    id: crownCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24; anchors.topMargin: 24
                    spacing: 24

                    // ── Appearance ─────────────────────────────────────────

                    SettingGroup {
                        title: qsTr("Appearance")
                        Layout.fillWidth: true

                        SettingCard {
                            label: qsTr("Panel Style")
                            description: qsTr("Choose how the top panel is displayed.")
                            Layout.fillWidth: true
                            ComboBox {
                                id: panelStyleCombo
                                readonly property var styleKeys: ["floating", "attached"]
                                model: [qsTr("Floating"), qsTr("Attached")]
                                currentIndex: {
                                    var v = DesktopSettings.settingValue("shellTheme.panelStyle", "floating")
                                    var idx = styleKeys.indexOf(String(v))
                                    return idx >= 0 ? idx : 0
                                }
                                Layout.preferredWidth: 160
                                onActivated: DesktopSettings.setSettingValue("shellTheme.panelStyle", styleKeys[currentIndex])
                                Connections {
                                    target: DesktopSettings
                                    function onSettingChanged(path) {
                                        if (path === "shellTheme.panelStyle") {
                                            var v = DesktopSettings.settingValue("shellTheme.panelStyle", "floating")
                                            var idx = panelStyleCombo.styleKeys.indexOf(String(v))
                                            panelStyleCombo.currentIndex = idx >= 0 ? idx : 0
                                        }
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Transparent Background")
                            description: qsTr("Remove the panel background color.")
                            Layout.fillWidth: true
                            SettingToggle {
                                id: crownTransparentToggle
                                checked: DesktopSettings.settingValue("shellTheme.crownTransparent", false) === true
                                onToggled: DesktopSettings.setSettingValue("shellTheme.crownTransparent", checked)
                                Connections {
                                    target: DesktopSettings
                                    function onSettingChanged(path) {
                                        if (path === "shellTheme.crownTransparent") {
                                            crownTransparentToggle.checked = DesktopSettings.settingValue("shellTheme.crownTransparent", false) === true
                                        }
                                    }
                                }
                            }
                        }

                        SettingCard {
                            label: qsTr("Blur")
                            description: qsTr("Apply a blur effect behind the top panel.")
                            Layout.fillWidth: true
                            enabled: !crownTransparentToggle.checked
                            opacity: enabled ? 1.0 : Theme.opacityHint
                            SettingToggle {
                                id: crownBlurToggle
                                checked: DesktopSettings.settingValue("shellTheme.blur", true) !== false
                                onToggled: DesktopSettings.setSettingValue("shellTheme.blur", checked)
                                Connections {
                                    target: DesktopSettings
                                    function onSettingChanged(path) {
                                        if (path === "shellTheme.blur") {
                                            crownBlurToggle.checked = DesktopSettings.settingValue("shellTheme.blur", true) !== false
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ── Applets ────────────────────────────────────────────

                    SettingGroup {
                        title: qsTr("Applets")
                        Layout.fillWidth: true

                        // ── Default applets before Network ─────────────────
                        Repeater {
                            model: [
                                { key: "sound", label: qsTr("Sound"), description: qsTr("Volume control and audio output."), locked: true },
                            ]
                            delegate: SettingCard {
                                required property var modelData
                                label: modelData.label; description: modelData.description
                                Layout.fillWidth: true
                                SettingToggle { enabled: false; checked: true }
                            }
                        }

                        // ── Network + Show IP sub-row (no separator between) ─
                        SettingCard {
                            label: qsTr("Network")
                            description: qsTr("Wi-Fi and network connection status.")
                            Layout.fillWidth: true
                            hideSeparator: true
                            SettingToggle { enabled: false; checked: true }
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: showIpRow.implicitHeight + 20

                            RowLayout {
                                id: showIpRow
                                anchors {
                                    left: parent.left; right: parent.right
                                    verticalCenter: parent.verticalCenter
                                    leftMargin: 32; rightMargin: 16
                                }
                                spacing: 16

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: qsTr("Show IP Address")
                                        font.pixelSize: Theme.fontSize("body")
                                        font.weight: Theme.fontWeight("medium")
                                        color: Theme.color("textPrimary")
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: qsTr("Display IP address section in the Network popup.")
                                        font.pixelSize: Theme.fontSize("small")
                                        color: Theme.color("textSecondary")
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                SettingToggle {
                                    id: showIpToggle

                                    function sync() {
                                        checked = (typeof DesktopSettings !== "undefined" && DesktopSettings)
                                                  ? DesktopSettings.settingValue("shellTheme.networkShowIp", false) === true
                                                  : false
                                    }

                                    Component.onCompleted: sync()
                                    onToggled: DesktopSettings.setSettingValue("shellTheme.networkShowIp", checked)

                                    Connections {
                                        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
                                        function onAvailableChanged() { showIpToggle.sync() }
                                        function onSettingChanged(path) {
                                            if (path === "shellTheme.networkShowIp") showIpToggle.sync()
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 16 }
                                height: 1
                                color: Theme.color("panelBorder")
                            }
                        }

                        // ── Remaining applets ───────────────────────────────
                        Repeater {
                            model: [
                                { key: "pulse",     label: qsTr("Search"),         description: qsTr("Quick search button (Pulse)."),             locked: true  },
                                { key: "controlcenter", label: qsTr("Control Center"), description: qsTr("Quick settings panel."),                    locked: true  },
                                { key: "datetime",      label: qsTr("Date & Time"),    description: qsTr("Clock and calendar popup."),                locked: true  },
                                { key: "bluetooth",     label: qsTr("Bluetooth"),      description: qsTr("Bluetooth device management."),             locked: false },
                                { key: "battery",       label: qsTr("Battery"),        description: qsTr("Battery level and power status."),          locked: true  },
                                { key: "notification",  label: qsTr("Notifications"),  description: qsTr("Notification history and do-not-disturb."), locked: false },
                                { key: "clipboard",     label: qsTr("Clipboard"),      description: qsTr("Clipboard history manager."),               locked: false },
                                { key: "print",         label: qsTr("Print Jobs"),     description: qsTr("Active print job status."),                 locked: false },
                            ]
                            delegate: SettingCard {
                                required property var modelData
                                label: modelData.label; description: modelData.description
                                Layout.fillWidth: true
                                SettingToggle {
                                    id: appletToggle
                                    readonly property bool locked: modelData.locked === true
                                    readonly property string settingPath: "shellTheme.applets." + modelData.key
                                    enabled: !locked
                                    checked: locked || DesktopSettings.settingValue(settingPath, false) === true
                                    onToggled: if (!locked) DesktopSettings.setSettingValue(settingPath, checked)
                                    Connections {
                                        target: DesktopSettings
                                        function onSettingChanged(path) {
                                            if (!appletToggle.locked && path === appletToggle.settingPath)
                                                appletToggle.checked = DesktopSettings.settingValue(appletToggle.settingPath, false) === true
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
