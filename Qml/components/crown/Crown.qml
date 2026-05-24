import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle
import "." as TB

// Branding definition:
// - Crown = user-facing name for the desktop crown.
// - Internal state/config may continue using "crown" naming.
// - New UI copy should use "Crown".

Rectangle {
    id: root
    property var fileManagerContent: null
    property var desktopMenuProvider: null
    property var rootWindow: null
    property bool deferredReady: true
    readonly property bool startupTraceEnabled: (typeof StartupTraceEnabled !== "undefined") ? !!StartupTraceEnabled : false
    property var popupHost: null
    readonly property var resolvedPopupHost: popupHost ? popupHost : ((parent && parent.height > 0) ? parent : root)
    readonly property int panelInset: Theme.metric("spacingLg")
    readonly property int panelGap: Theme.metric("spacingLg")
    readonly property int iconButtonW: Theme.metric("controlHeightRegular")
    readonly property int iconButtonH: Theme.metric("controlHeightCompact")
    readonly property int iconGlyph: 18
    readonly property int popupInset: Theme.metric("spacingLg")
    readonly property int popupGap: Theme.metric("spacingSm")
    readonly property int popupControlH: Theme.metric("controlHeightLarge")
    readonly property int popupDebounceMs: 220
    readonly property int popupHintMs: 320

    signal launcherRequested()
    signal pulseRequested()
    signal screenshotCaptureRequested(string mode, int delaySec, bool grabPointer, bool concealText)
    signal startupItemsReadyReached()
    property string timeText: ""
    property int panelTextYOffset: 0
    property bool popupOpenHint: false
    property double mainMenuLastCloseMs: 0
    property var searchProfilesModel: []
    property string activeSearchProfileId: "balanced"
    property bool indicatorsRegistered: false
    property var stagedIndicatorQueue: []
    readonly property bool mainMenuPopupOpen: !!(mainMenuControlLoader.item && mainMenuControlLoader.item.popupOpen)
    readonly property bool screenshotPopupOpen: !!(screenshotControlLoader.item && screenshotControlLoader.item.popupOpen)
    readonly property bool indicatorPopupOpen: !!(indicatorManagerLoader.item && indicatorManagerLoader.item.itemPopupOpen)
    readonly property bool globalMenuPopupOpen: !!brandSection && !!brandSection.anyPopupOpen
    readonly property bool startupItemsReady: !!deferredReady
                                             && !!indicatorsRegistered
                                             && mainMenuControlLoader.status === Loader.Ready
                                             && screenshotControlLoader.status === Loader.Ready
                                             && indicatorManagerLoader.status === Loader.Ready
    readonly property bool anyPopupOpen: popupOpenHint
                                        || mainMenuPopupOpen
                                        || screenshotPopupOpen
                                        || indicatorPopupOpen
                                        || globalMenuPopupOpen

    Timer {
        id: popupHintTimer
        interval: root.popupHintMs
        repeat: false
        onTriggered: root.popupOpenHint = false
    }

    property bool crownTransparent: false

    color: root.crownTransparent ? "transparent" : Theme.color("surface")
    border.color: root.crownTransparent ? "transparent" : Theme.color("panelBorder")
    border.width: root.crownTransparent ? Theme.borderWidthNone : Theme.borderWidthThin

    Behavior on color {
        ColorAnimation {
            duration: Theme.transitionDuration
            easing.type: Theme.easingStandard
        }
    }

    function recentlyClosed(lastCloseMs, debounceMs) {
        var d = debounceMs === undefined ? root.popupDebounceMs : debounceMs
        return (Date.now() - Number(lastCloseMs || 0)) < d
    }

    function openScreenshotPopup() {
        if (screenshotControlLoader.item && screenshotControlLoader.item.openPopup) {
            screenshotControlLoader.item.openPopup()
        }
    }

    function startupQmlMark(phase, detail) {
        if (!startupTraceEnabled)
            return
        var text = "[startup-qml] phase=" + String(phase || "")
        if (detail !== undefined && detail !== null && String(detail).length > 0) {
            text += " detail=" + String(detail)
        }
        text += " t=" + Date.now()
        console.warn(text)
    }

    function fallbackSearchProfiles() {
        return [
                    { "id": "balanced", "label": "Balanced" },
                    { "id": "apps-first", "label": "Apps First" },
                    { "id": "files-first", "label": "Files First" }
               ]
    }

    function normalizeProfileId(idValue) {
        var id = String(idValue || "").toLowerCase()
        if (id === "apps-first" || id === "files-first" || id === "balanced") {
            return id
        }
        return "balanced"
    }

    function refreshSearchProfilesModel() {
        var profiles = []
        if (typeof PulseService !== "undefined" && PulseService && PulseService.searchProfiles) {
            var rows = PulseService.searchProfiles()
            for (var i = 0; i < rows.length; ++i) {
                var row = rows[i]
                if (!row || !row.id) {
                    continue
                }
                profiles.push({
                                  "id": normalizeProfileId(row.id),
                                  "label": String(row.label || row.id),
                                  "description": String(row.description || "")
                              })
            }
        }
        if (profiles.length <= 0) {
            profiles = fallbackSearchProfiles()
        }
        searchProfilesModel = profiles

        var active = "balanced"
        if (typeof PulseService !== "undefined" && PulseService && PulseService.activeSearchProfile) {
            active = normalizeProfileId(PulseService.activeSearchProfile())
        } else if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.settingValue) {
            active = normalizeProfileId(DesktopSettings.settingValue("pulse.searchProfile", "balanced"))
        }
        activeSearchProfileId = active
    }

    readonly property var defaultApplets: ["sound", "network", "controlpanel", "datetime", "battery"]

    function appletEnabled(name) {
        var n = String(name)
        if (root.defaultApplets.indexOf(n) >= 0) return true
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue)
            return false
        return DesktopSettings.settingValue("shellTheme.applets." + n, false) === true
    }

    function applyNonStagedAppletSettings() {
        var nonStaged = ["notification", "clipboard", "battery", "datetime"]
        for (var i = 0; i < nonStaged.length; i++) {
            var n = nonStaged[i]
            IndicatorRegistry.setIndicatorEnabledByName(n, appletEnabled(n))
        }
    }

    function registerCoreIndicators() {
        if (indicatorsRegistered) {
            return
        }
        indicatorsRegistered = true

        IndicatorRegistry.registerIndicator({
            name: "screencast",
            source: "../applet/ScreencastIndicator.qml",
            order: 100,
            visible: true
        })
        IndicatorRegistry.registerIndicator({
            name: "inputcapture",
            source: "../applet/InputCaptureIndicator.qml",
            order: 150,
            visible: true
        })
        IndicatorRegistry.registerIndicator({
            name: "controlpanel",
            source: "../applet/ControlPanelApplet.qml",
            order: 890,
            enabled: false,
            properties: { popupHost: root.resolvedPopupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "network",
            source: "../applet/NetworkApplet.qml",
            order: 200,
            enabled: false,
            properties: { showText: true, popupHost: root.resolvedPopupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "bluetooth",
            source: "../applet/BluetoothApplet.qml",
            order: 300,
            enabled: false,
            properties: { popupHost: root.resolvedPopupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "sound",
            source: "../applet/SoundApplet.qml",
            order: 400,
            enabled: false,
            properties: { popupHost: root.resolvedPopupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "print",
            source: "../applet/PrintJobApplet.qml",
            order: 450,
            enabled: false,
            properties: { popupHost: root.resolvedPopupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "notification",
            source: "../applet/NotificationApplet.qml",
            order: 500,
            properties: { popupHost: root.resolvedPopupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "clipboard",
            source: "../applet/ClipboardApplet.qml",
            order: 600,
            properties: { popupHost: root.resolvedPopupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "battery",
            source: "../applet/BatteryApplet.qml",
            order: 700,
            properties: { popupHost: root.resolvedPopupHost, textYOffset: root.panelTextYOffset }
        })
        IndicatorRegistry.registerIndicator({
            name: "batch_ops",
            source: "../applet/BatchOperationIndicator.qml",
            order: 800,
            properties: { fileManagerApi: FileManagerApi }
        })
        IndicatorRegistry.registerIndicator({
            name: "datetime",
            source: "../applet/DatetimeApplet.qml",
            order: 900,
            properties: { popupHost: root.resolvedPopupHost, timeText: root.timeText }
        })
        applyNonStagedAppletSettings()
        stagedIndicatorQueue = ["network", "bluetooth", "sound", "print", "controlpanel"]
        stagedIndicatorTimer.restart()
    }

    function normalizedFontScale(value) {
        var n = Number(value)
        if (!isFinite(n) || n <= 0) {
            n = 1.0
        }
        return Math.max(0.9, Math.min(1.25, n))
    }

    function setFontScale(scaleValue) {
        var normalized = normalizedFontScale(scaleValue)
        Theme.userFontScale = normalized
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.setSettingValue) {
            DesktopSettings.setSettingValue("ui.fontScale", normalized)
        }
    }

    function updateTimeText() {
        var now = new Date()
        var parts = []

        // Show Day of Week
        if (DesktopSettings.settingValue("timedate/show-day-of-week", true)) {
            parts.push(Qt.formatDateTime(now, "ddd"))
        }

        // Show Date
        if (DesktopSettings.settingValue("timedate/show-date", true)) {
            parts.push(Qt.formatDateTime(now, "MMM d"))
        }

        // Time format
        var timeFormat = ""
        var use24 = DesktopSettings.settingValue("timedate/use-24-hour", false)
        if (use24) {
            timeFormat = "HH:mm"
        } else {
            timeFormat = "h:mm AP"
        }

        // Show Seconds
        if (DesktopSettings.settingValue("timedate/show-seconds", false)) {
            timeFormat += ":ss"
        }

        parts.push(Qt.formatDateTime(now, timeFormat))
        root.timeText = parts.join("  ")
    }

    Timer {
        id: clockTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.updateTimeText()
    }

    Component.onCompleted: {
        startupQmlMark("crown.onCompleted.begin")
        root.updateTimeText()
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.settingValue) {
            root.crownTransparent = DesktopSettings.settingValue("shellTheme.crownTransparent", false) === true
        }
        Qt.callLater(function() {
            startupQmlMark("crown.deferredInit.begin")
            refreshSearchProfilesModel()
            registerCoreIndicators()
            startupQmlMark("crown.deferredInit.end")
        })
        startupQmlMark("crown.onCompleted.end")
    }

    onDeferredReadyChanged: {
        if (!deferredReady) {
            return
        }
        startupQmlMark("crown.deferredReady")
        Qt.callLater(function() {
            registerCoreIndicators()
        })
    }

    onStartupItemsReadyChanged: {
        if (startupItemsReady) {
            startupQmlMark("crown.startupItems.ready")
            startupItemsReadyReached()
        }
    }

    Timer {
        id: stagedIndicatorTimer
        interval: 180
        repeat: true
        running: false
        onTriggered: {
            if (!stagedIndicatorQueue || stagedIndicatorQueue.length <= 0) {
                stop()
                root.startupQmlMark("crown.stagedIndicators.done")
                return
            }
            var next = stagedIndicatorQueue[0]
            stagedIndicatorQueue = stagedIndicatorQueue.slice(1)
            if (typeof IndicatorRegistry !== "undefined" && IndicatorRegistry
                    && IndicatorRegistry.setIndicatorEnabledByName) {
                IndicatorRegistry.setIndicatorEnabledByName(next, root.appletEnabled(next))
                root.startupQmlMark("crown.stagedIndicator.enabled", "name=" + String(next))
            }
            if (stagedIndicatorQueue.length <= 0) {
                stop()
                root.startupQmlMark("crown.stagedIndicators.done")
            }
        }
    }

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onSettingChanged(path) {
            var k = String(path || "")
            if (k === "pulse/searchProfile" || k === "pulse.searchProfile") {
                root.activeSearchProfileId = root.normalizeProfileId(
                            DesktopSettings.settingValue("pulse.searchProfile", "balanced"))
            } else if (k === "ui/fontScale" || k === "ui.fontScale") {
                Theme.userFontScale = root.normalizedFontScale(
                            DesktopSettings.settingValue("ui.fontScale", 1.0))
            } else if (k === "shellTheme.crownTransparent") {
                root.crownTransparent = DesktopSettings.settingValue("shellTheme.crownTransparent", false) === true
            } else if (k.startsWith("timedate/")) {
                root.updateTimeText()
            } else if (k.startsWith("shellTheme.applets.")) {
                var appletName = k.slice("shellTheme.applets.".length)
                if (appletName === "pulse") {
                    searchButton.visible = true
                } else if (typeof IndicatorRegistry !== "undefined" && IndicatorRegistry
                           && IndicatorRegistry.setIndicatorEnabledByName) {
                    IndicatorRegistry.setIndicatorEnabledByName(appletName, root.appletEnabled(appletName))
                }
            }
        }
    }

    Connections {
        target: (typeof PulseService !== "undefined") ? PulseService : null
        function onSearchProfileChanged(profileId) {
            root.activeSearchProfileId = root.normalizeProfileId(profileId)
        }
    }

    Connections {
        target: (typeof GlobalMenuManager !== "undefined") ? GlobalMenuManager : null
        function onOverrideMenuActivated(menuId, label, context) {
            if (String(context || "") === "slm-dbus-menu") {
                if (mainMenuControlLoader.item && !mainMenuControlLoader.item.popupOpen) {
                    mainMenuControlLoader.item.openPopup()
                }
            }
        }
    }

    Row {
        id: leftCluster
        anchors.left: parent.left
        anchors.leftMargin: root.panelInset
        anchors.verticalCenter: parent.verticalCenter
        height: Math.max(root.iconButtonH, Math.min(parent.height, Theme.metric("topBarHeight")))
        spacing: Theme.metric("spacingXs")

        Loader {
            id: mainMenuControlLoader
            width: item ? item.implicitWidth : root.iconButtonW
            // Match the row height so the logo's centerIn-parent lands on the
            // bar's true vertical center; the inner button is only iconButtonH
            // tall, which would top-align inside a taller Row.
            height: leftCluster.height
            active: root.deferredReady
            asynchronous: true
            sourceComponent: Component {
                TB.CrownMainMenuControl {
                    iconButtonW: root.iconButtonW
                    iconButtonH: root.iconButtonH
                    iconGlyph: root.iconGlyph
                    popupHost: root.resolvedPopupHost
                    rootWindow: root.rootWindow
                    popupGap: root.popupGap
                    searchProfilesModel: root.searchProfilesModel
                    onPopupHintRequested: {
                        root.popupOpenHint = true
                        popupHintTimer.restart()
                    }
                    onPopupHintCleared: {
                        root.popupOpenHint = false
                        root.mainMenuLastCloseMs = Date.now()
                    }
                    onSearchProfileSetRequested: function(profileId) {
                        root.activeSearchProfileId = profileId
                        var handled = false
                        if (typeof PulseService !== "undefined" && PulseService
                                && PulseService.setActiveSearchProfile) {
                            handled = !!PulseService.setActiveSearchProfile(profileId)
                        }
                        if (!handled && typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setSettingValue) {
                            DesktopSettings.setSettingValue("pulse.searchProfile", profileId)
                        }
                    }
                    onSearchProfileResetRequested: {
                        root.activeSearchProfileId = "balanced"
                        var handled = false
                        if (typeof PulseService !== "undefined" && PulseService
                                && PulseService.setActiveSearchProfile) {
                            handled = !!PulseService.setActiveSearchProfile("balanced")
                        }
                        if (!handled && typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setSettingValue) {
                            DesktopSettings.setSettingValue("pulse.searchProfile", "balanced")
                        }
                        root.refreshSearchProfilesModel()
                    }
                    onRefreshSearchProfilesRequested: root.refreshSearchProfilesModel()
                }
            }
        }
        TB.CrownBrandSection {
            id: brandSection
            height: leftCluster.height
            fileManagerContent: root.fileManagerContent
            desktopMenuProvider: root.desktopMenuProvider
            menuBarHeight: leftCluster.height
        }

    }

    Loader {
        id: screenshotControlLoader
        anchors.right: searchButton.left
        anchors.rightMargin: Theme.metric("spacingSm")
        anchors.verticalCenter: parent.verticalCenter
        active: root.deferredReady
        asynchronous: true
        sourceComponent: Component {
            TB.CrownScreenshotControl {
                iconButtonW: root.iconButtonW
                iconButtonH: root.iconButtonH
                iconGlyph: root.iconGlyph
                popupInset: root.popupInset
                popupGap: root.popupGap
                popupControlH: root.popupControlH
                popupHost: root.resolvedPopupHost
                onPopupHintRequested: {
                    root.popupOpenHint = true
                    popupHintTimer.restart()
                }
                onPopupHintCleared: root.popupOpenHint = false
                onScreenshotCaptureRequested: function(mode, delaySec, grabPointer, concealText) {
                    root.screenshotCaptureRequested(mode, delaySec, grabPointer, concealText)
                }
            }
        }
    }

    TB.CrownSearchButton {
        id: searchButton
        anchors.right: indicatorManagerLoader.left
        anchors.rightMargin: visible ? Theme.metric("spacingSm") : 0
        anchors.verticalCenter: parent.verticalCenter
        visible: true
        iconButtonW: root.iconButtonW
        iconButtonH: root.iconButtonH
        iconGlyph: root.iconGlyph
        onClicked: root.pulseRequested()
    }

    Loader {
        id: indicatorManagerLoader
        anchors.right: parent.right
        anchors.rightMargin: root.panelInset
        anchors.verticalCenter: parent.verticalCenter
        width: item ? item.implicitWidth : 0
        height: parent.height
        active: root.deferredReady
        asynchronous: true
        sourceComponent: Component {
            IndicatorManager {
                anchors.fill: parent
                fileManagerApi: FileManagerApi
                panelTextYOffset: root.panelTextYOffset
                timeText: root.timeText
                popupHost: root.resolvedPopupHost
            }
        }
    }
}
