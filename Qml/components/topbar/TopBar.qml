import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle
import "." as TB

Rectangle {
    id: root
    property var fileManagerContent: null
    property bool deferredReady: true
    readonly property bool startupTraceEnabled: (typeof StartupTraceEnabled !== "undefined") ? !!StartupTraceEnabled : false
    readonly property var popupHost: (parent && parent.height > 0) ? parent : root
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
    signal tothespotRequested()
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
    readonly property bool startupItemsReady: !!deferredReady
                                             && !!indicatorsRegistered
                                             && mainMenuControlLoader.status === Loader.Ready
                                             && screenshotControlLoader.status === Loader.Ready
                                             && indicatorManagerLoader.status === Loader.Ready
    readonly property bool anyPopupOpen: popupOpenHint
                                        || mainMenuPopupOpen
                                        || screenshotPopupOpen
                                        || indicatorPopupOpen

    Timer {
        id: popupHintTimer
        interval: root.popupHintMs
        repeat: false
        onTriggered: root.popupOpenHint = false
    }

    color: Theme.color("panelBg")
    border.color: Theme.color("panelBorder")
    border.width: Theme.borderWidthThin


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
        if (typeof TothespotService !== "undefined" && TothespotService && TothespotService.searchProfiles) {
            var rows = TothespotService.searchProfiles()
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
        if (typeof TothespotService !== "undefined" && TothespotService && TothespotService.activeSearchProfile) {
            active = normalizeProfileId(TothespotService.activeSearchProfile())
        } else if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.settingValue) {
            active = normalizeProfileId(DesktopSettings.settingValue("tothespot.searchProfile", "balanced"))
        }
        activeSearchProfileId = active
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
            name: "controlcenter",
            source: "../applet/ControlCenterApplet.qml",
            order: 890,
            enabled: false,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "network",
            source: "../applet/NetworkApplet.qml",
            order: 200,
            enabled: false,
            properties: { showText: true, popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "bluetooth",
            source: "../applet/BluetoothApplet.qml",
            order: 300,
            enabled: false,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "sound",
            source: "../applet/SoundApplet.qml",
            order: 400,
            enabled: false,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "print",
            source: "../applet/PrintJobApplet.qml",
            order: 450,
            enabled: false,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "notification",
            source: "../applet/NotificationApplet.qml",
            order: 500,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "clipboard",
            source: "../applet/ClipboardApplet.qml",
            order: 600,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "battery",
            source: "../applet/BatteryApplet.qml",
            order: 700,
            properties: { popupHost: root.popupHost, textYOffset: root.panelTextYOffset }
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
            properties: { popupHost: root.popupHost, timeText: root.timeText }
        })
        stagedIndicatorQueue = ["network", "bluetooth", "sound", "print", "controlcenter"]
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

    Timer {
        id: clockTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.timeText = Qt.formatDateTime(new Date(), "ddd MMM d  hh:mm")
    }

    Component.onCompleted: {
        startupQmlMark("topbar.onCompleted.begin")
        root.timeText = Qt.formatDateTime(new Date(), "ddd MMM d  hh:mm")
        Qt.callLater(function() {
            startupQmlMark("topbar.deferredInit.begin")
            refreshSearchProfilesModel()
            registerCoreIndicators()
            startupQmlMark("topbar.deferredInit.end")
        })
        startupQmlMark("topbar.onCompleted.end")
    }

    onDeferredReadyChanged: {
        if (!deferredReady) {
            return
        }
        startupQmlMark("topbar.deferredReady")
        Qt.callLater(function() {
            registerCoreIndicators()
        })
    }

    onStartupItemsReadyChanged: {
        if (startupItemsReady) {
            startupQmlMark("topbar.startupItems.ready")
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
                root.startupQmlMark("topbar.stagedIndicators.done")
                return
            }
            var next = stagedIndicatorQueue[0]
            stagedIndicatorQueue = stagedIndicatorQueue.slice(1)
            if (typeof IndicatorRegistry !== "undefined" && IndicatorRegistry
                    && IndicatorRegistry.setIndicatorEnabledByName) {
                IndicatorRegistry.setIndicatorEnabledByName(next, true)
                root.startupQmlMark("topbar.stagedIndicator.enabled", "name=" + String(next))
            }
            if (stagedIndicatorQueue.length <= 0) {
                stop()
                root.startupQmlMark("topbar.stagedIndicators.done")
            }
        }
    }

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onSettingChanged(path) {
            var k = String(path || "")
            if (k === "tothespot/searchProfile" || k === "tothespot.searchProfile") {
                root.activeSearchProfileId = root.normalizeProfileId(
                            DesktopSettings.settingValue("tothespot.searchProfile", "balanced"))
            } else if (k === "ui/fontScale" || k === "ui.fontScale") {
                Theme.userFontScale = root.normalizedFontScale(
                            DesktopSettings.settingValue("ui.fontScale", 1.0))
            }
        }
    }

    Connections {
        target: (typeof TothespotService !== "undefined") ? TothespotService : null
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
        anchors.left: parent.left
        anchors.leftMargin: root.panelInset
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.panelGap

        Loader {
            id: mainMenuControlLoader
            active: root.deferredReady
            asynchronous: true
            sourceComponent: Component {
                TB.TopBarMainMenuControl {
                    iconButtonW: root.iconButtonW
                    iconButtonH: root.iconButtonH
                    iconGlyph: root.iconGlyph
                    popupHost: root.popupHost
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
                        if (typeof TothespotService !== "undefined" && TothespotService
                                && TothespotService.setActiveSearchProfile) {
                            handled = !!TothespotService.setActiveSearchProfile(profileId)
                        }
                        if (!handled && typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setSettingValue) {
                            DesktopSettings.setSettingValue("tothespot.searchProfile", profileId)
                        }
                    }
                    onSearchProfileResetRequested: {
                        root.activeSearchProfileId = "balanced"
                        var handled = false
                        if (typeof TothespotService !== "undefined" && TothespotService
                                && TothespotService.setActiveSearchProfile) {
                            handled = !!TothespotService.setActiveSearchProfile("balanced")
                        }
                        if (!handled && typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setSettingValue) {
                            DesktopSettings.setSettingValue("tothespot.searchProfile", "balanced")
                        }
                        root.refreshSearchProfilesModel()
                    }
                    onRefreshSearchProfilesRequested: root.refreshSearchProfilesModel()
                }
            }
        }
        TB.TopBarBrandSection {
            fileManagerContent: root.fileManagerContent
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
            TB.TopBarScreenshotControl {
                iconButtonW: root.iconButtonW
                iconButtonH: root.iconButtonH
                iconGlyph: root.iconGlyph
                popupInset: root.popupInset
                popupGap: root.popupGap
                popupControlH: root.popupControlH
                popupHost: root.popupHost
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

    TB.TopBarSearchButton {
        id: searchButton
        anchors.right: indicatorManagerLoader.left
        anchors.rightMargin: Theme.metric("spacingSm")
        anchors.verticalCenter: parent.verticalCenter
        iconButtonW: root.iconButtonW
        iconButtonH: root.iconButtonH
        iconGlyph: root.iconGlyph
        onClicked: root.tothespotRequested()
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
                popupHost: root.popupHost
            }
        }
    }
}
