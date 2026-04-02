import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root

    property int iconButtonW: Theme.metric("controlHeightRegular")
    property int iconButtonH: Theme.metric("controlHeightCompact")
    property int iconGlyph: 18
    property var searchProfilesModel: []
    readonly property bool popupOpen: mainMenu.opened

    signal popupHintRequested()
    signal popupHintCleared()
    signal searchProfileSetRequested(string profileId)
    signal searchProfileResetRequested()
    signal refreshSearchProfilesRequested()

    implicitWidth: iconButtonW
    implicitHeight: iconButtonH

    property double lastCloseMs: 0

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    function normalizeProfileId(idValue) {
        var id = String(idValue || "").toLowerCase()
        if (id === "apps-first" || id === "files-first" || id === "balanced") {
            return id
        }
        return "balanced"
    }

    function recentlyClosed(debounceMs) {
        var d = debounceMs === undefined ? 220 : debounceMs
        return (Date.now() - Number(lastCloseMs || 0)) < d
    }

    function openPopup() {
        if (recentlyClosed(180)) {
            return
        }
        root.refreshSearchProfilesRequested()
        root.popupHintRequested()
        Qt.callLater(function() {
            mainMenu.open()
        })
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    function _openSettings(moduleId) {
        mainMenu.close()
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter
                && AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopid",
                                   {desktopId: "slm-settings.desktop",
                                    moduleHint: String(moduleId || "")},
                                   "main-menu")
        } else if (typeof AppExecutionGate !== "undefined" && AppExecutionGate
                   && AppExecutionGate.launchDesktopId) {
            AppExecutionGate.launchDesktopId("slm-settings.desktop", "main-menu")
        }
    }

    function _lockScreen() {
        mainMenu.close()
        if (typeof ShellStateController !== "undefined" && ShellStateController)
            ShellStateController.setLockScreenActive(true)
    }

    function _buildRecentApps() {
        var rows = []
        if (typeof AppCommandRouter === "undefined" || !AppCommandRouter
                || !AppCommandRouter.recentEvents) {
            return rows
        }
        var events = AppCommandRouter.recentEvents() || []
        var seen = {}
        for (var i = events.length - 1; i >= 0 && rows.length < 8; --i) {
            var e = events[i]
            if (!e || e.ok === false) continue
            var label = String(e.name || e.desktopId || e.desktopFile || "")
            if (label.length === 0 || seen[label]) continue
            seen[label] = true
            rows.push({
                label: label,
                desktopId: String(e.desktopId || ""),
                desktopFile: String(e.desktopFile || ""),
                executable: String(e.executable || ""),
                iconName: String(e.iconName || ""),
                iconSource: String(e.iconSource || "")
            })
        }
        return rows
    }

    function _buildRecentFiles() {
        var rows = []
        if (typeof FileManagerApi === "undefined" || !FileManagerApi
                || !FileManagerApi.recentFiles) {
            return rows
        }
        var files = FileManagerApi.recentFiles(8) || []
        for (var i = 0; i < files.length && rows.length < 8; ++i) {
            var f = files[i]
            if (!f) continue
            var uri = String(f.uri || f.url || f.path || "")
            var name = String(f.name || f.displayName || uri.split("/").pop() || "")
            if (name.length === 0) continue
            rows.push({label: name, uri: uri})
        }
        return rows
    }

    // ── Button ────────────────────────────────────────────────────────────────

    Rectangle {
        id: mainButton
        anchors.fill: parent
        radius: Theme.radiusControl
        color: mainMouse.containsMouse ? Theme.color("accentSoft") : "transparent"
        Behavior on color {
            enabled: root.microAnimationAllowed()
            ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }

        Image {
            id: mainLogo
            anchors.centerIn: parent
            width: root.iconGlyph
            height: root.iconGlyph
            fillMode: Image.PreserveAspectFit
            source: Theme.darkMode ? "qrc:/icons/logo_white.svg" : "qrc:/icons/logo.svg"
        }

        Label {
            anchors.centerIn: parent
            visible: mainLogo.status !== Image.Ready
            text: "\u25cf"
            color: Theme.color("textOnGlass")
            font.pixelSize: Theme.fontSize("body")
        }

        MouseArea {
            id: mainMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.recentlyClosed(220)) {
                    return
                }
                if (mainMenu.opened || mainMenu.visible) {
                    root.lastCloseMs = Date.now()
                    mainMenu.close()
                } else {
                    root.openPopup()
                }
            }
        }
    }

    // ── Menu ──────────────────────────────────────────────────────────────────

    Menu {
        id: mainMenu
        modal: false
        focus: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.round(mainButton.mapToGlobal(0, 0).x)
        y: Math.round(mainButton.mapToGlobal(0, 0).y + mainButton.height + Theme.metric("spacingSm"))
        onAboutToShow: {
            root.popupHintCleared()
            recentAppsMenu._rows = root._buildRecentApps()
            recentFilesMenu._rows = root._buildRecentFiles()
        }
        onAboutToHide: {
            root.popupHintCleared()
            root.lastCloseMs = Date.now()
        }
        background: DSStyle.PopupSurface {
            implicitWidth: Theme.metric("popupWidthS")
            implicitHeight: 40
            popupOpacity: Theme.popupSurfaceOpacityStrong
        }

        // ── System info & settings ────────────────────────────────────────────

        MenuItem {
            text: qsTr("About This Computer")
            onTriggered: root._openSettings("about")
        }

        MenuItem {
            text: qsTr("System Settings\u2026")
            onTriggered: root._openSettings("")
        }

        MenuSeparator {}

        // ── Recent Applications submenu ───────────────────────────────────────

        Menu {
            id: recentAppsMenu
            title: qsTr("Recent Applications")
            enabled: recentAppsInstantiator.count > 0

            Instantiator {
                id: recentAppsInstantiator
                model: recentAppsMenu._rows
                delegate: MenuItem {
                    property var entry: modelData || ({})
                    text: String(entry.label || "")
                    enabled: text.length > 0
                    onTriggered: {
                        mainMenu.close()
                        if (typeof AppCommandRouter === "undefined" || !AppCommandRouter) return
                        if (String(entry.desktopId || "").length > 0) {
                            AppCommandRouter.route("app.desktopid",
                                                   {desktopId: entry.desktopId}, "main-menu")
                        } else if (String(entry.desktopFile || "").length > 0) {
                            AppCommandRouter.route("app.desktopentry",
                                                   {desktopFile: entry.desktopFile,
                                                    executable: entry.executable || "",
                                                    name: entry.label || "",
                                                    iconName: entry.iconName || "",
                                                    iconSource: entry.iconSource || ""},
                                                   "main-menu")
                        }
                    }
                }
                onObjectAdded: function(index, object) { recentAppsMenu.insertItem(index, object) }
                onObjectRemoved: function(index, object) { recentAppsMenu.removeItem(object) }
            }

            // Populated in mainMenu.onAboutToShow
            property var _rows: []
        }

        // ── Recent Files submenu ──────────────────────────────────────────────

        Menu {
            id: recentFilesMenu
            title: qsTr("Recent Files")
            enabled: recentFilesInstantiator.count > 0

            Instantiator {
                id: recentFilesInstantiator
                model: recentFilesMenu._rows
                delegate: MenuItem {
                    property var entry: modelData || ({})
                    text: String(entry.label || "")
                    enabled: text.length > 0
                    onTriggered: {
                        mainMenu.close()
                        var uri = String(entry.uri || "")
                        if (uri.length === 0) return
                        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter
                                && AppCommandRouter.route) {
                            AppCommandRouter.route("filemanager.open", {target: uri}, "main-menu")
                        }
                    }
                }
                onObjectAdded: function(index, object) { recentFilesMenu.insertItem(index, object) }
                onObjectRemoved: function(index, object) { recentFilesMenu.removeItem(object) }
            }

            // Populated in mainMenu.onAboutToShow
            property var _rows: []
        }

        MenuSeparator {}

        // ── Power actions ─────────────────────────────────────────────────────

        MenuItem {
            text: qsTr("Sleep")
            enabled: typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.canSuspend
            onTriggered: {
                mainMenu.close()
                if (typeof PowerBridge !== "undefined" && PowerBridge) PowerBridge.sleep()
            }
        }

        MenuItem {
            text: qsTr("Restart\u2026")
            enabled: typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.canReboot
            onTriggered: {
                mainMenu.close()
                if (typeof PowerBridge !== "undefined" && PowerBridge) PowerBridge.reboot()
            }
        }

        MenuItem {
            text: qsTr("Shut Down\u2026")
            enabled: typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.canPowerOff
            onTriggered: {
                mainMenu.close()
                if (typeof PowerBridge !== "undefined" && PowerBridge) PowerBridge.shutdown()
            }
        }

        MenuSeparator {}

        // ── Session actions ───────────────────────────────────────────────────

        MenuItem {
            text: qsTr("Log Out\u2026")
            onTriggered: {
                mainMenu.close()
                if (typeof PowerBridge !== "undefined" && PowerBridge) PowerBridge.logout()
            }
        }

        MenuItem {
            text: qsTr("Lock Screen")
            onTriggered: root._lockScreen()
        }
    }
}
