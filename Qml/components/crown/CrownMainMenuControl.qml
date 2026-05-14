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

        // ── Force Quit ────────────────────────────────────────────────────────

        DSStyle.MenuItem {
            text: qsTr("Force Quit…")
            onTriggered: {
                mainMenu.close()
                if (typeof AppCommandRouter !== "undefined" && AppCommandRouter
                        && AppCommandRouter.route) {
                    AppCommandRouter.route("app.forcequit", {}, "main-menu")
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
        visible: opened
        z: 9999
        property bool opened: false
        property int dialogWidth: 548

        function open() { opened = true }
        function close() { opened = false }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.34)
            MouseArea {
                anchors.fill: parent
                onClicked: powerConfirmDialog.close()
            }
        }

        Rectangle {
            id: dialogCard
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(powerConfirmDialog.dialogWidth, parent.width - Theme.metric("spacingXxl") * 2)
            height: Math.min(parent.height - Theme.metric("spacingXxl") * 2,
                             bodyColumn.implicitHeight
                             + footerContainer.height
                             + Theme.metric("spacingXxl") * 2
                             + Theme.metric("spacingLg"))
            radius: Theme.radiusWindowAlt
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: Theme.borderWidthThin
            clip: true

            ColumnLayout {
                id: bodyColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: footerDivider.top
                anchors.margins: Theme.metric("spacingXxl")
                anchors.bottomMargin: Theme.metric("spacingLg")
                spacing: Theme.metric("spacingLg")

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("panelBg")
                    border.color: Theme.color("panelBorder")
                    implicitHeight: heroRow.implicitHeight + Theme.metric("spacingMd") * 2

                    RowLayout {
                        id: heroRow
                        anchors.fill: parent
                        anchors.margins: Theme.metric("spacingLg")
                        spacing: Theme.metric("spacingMd")

                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: Theme.radiusXxl
                            color: root.powerConfirmAction === "shutdown" ? Qt.rgba(0.88, 0.24, 0.21, 0.18)
                                                                          : Qt.rgba(0.91, 0.57, 0.12, 0.18)
                            DSStyle.Label {
                                anchors.centerIn: parent
                                text: "!"
                                font.pixelSize: Theme.fontSize("subtitle")
                                font.weight: Theme.fontWeight("bold")
                                color: root.powerConfirmAction === "shutdown" ? Qt.rgba(0.86, 0.2, 0.18, 1)
                                                                              : Qt.rgba(0.85, 0.5, 0.1, 1)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: root.powerConfirmAction === "restart"
                                      ? qsTr("Restart Computer?")
                                      : qsTr("Shut Down Computer?")
                                font.pixelSize: Theme.fontSize("subtitle")
                                font.weight: Theme.fontWeight("semibold")
                            }
                            DSStyle.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: Theme.color("textSecondary")
                                text: root.runningAppsForPowerAction.length > 0
                                      ? qsTr("Aplikasi yang masih terbuka dapat kehilangan data yang belum disimpan.")
                                      : qsTr("No active app was detected.")
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(176, runningAppsColumn.implicitHeight + (Theme.metric("spacingSm") * 2))
                    radius: Theme.radiusCard
                    color: Theme.color("menuBg")
                    border.color: Theme.color("panelBorder")
                    visible: root.runningAppsForPowerAction.length > 0
                    clip: true

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: Theme.metric("spacingSm")
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                            id: runningAppsColumn
                            width: parent.width
                            spacing: Theme.metric("spacingXs")

                            Repeater {
                                model: root.runningAppsForPowerAction
                                delegate: Rectangle {
                                    width: runningAppsColumn.width
                                    implicitHeight: appRow.implicitHeight + Theme.metric("spacingSm")
                                    radius: Theme.radiusControl
                                    color: Theme.color("panelBg")
                                    border.color: Theme.color("panelBorder")

                                    RowLayout {
                                        id: appRow
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.metric("spacingSm")
                                        anchors.rightMargin: Theme.metric("spacingSm")
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: Theme.metric("spacingSm")

                                        Rectangle {
                                            Layout.preferredWidth: 6
                                            Layout.preferredHeight: 6
                                            radius: Theme.radiusXs
                                            color: Theme.color("accent")
                                        }

                                        DSStyle.Label {
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            text: String((modelData && modelData.label) ? modelData.label : "")
                                        }

                                        DSStyle.Label {
                                            visible: Number((modelData && modelData.windowCount) || 0) > 0
                                            color: Theme.color("textSecondary")
                                            text: qsTr("%1 windows").arg(String((modelData && modelData.windowCount) || 0))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.powerConfirmAction === "shutdown" ? implicitHeight : 0
                    visible: root.powerConfirmAction === "shutdown"
                    implicitHeight: advancedColumn.implicitHeight

                    ColumnLayout {
                        id: advancedColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: Theme.metric("spacingSm")

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.metric("spacingXs")

                            DSStyle.Label {
                                text: qsTr("Schedule shutdown")
                                color: Theme.color("textSecondary")
                                font.weight: Theme.fontWeight("medium")
                            }

                            Item { Layout.fillWidth: true }

                            DSStyle.Button {
                                implicitWidth: 42
                                implicitHeight: 30
                                text: root.shutdownAdvancedExpanded ? "\u2715" : "\u22ef"
                                font.pixelSize: Theme.fontSize("body")
                                onClicked: root.shutdownAdvancedExpanded = !root.shutdownAdvancedExpanded
                            }
                        }

                        Rectangle {
                            id: schedulePanel
                            Layout.fillWidth: true
                            radius: Theme.radiusCard
                            color: Theme.color("menuBg")
                            border.color: Theme.color("panelBorder")
                            visible: true
                            opacity: root.shutdownAdvancedExpanded ? 1 : 0
                            implicitHeight: root.shutdownAdvancedExpanded
                                            ? (schedulerBody.implicitHeight + Theme.metric("spacingSm") * 2)
                                            : 0
                            clip: true

                            Behavior on implicitHeight {
                                NumberAnimation {
                                    duration: Theme.durationSm
                                    easing.type: Theme.easingEmphasized
                                }
                            }
                            Behavior on opacity {
                                NumberAnimation {
                                    duration: Theme.durationSm
                                    easing.type: Theme.easingDefault
                                }
                            }

                            ColumnLayout {
                                id: schedulerBody
                                anchors.fill: parent
                                anchors.margins: Theme.metric("spacingSm")
                                spacing: Theme.metric("spacingXs")

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.metric("spacingXs")

                                    Repeater {
                                        model: [
                                            { key: "now", label: qsTr("Now") },
                                            { key: "at", label: qsTr("At Time") },
                                            { key: "after", label: qsTr("After") }
                                        ]
                                        delegate: Rectangle {
                                            readonly property bool active: root.shutdownScheduleMode === String(modelData.key || "")
                                            Layout.fillWidth: true
                                            implicitHeight: 32
                                            radius: Theme.radiusControl
                                            color: active ? Theme.color("accentSoft") : Theme.color("panelBg")
                                            border.color: active ? Theme.color("accent") : Theme.color("panelBorder")

                                            DSStyle.Label {
                                                anchors.centerIn: parent
                                                text: String(modelData.label || "")
                                                font.weight: active ? Theme.fontWeight("semibold") : Theme.fontWeight("medium")
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
                                    Layout.leftMargin: Theme.metric("spacingMd")
                                    visible: root.shutdownScheduleMode === "at"
                                    use24Hour: true
                                    minuteStep: 5
                                    value: root.shutdownAtValue
                                    onValueEdited: function(v) { root.shutdownAtValue = v }
                                }

                                RowLayout {
                                    Layout.leftMargin: Theme.metric("spacingMd")
                                    visible: root.shutdownScheduleMode === "after"
                                    spacing: Theme.metric("spacingXs")
                                    Layout.fillWidth: true

                                    DSStyle.Slider {
                                        id: durationSlider
                                        from: 5
                                        to: 24 * 60
                                        stepSize: 5
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
                                    Layout.topMargin: Theme.metric("spacingXxs")
                                    color: Theme.color("textSecondary")
                                    text: root.shutdownScheduleMode === "at"
                                          ? qsTr("Will shut down at %1").arg(root._formatShutdownAtLabel())
                                          : (root.shutdownScheduleMode === "after"
                                             ? qsTr("Will shut down after %1").arg(root._formatShutdownAfterLabel())
                                             : qsTr("Will shut down immediately after confirmation"))
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: footerDivider
            parent: dialogCard
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footerContainer.top
            anchors.leftMargin: Theme.metric("spacingXxl")
            anchors.rightMargin: Theme.metric("spacingXxl")
            height: 1
            color: Theme.color("panelBorder")
            opacity: Theme.opacityMuted
        }

        Item {
            id: footerContainer
            parent: dialogCard
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.metric("spacingXxl")
            anchors.rightMargin: Theme.metric("spacingXxl")
            anchors.bottomMargin: Theme.metric("spacingXxl")
            height: 36

            RowLayout {
                anchors.fill: parent
                spacing: Theme.metric("spacingSm")

                Item { Layout.fillWidth: true }

                DSStyle.Button {
                    text: qsTr("Cancel")
                    implicitWidth: 118
                    implicitHeight: 36
                    onClicked: powerConfirmDialog.close()
                }

                DSStyle.Button {
                    id: confirmActionButton
                    text: root.powerConfirmAction === "restart" ? qsTr("Restart") : qsTr("Shut Down")
                    defaultAction: true
                    hoverEnabled: true
                    implicitWidth: 154
                    implicitHeight: 36
                    scale: !enabled ? 1.0 : (down ? 0.975 : 1.0)
                    transformOrigin: Item.Center
                    Behavior on scale {
                        NumberAnimation {
                            duration: Theme.durationXs
                            easing.type: Theme.easingDefault
                        }
                    }
                    background: Rectangle {
                        readonly property bool destructive: root.powerConfirmAction === "shutdown"
                        readonly property color baseColor: destructive ? Qt.rgba(0.84, 0.2, 0.18, 1.0)
                                                                       : Theme.color("accent")
                        readonly property color hoverColor: destructive ? Qt.rgba(0.78, 0.17, 0.16, 1.0)
                                                                        : Theme.color("accentHover")
                        readonly property color pressColor: destructive ? Qt.rgba(0.70, 0.14, 0.14, 1.0)
                                                                        : Theme.color("accentActive")
                        radius: Theme.radiusControl
                        color: !confirmActionButton.enabled ? Qt.rgba(baseColor.r, baseColor.g, baseColor.b, 0.45)
                              : (confirmActionButton.down ? pressColor
                                 : (confirmActionButton.hovered ? hoverColor : baseColor))
                        border.color: destructive ? Qt.rgba(0.55, 0.1, 0.1, 1.0) : Theme.color("accent")
                        border.width: Theme.borderWidthThin

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.durationXs
                                easing.type: Theme.easingDefault
                            }
                        }
                    }
                    contentItem: DSStyle.Label {
                        text: parent.text
                        color: Theme.color("textOnAccent")
                        font.pixelSize: Theme.fontSize("body")
                        font.weight: Theme.fontWeight("semibold")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root._confirmPowerAction()
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
