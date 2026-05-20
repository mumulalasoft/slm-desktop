import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle
import "." as TB

Item {
    id: root

    property int iconButtonW: Theme.metric("controlHeightRegular")
    property int iconButtonH: Theme.metric("controlHeightCompact")
    property int iconGlyph: 18
    property int logoVisualOffsetX: -1
    property int logoVisualOffsetY: 0
    property var popupHost: null
    property var rootWindow: null
    property int popupGap: Theme.metric("spacingSm")
    property var searchProfilesModel: []
    property string powerConfirmAction: ""
    property var runningAppsForPowerAction: []
    property string scheduledShutdownLabel: ""
    property bool shutdownAdvancedExpanded: false
    property string shutdownScheduleMode: "now"
    property date shutdownAtValue: new Date()
    property int shutdownAfterTotalMinutes: 30
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
        var mod = String(moduleId || "").trim()
        var deepLink = (mod.length > 0) ? ("settings://" + mod) : ""
        var directOpened = false
        if (typeof AppExecutionGate !== "undefined" && AppExecutionGate && AppExecutionGate.launchCommand) {
            if (typeof AppBinaryDir !== "undefined" && String(AppBinaryDir || "").length > 0) {
                var localCmd = String(AppBinaryDir) + "/slm-settings"
                if (deepLink.length > 0) {
                    localCmd += " --deep-link " + deepLink
                }
                directOpened = !!AppExecutionGate.launchCommand(localCmd, "", "main-menu-local")
            }
            if (!directOpened) {
                var cmd = "slm-settings"
                if (deepLink.length > 0) {
                    cmd += " --deep-link " + deepLink
                }
                directOpened = !!AppExecutionGate.launchCommand(cmd, "", "main-menu")
            }
        }
        if (directOpened) {
            return
        }
        var routed = false
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter) {
            if (AppCommandRouter.routeWithResult) {
                var res = AppCommandRouter.routeWithResult("app.desktopid",
                                                           {desktopId: "slm-settings.desktop"},
                                                           "main-menu")
                routed = !!(res && res.ok)
            } else if (AppCommandRouter.route) {
                routed = !!AppCommandRouter.route("app.desktopid",
                                                  {desktopId: "slm-settings.desktop"},
                                                  "main-menu")
            }
        }
        if (routed) {
            return
        }
        if (typeof AppExecutionGate !== "undefined" && AppExecutionGate) {
            if (AppExecutionGate.launchDesktopId
                    && AppExecutionGate.launchDesktopId("slm-settings.desktop", "main-menu")) {
                return
            }
        }
    }

    function _lockScreen() {
        mainMenu.close()
        if (typeof SessionStateClient !== "undefined" && SessionStateClient
                && typeof SessionStateClient.lock === "function") {
            SessionStateClient.lock()
        } else if (root.rootWindow && "lockScreenVisible" in root.rootWindow) {
            root.rootWindow.lockScreenVisible = true
        }
        if (typeof ShellStateController !== "undefined" && ShellStateController
                && typeof ShellStateController.setLockScreenActive === "function") {
            ShellStateController.setLockScreenActive(true)
        }
    }

    function _logOut() {
        mainMenu.close()
        if (typeof PowerController !== "undefined" && PowerController
                && PowerController.openPowerDialog) {
            PowerController.openPowerDialog("logout")
            return
        }
        if (root._hasPowerAction("logout")) {
            PowerBridge.logout()
        }
    }

    function _buildRecentApps() {
        var rows = []
        if (typeof AppModel === "undefined" || !AppModel || !AppModel.frequentApps) {
            return rows
        }
        var apps = AppModel.frequentApps(8) || []
        for (var i = 0; i < apps.length && rows.length < 8; ++i) {
            var a = apps[i]
            if (!a) continue
            var label = String(a.name || a.desktopId || a.desktopFile || "")
            if (label.length === 0) continue
            var iconSrc = String(a.iconSource || "")
            var iconName = String(a.iconName || "")
            if (iconSrc.length === 0 && iconName.length > 0) {
                var preferredName = root._stripSymbolicSuffix(iconName)
                iconSrc = root._themeIconSource(preferredName.length > 0 ? preferredName : iconName)
            }
            rows.push({
                label: label,
                desktopId: String(a.desktopId || ""),
                desktopFile: String(a.desktopFile || ""),
                executable: String(a.executable || ""),
                iconSource: iconSrc
            })
        }
        return rows
    }

    function _mimeIconForPath(filePath) {
        var ext = String(filePath || "").split(".").pop().toLowerCase()
        switch (ext) {
        case "pdf":  return "application-pdf"
        case "doc": case "docx": case "odt": case "rtf": return "x-office-document"
        case "xls": case "xlsx": case "ods": case "csv": return "x-office-spreadsheet"
        case "ppt": case "pptx": case "odp":             return "x-office-presentation"
        case "png": case "jpg": case "jpeg": case "gif":
        case "svg": case "webp": case "bmp": case "tiff": return "image-x-generic"
        case "mp3": case "flac": case "wav": case "ogg":
        case "m4a": case "aac": case "opus":             return "audio-x-generic"
        case "mp4": case "mkv": case "avi": case "mov":
        case "webm": case "flv": case "wmv":             return "video-x-generic"
        case "zip": case "tar": case "gz": case "bz2":
        case "xz":  case "7z":  case "rar": case "zst":  return "package-x-generic"
        case "html": case "htm":                          return "text-html"
        case "md":  case "markdown":                      return "text-x-markdown"
        case "txt":                                        return "text-x-generic"
        default:                                           return "text-x-generic"
        }
    }

    function _mimeIconForMimeType(mimeType) {
        var mime = String(mimeType || "").trim().toLowerCase()
        if (mime.length <= 0) {
            return ""
        }
        if (mime === "application/pdf") return "application-pdf"
        if (mime.indexOf("application/msword") === 0
                || mime.indexOf("application/vnd.openxmlformats-officedocument.wordprocessingml") === 0
                || mime.indexOf("application/vnd.oasis.opendocument.text") === 0
                || mime === "application/rtf") return "x-office-document"
        if (mime.indexOf("application/vnd.ms-excel") === 0
                || mime.indexOf("application/vnd.openxmlformats-officedocument.spreadsheetml") === 0
                || mime.indexOf("application/vnd.oasis.opendocument.spreadsheet") === 0
                || mime === "text/csv") return "x-office-spreadsheet"
        if (mime.indexOf("application/vnd.ms-powerpoint") === 0
                || mime.indexOf("application/vnd.openxmlformats-officedocument.presentationml") === 0
                || mime.indexOf("application/vnd.oasis.opendocument.presentation") === 0) return "x-office-presentation"
        if (mime.indexOf("image/") === 0) return "image-x-generic"
        if (mime.indexOf("audio/") === 0) return "audio-x-generic"
        if (mime.indexOf("video/") === 0) return "video-x-generic"
        if (mime.indexOf("inode/directory") === 0) return "folder"
        if (mime.indexOf("zip") >= 0 || mime.indexOf("tar") >= 0
                || mime.indexOf("gzip") >= 0 || mime.indexOf("bzip") >= 0
                || mime.indexOf("xz") >= 0 || mime.indexOf("7z") >= 0
                || mime.indexOf("rar") >= 0 || mime.indexOf("archive") >= 0) return "package-x-generic"
        if (mime.indexOf("text/") === 0) return "text-x-generic"
        return ""
    }

    function _stripSymbolicSuffix(iconNameValue) {
        var value = String(iconNameValue || "").trim()
        if (value.length > 9 && value.slice(value.length - 9) === "-symbolic") {
            return value.slice(0, value.length - 9)
        }
        return value
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
            var iconName = String(f.iconName || "").trim()
            var mimeType = String(f.mimeType || "").trim()
            var iconSource = ""
            if (iconName.length > 0) {
                var preferredIconName = root._stripSymbolicSuffix(iconName)
                iconSource = root._themeIconSource(preferredIconName.length > 0 ? preferredIconName : iconName)
            }
            if (iconSource.length === 0 && mimeType.length > 0) {
                var mimeIconName = root._mimeIconForMimeType(mimeType)
                if (mimeIconName.length > 0) {
                    iconSource = root._themeIconSource(mimeIconName)
                }
            }
            if (iconSource.length === 0) {
                iconSource = root._themeIconSource(root._mimeIconForPath(uri))
            }
            rows.push({label: name, uri: uri, iconSource: iconSource})
        }
        return rows
    }

    function _themeIconSource(iconName) {
        var name = String(iconName || "").trim()
        if (name.length === 0) {
            return ""
        }
        var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                   ? ThemeIconController.revision : 0)
        return "image://themeicon/" + name + "?v=" + rev
    }

    function _hasPowerAction(actionName) {
        return typeof PowerBridge !== "undefined"
                && PowerBridge
                && typeof PowerBridge[actionName] === "function"
    }

    function _hasLockAction() {
        return (typeof SessionStateClient !== "undefined"
                    && SessionStateClient
                    && typeof SessionStateClient.lock === "function")
                || (root.rootWindow && "lockScreenVisible" in root.rootWindow)
                || (typeof ShellStateController !== "undefined"
                    && ShellStateController
                    && typeof ShellStateController.setLockScreenActive === "function")
    }

    function _collectRunningApps() {
        var rows = []
        if (typeof AppStateClient !== "undefined" && AppStateClient && AppStateClient.listRunningApps) {
            var apps = AppStateClient.listRunningApps() || []
            for (var i = 0; i < apps.length; ++i) {
                var app = apps[i] || {}
                var appId = String(app.appId || "")
                var state = String(app.state || "")
                var windowCount = Number(app.windowCount || 0)
                if (appId.length <= 0) {
                    continue
                }
                rows.push({
                    label: appId,
                    state: state,
                    windowCount: windowCount
                })
            }
        }
        return rows
    }

    function _startPowerConfirmation(actionName) {
        var action = String(actionName || "")
        if (action !== "restart" && action !== "shutdown") {
            return
        }
        if (typeof PowerController !== "undefined" && PowerController
                && PowerController.openPowerDialog) {
            PowerController.openPowerDialog(action)
            return
        }
        shutdownAdvancedExpanded = false
        shutdownScheduleMode = "now"
        shutdownAtValue = new Date()
        shutdownAfterTotalMinutes = 30
        powerConfirmAction = action
        runningAppsForPowerAction = _collectRunningApps()
        powerConfirmDialog.open()
    }

    function _confirmPowerAction() {
        if (powerConfirmAction === "restart") {
            if (typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.reboot) {
                PowerBridge.reboot()
            }
        } else if (powerConfirmAction === "shutdown") {
            if (shutdownScheduleMode === "at") {
                var hhValue = shutdownAtValue instanceof Date ? shutdownAtValue.getHours() : 22
                var mmValue = shutdownAtValue instanceof Date ? shutdownAtValue.getMinutes() : 0
                var hh = (hhValue < 10 ? "0" : "") + String(hhValue)
                var mm = (mmValue < 10 ? "0" : "") + String(mmValue)
                _scheduleShutdownAt(hh + ":" + mm)
            } else if (shutdownScheduleMode === "after") {
                _scheduleShutdownAfter(shutdownAfterTotalMinutes)
            } else if (typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.powerOff) {
                PowerBridge.powerOff()
            }
        }
        powerConfirmDialog.close()
    }

    function _scheduleShutdownAt(timeSpec) {
        var text = String(timeSpec || "").trim()
        if (text.length <= 0) {
            return
        }
        if (typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.scheduleShutdownAt) {
            if (PowerBridge.scheduleShutdownAt(text)) {
                scheduledShutdownLabel = qsTr("Shutdown scheduled at %1").arg(text)
                scheduledShutdownToast.restart()
            }
        }
    }

    function _scheduleShutdownAfter(minutes) {
        var value = Number(minutes || 0)
        if (value <= 0) {
            return
        }
        if (typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.scheduleShutdownAfterMinutes) {
            if (PowerBridge.scheduleShutdownAfterMinutes(value)) {
                scheduledShutdownLabel = qsTr("Shutdown scheduled in %1 minutes").arg(String(value))
                scheduledShutdownToast.restart()
            }
        }
    }

    function _formatShutdownAtLabel() {
        var hhValue = shutdownAtValue instanceof Date ? shutdownAtValue.getHours() : 22
        var mmValue = shutdownAtValue instanceof Date ? shutdownAtValue.getMinutes() : 0
        var hh = (hhValue < 10 ? "0" : "") + String(hhValue)
        var mm = (mmValue < 10 ? "0" : "") + String(mmValue)
        return hh + ":" + mm
    }

    function _formatShutdownAfterLabel() {
        var total = Math.max(1, Number(shutdownAfterTotalMinutes || 0))
        var h = Math.floor(total / 60)
        var m = total % 60
        if (h > 0 && m > 0) return String(h) + "h " + String(m) + "m"
        if (h > 0) return String(h) + "h"
        return String(m) + "m"
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

        Item {
            id: logoSlot
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.logoVisualOffsetX
            anchors.verticalCenterOffset: root.logoVisualOffsetY
            width: Math.round(root.iconGlyph)
            height: Math.round(root.iconGlyph)

            Image {
                id: crownLogo
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                source: Theme.darkMode ? "qrc:/icons/dark/logo.svg" : "qrc:/icons/light/logo.svg"
                opacity: Theme.opacityIconMuted
            }
        }

        Label {
            anchors.centerIn: logoSlot
            visible: crownLogo.status !== Image.Ready
            text: "⌘"
            color: Theme.color("textOnGlass")
            font.pixelSize: Theme.fontSize("bodyLarge")
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

    TB.CrownAnchoredMenu {
        id: mainMenu
        appletId: "crown.mainmenu"
        anchorItem: mainButton
        popupHost: root.popupHost
        popupGap: root.popupGap
        alignRightToAnchor: false
        onAboutToShow: {
            root.popupHintCleared()
            recentAppsMenu._rows = root._buildRecentApps()
            recentFilesMenu._rows = root._buildRecentFiles()
        }
        onAboutToHide: {
            root.popupHintCleared()
            root.lastCloseMs = Date.now()
        }

        // ── System info & settings ────────────────────────────────────────────

        DSStyle.MenuItem {
            text: qsTr("About This Computer")
            onTriggered: root._openSettings("about")
        }

        DSStyle.MenuItem {
            text: qsTr("System Settings\u2026")
            onTriggered: root._openSettings("")
        }

        DSStyle.MenuSeparator {}

        // ── Recent Applications submenu ───────────────────────────────────────

        DSStyle.Menu {
            id: recentAppsMenu
            title: qsTr("Recent Applications")
            property string entryIconSource: "qrc:/icons/appdeck.svg"
            property string entryIconName: "application-x-executable-symbolic"
            icon.name: "application-x-executable-symbolic"
            icon.source: root._themeIconSource("application-x-executable-symbolic")
            enabled: recentAppsInstantiator.count > 0

            Instantiator {
                id: recentAppsInstantiator
                model: recentAppsMenu._rows
                delegate: DSStyle.MenuItem {
                    property var entry: modelData || ({})
                    text: String(entry.label || "")
                    enabled: text.length > 0
                    iconSource: String(entry.iconSource || "")
                    fallbackIconSource: "qrc:/icons/appdeck.svg"
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

        DSStyle.Menu {
            id: recentFilesMenu
            title: qsTr("Recent Files")
            property string entryIconSource: "qrc:/icons/logo.svg"
            property string entryIconName: "document-open-symbolic"
            icon.name: "document-open-symbolic"
            icon.source: root._themeIconSource("document-open-symbolic")
            enabled: recentFilesInstantiator.count > 0

            Instantiator {
                id: recentFilesInstantiator
                model: recentFilesMenu._rows
                delegate: DSStyle.MenuItem {
                    property var entry: modelData || ({})
                    text: String(entry.label || "")
                    enabled: text.length > 0
                    iconSource: String(entry.iconSource || "")
                    fallbackIconSource: root._themeIconSource("text-x-generic")
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

        DSStyle.MenuSeparator {}

        // ── Theme Toggle ──────────────────────────────────────────────────────

        DSStyle.MenuItem {
            text: (typeof DesktopSettings !== "undefined" && DesktopSettings.themeMode === "dark")
                  ? qsTr("Switch to Light Mode")
                  : qsTr("Switch to Dark Mode")
            onTriggered: {
                mainMenu.close()
                if (typeof DesktopSettings !== "undefined") {
                    var newMode = (DesktopSettings.themeMode === "dark") ? "light" : "dark"
                    DesktopSettings.setThemeMode(newMode)
                }
            }
        }

        DSStyle.MenuSeparator {}

        // ── Power actions ─────────────────────────────────────────────────────

        DSStyle.MenuItem {
            text: qsTr("Sleep")
            enabled: typeof PowerBridge !== "undefined" && PowerBridge && PowerBridge.canSuspend
            onTriggered: {
                mainMenu.close()
                if (typeof PowerController !== "undefined" && PowerController
                        && PowerController.openPowerDialog) {
                    PowerController.openPowerDialog("sleep")
                    return
                }
                if (typeof PowerBridge !== "undefined" && PowerBridge) PowerBridge.sleep()
            }
        }

        DSStyle.MenuItem {
            text: qsTr("Restart\u2026")
            enabled: root._hasPowerAction("reboot")
            onTriggered: {
                mainMenu.close()
                root._startPowerConfirmation("restart")
            }
        }

        DSStyle.MenuItem {
            text: qsTr("Shut Down\u2026")
            enabled: root._hasPowerAction("powerOff")
            onTriggered: {
                mainMenu.close()
                root._startPowerConfirmation("shutdown")
            }
        }

        DSStyle.MenuSeparator {}

        // ── Session actions ───────────────────────────────────────────────────

        DSStyle.MenuItem {
            text: qsTr("Lock Screen")
            enabled: root._hasLockAction()
            onTriggered: root._lockScreen()
        }

        DSStyle.MenuItem {
            text: qsTr("Log Out\u2026")
            enabled: root._hasPowerAction("logout")
            onTriggered: root._logOut()
        }
    }

    Item {
        id: powerConfirmDialog
        parent: root.rootWindow ? root.rootWindow.contentItem : root
        anchors.fill: parent
        visible: _progress > 0
        z: 9999

        property bool opened: false
        property real _progress: 0
        property int  countdown: 60
        readonly property int dialogWidth: 420

        Behavior on _progress {
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
        }

        onOpenedChanged: {
            _progress = opened ? 1.0 : 0.0
            if (opened) {
                countdown = 60
                forceActiveFocus()
            }
        }

        function open()  { opened = true }
        function close() { opened = false }

        // Auto-shutdown countdown \u2014 only fires when action is "shutdown" + mode "now"
        Timer {
            id: countdownTimer
            interval: 1000
            repeat: true
            running: powerConfirmDialog.opened
                     && root.powerConfirmAction === "shutdown"
                     && root.shutdownScheduleMode === "now"
            onTriggered: {
                powerConfirmDialog.countdown = Math.max(0, powerConfirmDialog.countdown - 1)
                if (powerConfirmDialog.countdown <= 0) root._confirmPowerAction()
            }
        }

        Keys.onEscapePressed: powerConfirmDialog.close()
        Keys.onReturnPressed: root._confirmPowerAction()
        focus: opened

        // \u2500\u2500 Backdrop \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.46 * powerConfirmDialog._progress)
            MouseArea {
                anchors.fill: parent
                onClicked: powerConfirmDialog.close()
            }
        }

        // \u2500\u2500 Dialog card \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        Rectangle {
            id: dialogCard
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -36
            width: Math.min(powerConfirmDialog.dialogWidth,
                            parent.width - Theme.metric("spacingXxl") * 2)
            height: dialogInner.implicitHeight
            radius: Theme.radiusWindowAlt
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: Theme.borderWidthThin
            clip: true

            opacity: powerConfirmDialog._progress
            scale: 0.92 + (0.08 * powerConfirmDialog._progress)
            transformOrigin: Item.Center

            ColumnLayout {
                id: dialogInner
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: 0

                // \u2500\u2500 Icon \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    Layout.topMargin: 24

                    Rectangle {
                        anchors.centerIn: parent
                        width: 52; height: 52
                        radius: width / 2
                        color: root.powerConfirmAction === "shutdown"
                               ? Qt.rgba(0.84, 0.20, 0.18, 0.12)
                               : Qt.rgba(0.91, 0.57, 0.12, 0.12)

                        Item {
                            anchors.centerIn: parent
                            width: 26; height: 26

                            Image {
                                id: powerIcon
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                source: root._themeIconSource(
                                    root.powerConfirmAction === "shutdown"
                                        ? "system-shutdown"
                                        : "system-reboot"
                                )
                            }

                            DSStyle.Label {
                                anchors.centerIn: parent
                                visible: powerIcon.status !== Image.Ready
                                text: root.powerConfirmAction === "shutdown" ? "\u23fb" : "\u21ba"
                                font.pixelSize: Theme.fontSize("subtitle")
                                color: root.powerConfirmAction === "shutdown"
                                       ? Qt.rgba(0.84, 0.20, 0.18, 1)
                                       : Qt.rgba(0.85, 0.52, 0.11, 1)
                            }
                        }
                    }
                }

                // \u2500\u2500 Title \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                DSStyle.Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    Layout.rightMargin: 28
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: root.powerConfirmAction === "restart"
                          ? qsTr("Restart Computer?")
                          : qsTr("Shut Down Computer?")
                    font.pixelSize: Theme.fontSize("subtitle")
                    font.weight: Theme.fontWeight("semibold")
                }

                // \u2500\u2500 Subtitle / countdown \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                DSStyle.Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    Layout.rightMargin: 28
                    Layout.topMargin: 6
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.color("textSecondary")
                    text: {
                        if (root.powerConfirmAction === "shutdown"
                                && root.shutdownScheduleMode === "now"
                                && powerConfirmDialog.countdown > 0) {
                            return qsTr("If you do nothing, your computer will shut down automatically in %1 seconds.")
                                       .arg(powerConfirmDialog.countdown)
                        }
                        if (root.runningAppsForPowerAction.length > 0) {
                            return qsTr("Open apps may have unsaved changes that will be lost.")
                        }
                        return root.powerConfirmAction === "restart"
                               ? qsTr("Your computer will restart immediately.")
                               : qsTr("Your computer will shut down immediately.")
                    }
                }

                // \u2500\u2500 Running apps \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.topMargin: 14
                    visible: root.runningAppsForPowerAction.length > 0
                    implicitHeight: Math.min(136,
                        runningAppsColumn.implicitHeight + Theme.metric("spacingSm") * 2)
                    radius: Theme.radiusCard
                    color: Theme.color("menuBg")
                    border.color: Theme.color("panelBorder")
                    clip: true

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: Theme.metric("spacingSm")
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                            id: runningAppsColumn
                            width: parent.width
                            spacing: 3

                            Repeater {
                                model: root.runningAppsForPowerAction
                                delegate: RowLayout {
                                    width: runningAppsColumn.width
                                    height: 28
                                    spacing: 8

                                    Rectangle {
                                        width: 5; height: 5; radius: width / 2
                                        color: Theme.color("accent")
                                        Layout.leftMargin: 2
                                    }

                                    DSStyle.Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        text: String((modelData && modelData.label) ? modelData.label : "")
                                    }

                                    DSStyle.Label {
                                        visible: Number((modelData && modelData.windowCount) || 0) > 0
                                        color: Theme.color("textSecondary")
                                        font.pixelSize: Theme.fontSize("caption")
                                        text: qsTr("%1 windows").arg(String((modelData && modelData.windowCount) || 0))
                                        Layout.rightMargin: 2
                                    }
                                }
                            }
                        }
                    }
                }

                // \u2500\u2500 Schedule (shutdown only, always visible) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.topMargin: 14
                    visible: root.powerConfirmAction === "shutdown"
                    spacing: 8

                    // Segmented control: Now / At Time / After
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Repeater {
                            model: [
                                { key: "now",   label: qsTr("Now") },
                                { key: "at",    label: qsTr("At Time") },
                                { key: "after", label: qsTr("After") }
                            ]
                            delegate: Rectangle {
                                readonly property bool active: root.shutdownScheduleMode === String(modelData.key || "")
                                Layout.fillWidth: true
                                implicitHeight: 30
                                radius: Theme.radiusControl
                                color: active ? Theme.color("accentSoft") : Theme.color("panelBg")
                                border.color: active ? Theme.color("accent") : Theme.color("panelBorder")

                                Behavior on color {
                                    ColorAnimation { duration: Theme.durationXs; easing.type: Theme.easingDefault }
                                }

                                DSStyle.Label {
                                    anchors.centerIn: parent
                                    text: String(modelData.label || "")
                                    font.weight: active ? Theme.fontWeight("semibold") : Theme.fontWeight("regular")
                                    font.pixelSize: Theme.fontSize("caption")
                                    color: active ? Theme.color("accent") : Theme.color("textSecondary")
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.shutdownScheduleMode = String(modelData.key || "now")
                                }
                            }
                        }
                    }

                    DSStyle.TimePicker {
                        Layout.fillWidth: true
                        visible: root.shutdownScheduleMode === "at"
                        use24Hour: true
                        minuteStep: 5
                        value: root.shutdownAtValue
                        onValueEdited: function(v) { root.shutdownAtValue = v }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.shutdownScheduleMode === "after"
                        spacing: Theme.metric("spacingXs")

                        DSStyle.Slider {
                            id: durationSlider
                            from: 5; to: 24 * 60; stepSize: 5
                            value: root.shutdownAfterTotalMinutes
                            Layout.fillWidth: true
                            onMoved: root.shutdownAfterTotalMinutes = Math.round(value / 5) * 5
                        }

                        DSStyle.Label {
                            color: Theme.color("textSecondary")
                            text: root._formatShutdownAfterLabel()
                        }
                    }

                    DSStyle.Label {
                        Layout.fillWidth: true
                        visible: root.shutdownScheduleMode !== "now"
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("caption")
                        horizontalAlignment: Text.AlignHCenter
                        text: root.shutdownScheduleMode === "at"
                              ? qsTr("Will shut down at %1").arg(root._formatShutdownAtLabel())
                              : qsTr("Will shut down after %1").arg(root._formatShutdownAfterLabel())
                    }
                }

                // \u2500\u2500 Spacer \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                Item { Layout.preferredHeight: 20 }

                // \u2500\u2500 Horizontal divider \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.color("panelBorder")
                    opacity: Theme.opacityMuted
                }

                // \u2500\u2500 iOS-style split button bar \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    // Cancel button
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 44

                        Rectangle {
                            anchors.fill: parent
                            color: cancelMouse.containsMouse
                                   ? Theme.color("accentSoft")
                                   : "transparent"
                            Behavior on color {
                                ColorAnimation { duration: Theme.durationXs }
                            }
                        }

                        DSStyle.Label {
                            anchors.centerIn: parent
                            text: qsTr("Cancel")
                            font.weight: Theme.fontWeight("regular")
                            color: Theme.color("textPrimary")
                        }

                        MouseArea {
                            id: cancelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: powerConfirmDialog.close()
                        }
                    }

                    // Vertical divider between buttons
                    Rectangle {
                        width: 1; height: 44
                        color: Theme.color("panelBorder")
                        opacity: Theme.opacityMuted
                    }

                    // Confirm action button
                    Item {
                        id: confirmActionItem
                        Layout.fillWidth: true
                        implicitHeight: 44

                        readonly property color actionColor: root.powerConfirmAction === "shutdown"
                                                            ? Qt.rgba(0.84, 0.20, 0.18, 1)
                                                            : Theme.color("accent")

                        Rectangle {
                            anchors.fill: parent
                            color: confirmMouse.containsMouse
                                   ? Qt.rgba(confirmActionItem.actionColor.r,
                                             confirmActionItem.actionColor.g,
                                             confirmActionItem.actionColor.b, 0.08)
                                   : "transparent"
                            Behavior on color {
                                ColorAnimation { duration: Theme.durationXs }
                            }
                        }

                        DSStyle.Label {
                            anchors.centerIn: parent
                            text: root.powerConfirmAction === "restart" ? qsTr("Restart") : qsTr("Shut Down")
                            font.weight: Theme.fontWeight("semibold")
                            color: confirmActionItem.actionColor
                        }

                        MouseArea {
                            id: confirmMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root._confirmPowerAction()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: scheduledShutdownBanner
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.bottom
        anchors.topMargin: Theme.metric("spacingSm")
        radius: Theme.radiusControl
        color: Theme.color("panelBg")
        border.color: Theme.color("panelBorder")
        opacity: scheduledShutdownToast.running ? 1 : 0
        visible: opacity > 0
        implicitWidth: toastText.implicitWidth + Theme.metric("spacingMd") * 2
        implicitHeight: toastText.implicitHeight + Theme.metric("spacingSm") * 2

        DSStyle.Label {
            id: toastText
            anchors.centerIn: parent
            text: root.scheduledShutdownLabel
            color: Theme.color("textPrimary")
        }
    }

    Timer {
        id: scheduledShutdownToast
        interval: 2200
        repeat: false
    }
}
