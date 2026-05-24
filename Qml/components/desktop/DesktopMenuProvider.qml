import QtQuick 2.15

// DesktopMenuProvider — SLM-native global menu for the desktop view.
//
// Menu structure (4 menus, IDs 3101–3104):
//   3101  Desktop   — new folder/file, clean up, sort, toggle view options, wallpaper
//   3102  Edit      — undo/redo, clipboard ops, select, rename
//   3103  Storage   — home, downloads, connect server, mounted devices (dynamic), trash
//   3104  Workspace — focus, split, terminal, file manager, pin, show desktop
//
// Contract (do NOT change these signatures — ShellEventBridge + GlobalMenuManager depend on them):
//   topLevelMenus()           → [{id, label, enabled, source}]
//   ownsMenu(menuId)          → bool
//   menuItemsFor(menuId)      → [{id, label, enabled, separator?, checked?, iconName?, shortcutText?}]
//   activateMenuItem(menuId, itemId) → void

QtObject {
    id: root

    // ── identity ──────────────────────────────────────────────────────────────
    property string appId: "slm-filemanager"
    property string role: "desktop_view"
    property string path: "~/Desktop"
    property var selection: []
    property var menuContext: ({
        "selectionCount": 0,
        "selectionTypes": []
    })
    property var desktopSurface: null
    property bool enabled: true
    property string overrideContext: "desktop-view"

    // ── internal helpers ──────────────────────────────────────────────────────

    function _route(action, payload) {
        if (typeof AppCommandRouter === "undefined" || !AppCommandRouter || !AppCommandRouter.route) {
            return
        }
        AppCommandRouter.route(String(action || ""),
                               payload ? payload : ({}),
                               "desktop-menu-provider")
    }

    function _notifyInfo(message) {
        var body = String(message || "").trim()
        if (body.length <= 0) {
            return
        }
        if (typeof NotificationManager !== "undefined"
                && NotificationManager
                && NotificationManager.Notify) {
            NotificationManager.Notify("Desktop", 0,
                                       "dialog-information-symbolic",
                                       "Desktop View", body, [], {}, 2600)
        } else {
            console.log("[desktop-menu-provider]", body)
        }
    }

    // ── selection helpers ─────────────────────────────────────────────────────

    function selectionCount() {
        if (menuContext && menuContext.selectionCount !== undefined) {
            return Number(menuContext.selectionCount || 0)
        }
        return selection ? Number(selection.length || 0) : 0
    }

    function hasSingleSelection() {
        return selectionCount() === 1
    }

    function hasMultiSelection() {
        return selectionCount() > 1
    }

    function hasSelection() {
        return selectionCount() > 0
    }

    function _hasClipboard() {
        return !!(desktopSurface
                  && desktopSurface.clipboardPaths
                  && desktopSurface.clipboardPaths.length > 0)
    }

    // ── storage helpers ───────────────────────────────────────────────────────

    // Returns the raw rows from fileManagerApiRef.storageLocations() filtered to
    // mounted removable volumes only (excludes home, system mounts).
    function _mountedDeviceRows() {
        if (!desktopSurface) {
            return []
        }
        var api = desktopSurface.fileManagerApiRef
        if (!api || !api.storageLocations) {
            return []
        }
        var all = api.storageLocations() || []
        var out = []
        for (var i = 0; i < all.length; ++i) {
            var row = all[i] || ({})
            // Include rows that are explicitly mounted and not the home/root partition.
            // Rows without a "device" field are virtual (Recent, Network) — skip.
            var device = String(row.device || "").trim()
            var rowPath = String(row.path || row.rootPath || "").trim()
            if (device.length <= 0) {
                continue
            }
            // Exclude the root filesystem and the user's home directory volume.
            if (rowPath === "/" || rowPath === "/home") {
                continue
            }
            if (row.mounted === false) {
                // Show unmounted devices too — user can click to mount.
                out.push(row)
            } else {
                out.push(row)
            }
        }
        return out
    }

    // Builds the flat items list for the "Mounted Devices" submenu section.
    // These items are inlined into the Storage menu starting at id 20.
    // Item IDs 20–49 are reserved for dynamic mounted device entries.
    function _mountedDeviceItems() {
        var rows = _mountedDeviceRows()
        if (rows.length === 0) {
            return [{ "id": 20, "label": "No Devices Mounted", "enabled": false }]
        }
        var items = []
        for (var i = 0; i < rows.length && i < 20; ++i) {
            var row = rows[i] || ({})
            var label = String(row.label || row.name || row.device || "").trim()
            if (label.length === 0) {
                label = String(row.device || ("Device " + (i + 1)))
            }
            var mounted = row.mounted !== false
            // Append mount path or "(not mounted)" suffix for clarity.
            var rowPath = String(row.path || row.rootPath || "").trim()
            if (!mounted) {
                label = label + " (not mounted)"
            }
            items.push({
                           "id": 20 + i,
                           "label": label,
                           "enabled": true,
                           "_deviceRow": row
                       })
        }
        return items
    }

    // ── toggle state helpers ──────────────────────────────────────────────────

    function _showHiddenFiles() {
        if (desktopSurface && desktopSurface.fileModel
                && desktopSurface.fileModel.includeHidden !== undefined) {
            return !!desktopSurface.fileModel.includeHidden
        }
        return false
    }

    function _showFileExtensions() {
        if (desktopSurface && desktopSurface.showFileExtensions !== undefined) {
            return !!desktopSurface.showFileExtensions
        }
        return true
    }

    // ── contract: topLevelMenus() ─────────────────────────────────────────────

    function topLevelMenus() {
        return [
            { "id": 3101, "label": "Desktop",   "enabled": true, "source": "desktop-provider" },
            { "id": 3102, "label": "Edit",       "enabled": true, "source": "desktop-provider" },
            { "id": 3103, "label": "Storage",    "enabled": true, "source": "desktop-provider" },
            { "id": 3104, "label": "Workspace",  "enabled": true, "source": "desktop-provider" }
        ]
    }

    // ── contract: ownsMenu(menuId) ────────────────────────────────────────────

    function ownsMenu(menuId) {
        var id = Number(menuId || 0)
        return id >= 3101 && id <= 3104
    }

    // ── contract: menuItemsFor(menuId) ────────────────────────────────────────

    function menuItemsFor(menuId) {
        var id = Number(menuId || 0)

        // ── 3101 Desktop ──────────────────────────────────────────────────────
        if (id === 3101) {
            return [
                { "id": 1, "label": "New Folder",         "enabled": true },
                { "id": 2, "label": "New Text Document",   "enabled": true },
                { "separator": true },
                { "id": 3, "label": "Clean Up Desktop",    "enabled": true },
                { "id": 4, "label": "Sort By",             "enabled": true,
                  "submenu": [
                      { "id": 40, "label": "Name",          "enabled": true },
                      { "id": 41, "label": "Date Modified", "enabled": true },
                      { "id": 42, "label": "Size",          "enabled": true },
                      { "id": 43, "label": "Kind",          "enabled": true }
                  ] },
                { "separator": true },
                { "id": 5, "label": "Show Hidden Files",    "enabled": true,
                  "checked": _showHiddenFiles() },
                { "id": 6, "label": "Show File Extensions", "enabled": true,
                  "checked": _showFileExtensions() },
                { "separator": true },
                { "id": 7, "label": "Desktop Wallpaper…", "enabled": true },
                { "id": 8, "label": "View Options…",      "enabled": true }
            ]
        }

        // ── 3102 Edit ─────────────────────────────────────────────────────────
        if (id === 3102) {
            return [
                { "id": 1, "label": "Undo",       "enabled": true },
                { "id": 2, "label": "Redo",        "enabled": true },
                { "separator": true },
                { "id": 3, "label": "Cut",         "enabled": hasSelection() },
                { "id": 4, "label": "Copy",        "enabled": hasSelection() },
                { "id": 5, "label": "Paste",       "enabled": _hasClipboard() },
                { "separator": true },
                { "id": 6, "label": "Select All",  "enabled": true },
                { "id": 7, "label": "Rename…","enabled": hasSelection() }
            ]
        }

        // ── 3103 Storage ──────────────────────────────────────────────────────
        if (id === 3103) {
            var deviceItems = _mountedDeviceItems()
            var hasDevices = deviceItems.length > 0 && deviceItems[0].enabled !== false
            return [
                { "id": 1, "label": "Open Home Folder",    "enabled": true },
                { "id": 2, "label": "Open Downloads",       "enabled": true },
                { "separator": true },
                { "id": 3, "label": "Connect to Server…", "enabled": true },
                { "separator": true },
                { "id": 4, "label": "Mounted Devices",      "enabled": true,
                  "submenu": deviceItems },
                { "id": 5, "label": "Eject All…",         "enabled": hasDevices },
                { "separator": true },
                { "id": 6, "label": "Trash",                "enabled": true }
            ]
        }

        // ── 3104 Workspace ────────────────────────────────────────────────────
        if (id === 3104) {
            return [
                { "id": 1, "label": "Focus Left",           "enabled": true },
                { "id": 2, "label": "Focus Right",          "enabled": true },
                { "separator": true },
                { "id": 3, "label": "Split Left",           "enabled": true },
                { "id": 4, "label": "Split Right",          "enabled": true },
                { "separator": true },
                { "id": 5, "label": "Open Terminal",        "enabled": true },
                { "id": 6, "label": "Open File Manager",    "enabled": true },
                { "separator": true },
                { "id": 7, "label": "Pin to This Workspace","enabled": true },
                { "id": 8, "label": "Show Desktop",         "enabled": true }
            ]
        }

        return []
    }

    // ── contract: activateMenuItem(menuId, itemId) ────────────────────────────

    function activateMenuItem(menuId, itemId) {
        if (!desktopSurface) {
            return
        }
        var menu = Number(menuId || 0)
        var item = Number(itemId || 0)

        // ── 3101 Desktop ──────────────────────────────────────────────────────
        if (menu === 3101) {
            // 1 — New Folder
            if (item === 1) {
                if (desktopSurface.createNewFolder) {
                    desktopSurface.createNewFolder()
                }
                return
            }
            // 2 — New Text Document
            if (item === 2) {
                if (desktopSurface.createNewFile) {
                    desktopSurface.createNewFile()
                } else {
                    _notifyInfo("New Text Document is not available yet.")
                }
                return
            }
            // 3 — Clean Up Desktop
            if (item === 3) {
                if (desktopSurface.cleanUpDesktop) {
                    desktopSurface.cleanUpDesktop()
                } else if (desktopSurface.fileModel && desktopSurface.fileModel.setSort) {
                    desktopSurface.fileModel.setSort("name", false)
                } else {
                    _notifyInfo("Clean Up Desktop is not available yet.")
                }
                return
            }
            // 40–43 — Sort By submenu: Name / Date Modified / Size / Kind
            if (item >= 40 && item <= 43) {
                if (desktopSurface.fileModel && desktopSurface.fileModel.setSort) {
                    var sortKeyMap = { 40: "name", 41: "dateModified", 42: "size", 43: "kind" }
                    var key = sortKeyMap[item] || "name"
                    desktopSurface.fileModel.setSort(key, false)
                } else {
                    _notifyInfo("Sort is not available yet.")
                }
                return
            }
            // 5 — Toggle Show Hidden Files
            if (item === 5) {
                if (desktopSurface.fileModel && desktopSurface.fileModel.includeHidden !== undefined) {
                    desktopSurface.fileModel.includeHidden = !desktopSurface.fileModel.includeHidden
                } else {
                    _notifyInfo("Show Hidden Files is not available yet.")
                }
                return
            }
            // 6 — Toggle Show File Extensions
            if (item === 6) {
                if (desktopSurface.toggleShowFileExtensions) {
                    desktopSurface.toggleShowFileExtensions()
                } else if (desktopSurface.showFileExtensions !== undefined) {
                    desktopSurface.showFileExtensions = !desktopSurface.showFileExtensions
                } else {
                    _notifyInfo("Show File Extensions is not available yet.")
                }
                return
            }
            // 7 — Desktop Wallpaper…
            if (item === 7) {
                if (typeof ShellStateController !== "undefined"
                        && ShellStateController
                        && ShellStateController.openWallpaperSettings) {
                    ShellStateController.openWallpaperSettings()
                } else {
                    _route("settings.open", { "section": "wallpaper" })
                }
                return
            }
            // 8 — View Options…
            if (item === 8) {
                if (desktopSurface.openViewOptionsDialog) {
                    desktopSurface.openViewOptionsDialog()
                } else {
                    _notifyInfo("View Options is not available yet.")
                }
                return
            }
            return
        }

        // ── 3102 Edit ─────────────────────────────────────────────────────────
        if (menu === 3102) {
            if (item === 1) {
                if (desktopSurface.desktopUndo) { desktopSurface.desktopUndo() }
                return
            }
            if (item === 2) {
                if (desktopSurface.desktopRedo) { desktopSurface.desktopRedo() }
                return
            }
            // 3 — Cut
            if (item === 3) {
                if (desktopSurface.copySelection) { desktopSurface.copySelection(true) }
                return
            }
            // 4 — Copy
            if (item === 4) {
                if (desktopSurface.copySelection) { desktopSurface.copySelection(false) }
                return
            }
            // 5 — Paste
            if (item === 5) {
                if (desktopSurface.pasteSelectionToDesktop) { desktopSurface.pasteSelectionToDesktop() }
                return
            }
            // 6 — Select All
            if (item === 6) {
                if (desktopSurface.selectAllEntries) { desktopSurface.selectAllEntries() }
                return
            }
            // 7 — Rename…
            if (item === 7) {
                if (desktopSurface.renameSelection) { desktopSurface.renameSelection() }
                return
            }
            return
        }

        // ── 3103 Storage ──────────────────────────────────────────────────────
        if (menu === 3103) {
            // 1 — Open Home Folder
            if (item === 1) {
                if (desktopSurface.openPathInFileManager) {
                    desktopSurface.openPathInFileManager("~")
                } else {
                    _route("filemanager.open", { "target": "~" })
                }
                return
            }
            // 2 — Open Downloads
            if (item === 2) {
                if (desktopSurface.openPathInFileManager) {
                    desktopSurface.openPathInFileManager("~/Downloads")
                } else {
                    _route("filemanager.open", { "target": "~/Downloads" })
                }
                return
            }
            // 3 — Connect to Server…
            if (item === 3) {
                if (desktopSurface.openConnectServerDialog) {
                    desktopSurface.openConnectServerDialog()
                } else {
                    _notifyInfo("Connect to Server is not available yet.")
                }
                return
            }
            // 5 — Eject All…
            if (item === 5) {
                var allRows = _mountedDeviceRows()
                var ejected = 0
                for (var ei = 0; ei < allRows.length; ++ei) {
                    var erow = allRows[ei] || ({})
                    var edevice = String(erow.device || "").trim()
                    if (edevice.length > 0 && erow.mounted !== false) {
                        _route("storage.unmount", { "devicePath": edevice })
                        ejected++
                    }
                }
                if (ejected === 0) {
                    _notifyInfo("No mounted devices to eject.")
                }
                return
            }
            // 6 — Trash
            if (item === 6) {
                if (desktopSurface.openPathInFileManager) {
                    desktopSurface.openPathInFileManager("__trash__")
                } else {
                    _route("filemanager.open", { "target": "__trash__" })
                }
                return
            }
            // 20–39 — Dynamic mounted device items from the Mounted Devices submenu
            if (item >= 20 && item < 40) {
                var deviceIndex = item - 20
                var rows = _mountedDeviceRows()
                if (deviceIndex >= rows.length) {
                    return
                }
                var row = rows[deviceIndex] || ({})
                var rowPath = String(row.path || row.rootPath || "").trim()
                var device = String(row.device || "").trim()
                var mounted = row.mounted !== false
                if (mounted && rowPath.length > 0 && rowPath.indexOf("__mount__:") !== 0) {
                    if (desktopSurface.openPathInFileManager) {
                        desktopSurface.openPathInFileManager(rowPath)
                    } else {
                        _route("filemanager.open", { "target": rowPath })
                    }
                } else if (device.length > 0) {
                    _route("storage.mount", { "devicePath": device })
                } else {
                    _notifyInfo("Cannot open device: path unavailable.")
                }
                return
            }
            return
        }

        // ── 3104 Workspace ────────────────────────────────────────────────────
        if (menu === 3104) {
            // 1 — Focus Left
            if (item === 1) {
                if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
                    WindowingBackend.sendCommand("focus left")
                } else {
                    _notifyInfo("Focus Left is unavailable.")
                }
                return
            }
            // 2 — Focus Right
            if (item === 2) {
                if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
                    WindowingBackend.sendCommand("focus right")
                } else {
                    _notifyInfo("Focus Right is unavailable.")
                }
                return
            }
            // 3 — Split Left
            if (item === 3) {
                _route("workspace.split_left", {})
                return
            }
            // 4 — Split Right
            if (item === 4) {
                _route("workspace.split_right", {})
                return
            }
            // 5 — Open Terminal
            // Strategy: focus existing if running, else launch new.
            // AppStateClient.isRunning("terminal") checks appdeck/process state.
            // If already running, route via app.desktopid to raise the existing window.
            // If not, fall through to launchTerminalAt(desktopPath).
            if (item === 5) {
                var terminalRunning = (typeof AppStateClient !== "undefined"
                                       && AppStateClient
                                       && AppStateClient.isRunning
                                       && AppStateClient.isRunning("terminal"))
                if (terminalRunning) {
                    // Attempt to raise existing window via desktopid routing.
                    // AppCommandRouter.route("app.desktopid") will bring existing
                    // terminal window to front if the compositor tracks it.
                    var focusRes = false
                    if (typeof AppCommandRouter !== "undefined"
                            && AppCommandRouter
                            && AppCommandRouter.routeWithResult) {
                        var res = AppCommandRouter.routeWithResult(
                                      "app.desktopid",
                                      { "desktopId": "terminal" },
                                      "desktop-menu-provider")
                        focusRes = !!(res && res.ok)
                    }
                    if (focusRes) {
                        return
                    }
                    // Fallthrough: routing failed, launch a new window.
                }
                // Launch new terminal at desktop path.
                if (typeof AppExecutionGate !== "undefined"
                        && AppExecutionGate
                        && AppExecutionGate.launchTerminalAt) {
                    AppExecutionGate.launchTerminalAt(
                        String(desktopSurface.desktopPath || "~/Desktop"))
                } else {
                    // Final fallback: terminal.exec with empty command will
                    // be rejected by router, but try anyway for future compatibility.
                    _route("terminal.exec", { "command": "", "cwd": String(desktopSurface.desktopPath || "~/Desktop") })
                }
                return
            }
            // 6 — Open File Manager
            // Strategy: filemanager.open is idempotent by design in AppCommandRouter —
            // if a file manager window is already open the gate raises it rather than
            // spawning a duplicate. No separate focus check needed here.
            if (item === 6) {
                _route("filemanager.open", { "target": String(desktopSurface.desktopPath || "~/Desktop") })
                return
            }
            // 7 — Pin to This Workspace
            if (item === 7) {
                _route("workspace.pin_current", {})
                return
            }
            // 8 — Show Desktop
            if (item === 8) {
                if (typeof ShellStateController !== "undefined"
                        && ShellStateController
                        && ShellStateController.setShowDesktop) {
                    var current = !!(ShellStateController.showDesktop)
                    ShellStateController.setShowDesktop(!current)
                } else {
                    _route("workspace.toggle", {})
                }
                return
            }
            return
        }
    }

    // ── override registration ─────────────────────────────────────────────────
    //
    // The desktop provider is the "default" menu shown in the global menu bar
    // when no app has focus. We register our top-level menus as override on
    // GlobalMenuManager so that GlobalMenuManager.topLevelMenus reports them
    // through its normal property (which CrownBrandSection consumes), and we
    // clear the override whenever an app gains focus so the app's own dbus
    // menus take precedence.
    //
    // Items are NOT embedded in the override payload — they are resolved live
    // by CrownBrandSection via menuItemsFor(menuId) when the dropdown opens,
    // because source="desktop-provider" routes back to this QML object.

    function _hasFocusedApp() {
        if (typeof AppStateClient !== "undefined" && AppStateClient) {
            var id = String(AppStateClient.focusedAppId || "")
            return id.length > 0
        }
        return false
    }

    function syncGlobalMenuOverride() {
        if (typeof GlobalMenuManager === "undefined" || !GlobalMenuManager) {
            return
        }
        if (!enabled || _hasFocusedApp()) {
            if (GlobalMenuManager.clearOverrideMenus) {
                GlobalMenuManager.clearOverrideMenus()
            }
            return
        }
        if (GlobalMenuManager.setOverrideMenus) {
            GlobalMenuManager.setOverrideMenus(topLevelMenus(), overrideContext)
        }
    }

    onEnabledChanged: syncGlobalMenuOverride()

    // Re-sync when focus changes — desktop menu visible only when no app focused.
    property var _focusWatcher: Connections {
        target: (typeof AppStateClient !== "undefined") ? AppStateClient : null
        ignoreUnknownSignals: true
        function onFocusedAppIdChanged() {
            root.syncGlobalMenuOverride()
        }
    }
}
