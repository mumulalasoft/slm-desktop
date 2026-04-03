import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle
import "." as TB

Item {
    id: root

    property int iconButtonW: Theme.metric("controlHeightRegular")
    property int iconButtonH: Theme.metric("controlHeightCompact")
    property int iconGlyph: 18
    property var popupHost: null
    property int popupGap: Theme.metric("spacingSm")
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

    TB.TopBarAnchoredMenu {
        id: mainMenu
        appletId: "topbar.mainmenu"
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
            property string entryIconSource: "qrc:/icons/launchpad.svg"
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
                    fallbackIconSource: "qrc:/icons/launchpad.svg"
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
