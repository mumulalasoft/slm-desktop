import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle
import "." as TB

Rectangle {
    id: root
    property var fileManagerContent: null
    readonly property var popupHost: (root.Window.window && root.Window.window.overlay)
                                     ? root.Window.window.overlay : null
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
    signal styleGalleryRequested()
    signal tothespotRequested()
    signal screenshotCaptureRequested(string mode, int delaySec, bool grabPointer, bool concealText)
    property string timeText: ""
    property int panelTextYOffset: 0
    property bool popupOpenHint: false
    property double mainMenuLastCloseMs: 0
    property var searchProfilesModel: []
    property string activeSearchProfileId: "balanced"
    readonly property bool anyPopupOpen: popupOpenHint
                                        || mainMenuControl.popupOpen
                                        || screenshotControl.popupOpen
                                        || (indicatorManager && indicatorManager.itemPopupOpen)

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
        } else if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference) {
            active = normalizeProfileId(UIPreferences.getPreference("tothespot.searchProfile", "balanced"))
        }
        activeSearchProfileId = active
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
        if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
            UIPreferences.setPreference("ui.fontScale", normalized)
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
        root.timeText = Qt.formatDateTime(new Date(), "ddd MMM d  hh:mm")
        refreshSearchProfilesModel()

        // Register core indicators
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
            name: "network",
            source: "../applet/NetworkApplet.qml",
            order: 200,
            properties: { showText: true, popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "bluetooth",
            source: "../applet/BluetoothApplet.qml",
            order: 300,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "sound",
            source: "../applet/SoundApplet.qml",
            order: 400,
            properties: { popupHost: root.popupHost }
        })
        IndicatorRegistry.registerIndicator({
            name: "print",
            source: "../applet/PrintJobApplet.qml",
            order: 450,
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
            properties: { popupHost: root.popupHost, timeText: root.timeText, manager: indicatorManager }
        })
    }

    Connections {
        target: (typeof UIPreferences !== "undefined") ? UIPreferences : null
        function onPreferenceChanged(key, value) {
            var k = String(key || "")
            if (k === "tothespot/searchProfile" || k === "tothespot.searchProfile") {
                root.activeSearchProfileId = root.normalizeProfileId(value)
            } else if (k === "ui/fontScale" || k === "ui.fontScale") {
                Theme.userFontScale = root.normalizedFontScale(value)
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
                if (!mainMenuControl.popupOpen) {
                    mainMenuControl.openPopup()
                }
            }
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: root.panelInset
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.panelGap

        TB.TopBarMainMenuControl {
            id: mainMenuControl
            iconButtonW: root.iconButtonW
            iconButtonH: root.iconButtonH
            iconGlyph: root.iconGlyph
            searchProfilesModel: root.searchProfilesModel
            activeSearchProfileId: root.activeSearchProfileId
            onPopupHintRequested: {
                root.popupOpenHint = true
                popupHintTimer.restart()
            }
            onPopupHintCleared: {
                root.popupOpenHint = false
                root.mainMenuLastCloseMs = Date.now()
            }
            onStyleGalleryRequested: root.styleGalleryRequested()
            onSetFontScaleRequested: function(scaleValue) {
                root.setFontScale(scaleValue)
            }
            onSearchProfileSetRequested: function(profileId) {
                root.activeSearchProfileId = profileId
                var handled = false
                if (typeof TothespotService !== "undefined" && TothespotService
                        && TothespotService.setActiveSearchProfile) {
                    handled = !!TothespotService.setActiveSearchProfile(profileId)
                }
                if (!handled && typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                    UIPreferences.setPreference("tothespot.searchProfile", profileId)
                }
            }
            onSearchProfileResetRequested: {
                root.activeSearchProfileId = "balanced"
                var handled = false
                if (typeof TothespotService !== "undefined" && TothespotService
                        && TothespotService.setActiveSearchProfile) {
                    handled = !!TothespotService.setActiveSearchProfile("balanced")
                }
                if (!handled && typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.resetPreference) {
                    UIPreferences.resetPreference("tothespot.searchProfile")
                } else if (!handled && typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                    UIPreferences.setPreference("tothespot.searchProfile", "balanced")
                }
                root.refreshSearchProfilesModel()
            }
            onRefreshSearchProfilesRequested: root.refreshSearchProfilesModel()
        }
        TB.TopBarBrandSection {
            fileManagerContent: root.fileManagerContent
        }

    }

    TB.TopBarScreenshotControl {
        id: screenshotControl
        anchors.right: searchButton.left
        anchors.rightMargin: Theme.metric("spacingSm")
        anchors.verticalCenter: parent.verticalCenter
        iconButtonW: root.iconButtonW
        iconButtonH: root.iconButtonH
        iconGlyph: root.iconGlyph
        popupInset: root.popupInset
        popupGap: root.popupGap
        popupControlH: root.popupControlH
        onPopupHintRequested: {
            root.popupOpenHint = true
            popupHintTimer.restart()
        }
        onPopupHintCleared: root.popupOpenHint = false
        onScreenshotCaptureRequested: function(mode, delaySec, grabPointer, concealText) {
            root.screenshotCaptureRequested(mode, delaySec, grabPointer, concealText)
        }
    }

    TB.TopBarSearchButton {
        id: searchButton
        anchors.right: indicatorManager.left
        anchors.rightMargin: Theme.metric("spacingSm")
        anchors.verticalCenter: parent.verticalCenter
        iconButtonW: root.iconButtonW
        iconButtonH: root.iconButtonH
        iconGlyph: root.iconGlyph
        onClicked: root.tothespotRequested()
    }

    IndicatorManager {
        id: indicatorManager
        anchors.right: parent.right
        anchors.rightMargin: root.panelInset
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height
        fileManagerApi: FileManagerApi
        panelTextYOffset: root.panelTextYOffset
        timeText: root.timeText
        popupHost: root.popupHost
    }
}
