import QtQuick 2.15
import "../shell/ShellUtils.js" as ShellUtils

QtObject {
    id: root

    property var rootWindow: null
    property var fileManagerContent: null
    property var detachedFileManagerWindow: null

    signal requestFocusPulse()
    signal requestHelpMessage(string message)
    signal requestMenuNotification(bool ok, string summary, string body, string iconName)

    function _route(action, payload, source) {
        if (typeof AppCommandRouter === "undefined" || !AppCommandRouter || !AppCommandRouter.routeWithResult) {
            return { "ok": false, "error": "router-unavailable" }
        }
        return AppCommandRouter.routeWithResult(String(action || ""),
                                                payload ? payload : ({}),
                                                String(source || "global-menu"))
    }

    function _withMenuMeta(payload, menuId, itemId) {
        var out = {}
        var src = payload ? payload : ({})
        for (var k in src) {
            if (Object.prototype.hasOwnProperty.call(src, k)) {
                out[k] = src[k]
            }
        }
        out.__menuId = Number(menuId || 0)
        out.__itemId = Number(itemId || 0)
        return out
    }

    function _friendlyStorageError(errorCode) {
        var k = String(errorCode || "").toLowerCase()
        if (k.length <= 0) {
            return "Operasi drive gagal."
        }
        if (k === "missing-device-path" || k === "mount-not-found" || k === "volume-not-available") {
            return "Drive tidak ditemukan."
        }
        if (k === "storage-service-unavailable" || k === "storage-dbus-error") {
            return "Layanan drive tidak tersedia."
        }
        if (k.indexOf("busy") >= 0 || k.indexOf("in-use") >= 0) {
            return "Drive sedang digunakan."
        }
        if (k.indexOf("permission") >= 0 || k.indexOf("denied") >= 0) {
            return "Akses ke drive ditolak."
        }
        if (k.indexOf("timeout") >= 0) {
            return "Drive tidak merespons tepat waktu."
        }
        if (k.indexOf("lock") >= 0 || k.indexOf("encrypted") >= 0) {
            return "Drive terkunci."
        }
        if (k.indexOf("unsupported") >= 0 || k.indexOf("unknown") >= 0) {
            return "Drive tidak dikenali."
        }
        return "Operasi drive gagal."
    }

    function _routeOrWarn(action, payload, source, hint) {
        var res = _route(action, payload, source)
        if (!res || !res.ok) {
            var err = String((res && res.error) ? res.error : "action-failed")
            var label = String(hint || "Global Menu action failed")
            if (String(action || "").indexOf("storage.") === 0) {
                requestHelpMessage(_friendlyStorageError(err))
            } else {
                requestHelpMessage(label + ": " + err)
            }
        }
        return res
    }

    function _notify(ok, summary, body, iconName) {
        requestMenuNotification(!!ok,
                                String(summary || "Global Menu"),
                                String(body || ""),
                                String(iconName || "dialog-information-symbolic"))
    }

    function _resolveStorageDeviceTarget() {
        var out = {
            "device": "",
            "path": "",
            "mounted": false,
            "row": null
        }
        if (!fileManagerContent) {
            return out
        }
        function decodeMountPath(pathValue) {
            var p = String(pathValue || "")
            if (p.indexOf("__mount__:") === 0) {
                return decodeURIComponent(p.slice(10))
            }
            return ""
        }
        function normalizePath(pathValue) {
            return String(pathValue || "").trim()
        }
        function storageRows() {
            if (fileManagerContent.fileManagerApiRef
                    && fileManagerContent.fileManagerApiRef.storageLocations) {
                return fileManagerContent.fileManagerApiRef.storageLocations() || []
            }
            return []
        }
        function matchRowByPath(rows, needle) {
            var n = normalizePath(needle)
            if (n.length <= 0) {
                return null
            }
            for (var i = 0; i < rows.length; ++i) {
                var row = rows[i] || ({})
                var rowPath = normalizePath(row.path || row.rootPath || "")
                if (rowPath.length > 0 && rowPath === n) {
                    return row
                }
            }
            return null
        }

        var rows = storageRows()
        var contextDevice = normalizePath(fileManagerContent.sidebarContextDevice || "")
        var contextPath = normalizePath(fileManagerContent.sidebarContextPath || "")
        var selectedSidebarPath = normalizePath(fileManagerContent.selectedSidebarPath || "")
        var currentPath = (fileManagerContent.fileModel && fileManagerContent.fileModel.currentPath)
                ? normalizePath(fileManagerContent.fileModel.currentPath) : ""

        var row = null
        if (contextPath.length > 0) {
            row = matchRowByPath(rows, contextPath)
        }
        if (!row && selectedSidebarPath.length > 0) {
            row = matchRowByPath(rows, selectedSidebarPath)
        }
        if (!row && currentPath.length > 0) {
            row = matchRowByPath(rows, currentPath)
        }
        if (!row && contextDevice.length > 0) {
            for (var ci = 0; ci < rows.length; ++ci) {
                var crow = rows[ci] || ({})
                if (normalizePath(crow.device || "") === contextDevice) {
                    row = crow
                    break
                }
            }
        }

        if (row) {
            out.row = row
            out.device = normalizePath(row.device || "")
            out.path = normalizePath(row.path || row.rootPath || "")
            out.mounted = !!row.mounted
            if (out.device.length > 0) {
                return out
            }
        }

        if (contextDevice.length > 0) {
            out.device = contextDevice
            out.path = contextPath
            out.mounted = !!fileManagerContent.sidebarContextMounted
            return out
        }
        var selectedMountDevice = decodeMountPath(selectedSidebarPath)
        if (selectedMountDevice.length > 0) {
            out.device = selectedMountDevice
            out.path = selectedSidebarPath
            return out
        }
        var currentMountDevice = decodeMountPath(currentPath)
        if (currentMountDevice.length > 0) {
            out.device = currentMountDevice
            out.path = currentPath
            return out
        }
        if (currentPath.indexOf("/dev/") === 0) {
            out.device = currentPath
            out.path = currentPath
            return out
        }
        return out
    }

    function handleMenuItem(menuId, itemId, label, context) {
        var menu = Number(menuId || 0)
        var item = Number(itemId || 0)
        var ctx = String(context || "")
        if (ctx.length <= 0) return

        // 1. GLOBAL MENU ROUTING (Independent of File Manager context)
        if (menu === 2101 && item === 12) { // Go > Connect Server
            console.log("GlobalMenu: Connect Server requested, context=", ctx)
            if (fileManagerContent && fileManagerContent.openConnectServerDialog) {
                console.log("GlobalMenu: Routing to fileManagerContent")
                fileManagerContent.openConnectServerDialog()
            } else if (rootWindow) {
                console.log("GlobalMenu: Routing to ShellUtils (detached)")
                ShellUtils.openDetachedConnectServerDialog(rootWindow)
            }
            return
        }

        // 2. FILE MANAGER CONTEXTUAL MENU
        if (ctx === "slm-filemanager" && fileManagerContent) {
            var currentPath = (fileManagerContent.fileModel && fileManagerContent.fileModel.currentPath)
                    ? String(fileManagerContent.fileModel.currentPath) : "~"

            function openPathSafe(pathValue) {
                if (fileManagerContent.openPath) fileManagerContent.openPath(String(pathValue || "~"))
            }

            function unsupported(title) {
                requestHelpMessage(String(title || "Action") + " is not available yet.")
            }

            if (menu === 2001) { // File
                if (item === 1 && fileManagerContent.createNewFolder) { fileManagerContent.createNewFolder(); return }
                if (item === 2 && fileManagerContent.createNewFile) { fileManagerContent.createNewFile(); return }
                if (item === 3 && fileManagerContent.openSelectedEntries) { fileManagerContent.openSelectedEntries(); return }
                if (item === 4 && fileManagerContent.openInOtherApplication) { fileManagerContent.openInOtherApplication(); return }
                if (item === 5) {
                    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.routeWithResult) {
                        var quotedPath = fileManagerContent.shellSingleQuote
                                ? fileManagerContent.shellSingleQuote(currentPath)
                                : ("'" + String(currentPath).replace(/'/g, "'\\''") + "'")
                        var terminalCmd = "cd " + quotedPath
                        var termRes = AppCommandRouter.routeWithResult("terminal.exec", { "command": terminalCmd }, "global-menu-filemanager")
                        if (!termRes || !termRes.ok) requestHelpMessage("Open Terminal Here failed.")
                    } else {
                        requestHelpMessage("Open Terminal Here is unavailable.")
                    }
                    return
                }
                if (item === 6 && fileManagerContent.openHome) { fileManagerContent.openHome(); return }
                if (item === 7 && fileManagerContent.compressSelection) { fileManagerContent.compressSelection(); return }
                if (item === 8 && fileManagerContent.deleteSelected) { fileManagerContent.deleteSelected(false); return }
                if (item === 9 && fileManagerContent.deleteSelected) { fileManagerContent.deleteSelected(true); return }
                return
            }

            if (menu === 2002) { // Edit
                if (item === 1) { unsupported("Undo"); return }
                if (item === 2) { unsupported("Redo"); return }
                if (item === 3 && fileManagerContent.copySelected) { fileManagerContent.copySelected(true); return }
                if (item === 4 && fileManagerContent.copySelected) { fileManagerContent.copySelected(false); return }
                if (item === 5 && fileManagerContent.pasteIntoCurrent) { fileManagerContent.pasteIntoCurrent(); return }
                if (item === 6) {
                    if (fileManagerContent.copySelected && fileManagerContent.pasteIntoCurrent) {
                        fileManagerContent.copySelected(false)
                        fileManagerContent.pasteIntoCurrent()
                    }
                    return
                }
                if (item === 7 && fileManagerContent.selectAllVisibleEntries) { fileManagerContent.selectAllVisibleEntries(); return }
                if (item === 8 && fileManagerContent.clearSelection) { fileManagerContent.clearSelection(); return }
                if (item === 9 && fileManagerContent.renameSelected) { fileManagerContent.renameSelected(); return }
                return
            }

            if (menu === 2003) { // View
                if (item === 1) { fileManagerContent.viewMode = "grid"; return }
                if (item === 2) {
                    if (fileManagerContent.fileModel && fileManagerContent.fileModel.setSort) fileManagerContent.fileModel.setSort("name", false)
                    return
                }
                if (item === 3) {
                    if (fileManagerContent.fileModel && fileManagerContent.fileModel.setSort) {
                        var key = String(fileManagerContent.fileModel.sortKey || "dateModified")
                        var descending = !!fileManagerContent.fileModel.sortDescending
                        fileManagerContent.fileModel.setSort(key, !descending)
                    }
                    return
                }
                if (item === 4) {
                    if (fileManagerContent.fileModel && fileManagerContent.fileModel.directoriesFirst !== undefined)
                        fileManagerContent.fileModel.directoriesFirst = !fileManagerContent.fileModel.directoriesFirst
                    return
                }
                if (item === 5) {
                    if (fileManagerContent.fileModel && fileManagerContent.fileModel.includeHidden !== undefined)
                        fileManagerContent.fileModel.includeHidden = !fileManagerContent.fileModel.includeHidden
                    return
                }
                if (item === 6) { unsupported("Show File Extensions"); return }
                if (item === 7) { unsupported("Icon Size"); return }
                if (item === 8) { unsupported("Grid Spacing"); return }
                if (item === 9 && fileManagerContent.showPropertiesForSelection) { fileManagerContent.showPropertiesForSelection(); return }
                return
            }

            if (menu === 2101) { // Go (Contextual)
                if (item === 1 && fileManagerContent.openHome) { fileManagerContent.openHome(); return }
                if (item === 2 && fileManagerContent.openPath) { fileManagerContent.openPath("~/Desktop"); return }
                if (item === 3 && fileManagerContent.openPath) { fileManagerContent.openPath("~/Documents"); return }
                if (item === 4 && fileManagerContent.openPath) { fileManagerContent.openPath("~/Downloads"); return }
                if (item === 5 && fileManagerContent.openPath) { fileManagerContent.openPath("~/Pictures"); return }
                if (item === 6 && fileManagerContent.openPath) { fileManagerContent.openPath("~/Videos"); return }
                if (item === 11 && fileManagerContent.openPath) { fileManagerContent.openPath("__trash__"); return }
                return
            }

            if (menu === 2103) { // Storage
                if (item === 1 && fileManagerContent.openHome) { fileManagerContent.openHome(); return }
                if (item === 2 && fileManagerContent.openPath) { fileManagerContent.openPath("~/Downloads"); return }
                if (item === 3 && fileManagerContent.openPath) { fileManagerContent.openPath("__trash__"); return }
                return
            }

            if (menu === 2004) { // Tools
                if (item === 1) { ShellUtils.openDetachedFileManager(rootWindow, currentPath); return }
                if (item === 2) {
                    if (fileManagerContent.openPathInNewTab) fileManagerContent.openPathInNewTab(currentPath)
                    return
                }
                if (item === 3) { requestFocusPulse(); return }
                if (item === 4) { unsupported("Batch Rename"); return }
                if (item === 5 && fileManagerContent.compressSelection) { fileManagerContent.compressSelection(); return }
                if (item === 6) {
                    if (fileManagerContent.copySelectedAsLinkToClipboard) fileManagerContent.copySelectedAsLinkToClipboard()
                    else unsupported("Create Shortcut / Link")
                    return
                }
                if (item === 7 && fileManagerContent.showPropertiesForSelection) { fileManagerContent.showPropertiesForSelection(); return }
                return
            }

            if (menu === 2005) { // Workspace
                if (item === 1) {
                    _routeOrWarn("workspace.toggle", _withMenuMeta({}, menu, item), "global-menu", "Workspace action failed")
                    return
                }
                if (item === 2 || item === 3) {
                    if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand)
                        WindowingBackend.sendCommand(item === 2 ? "focus left" : "focus right")
                    else unsupported(item === 2 ? "Move Focus Left" : "Move Focus Right")
                    return
                }
                if (item === 4) {
                    _routeOrWarn("workspace.pin_current", _withMenuMeta({}, menu, item), "global-menu", "Workspace pin failed")
                    return
                }
                if (item === 5) {
                    _routeOrWarn("workspace.split_right", _withMenuMeta({}, menu, item), "global-menu", "Workspace split failed")
                    openPathSafe("~/Desktop")
                    return
                }
                return
            }

            if (menu === 2102) { // Devices
                var target = _resolveStorageDeviceTarget()
                var deviceTarget = String(target.device || "").trim()
                if (item === 1 && target.row && fileManagerContent.openStorageVolumeChoice) {
                    fileManagerContent.openStorageVolumeChoice(target.row)
                    _notify(true, "Drive opened", String(target.row.label || target.row.name || "Storage"), "drive-removable-media-symbolic")
                    return
                }
                if (item === 1 && deviceTarget.length > 0) {
                    var mountRes = _routeOrWarn("storage.mount", _withMenuMeta({ "devicePath": deviceTarget }, menu, item), "global-menu", "Storage mount failed")
                    if (mountRes && mountRes.ok) _notify(true, "Drive mounted", deviceTarget, "drive-removable-media-symbolic")
                    if ((!mountRes || !mountRes.ok) && fileManagerContent.openStorageVolumeChoice) {
                        fileManagerContent.openStorageVolumeChoice({ "device": deviceTarget, "path": String(target.path || ""), "mounted": !!target.mounted })
                    }
                    return
                }
                if (item === 2 && deviceTarget.length > 0) {
                    var unmountRes = _routeOrWarn("storage.unmount", _withMenuMeta({ "devicePath": deviceTarget }, menu, item), "global-menu", "Storage unmount failed")
                    if (unmountRes && unmountRes.ok) _notify(true, "Drive ejected", deviceTarget, "media-eject-symbolic")
                    if ((!unmountRes || !unmountRes.ok) && fileManagerContent.fileManagerApiRef && fileManagerContent.fileManagerApiRef.startUnmountStorageDevice)
                        fileManagerContent.fileManagerApiRef.startUnmountStorageDevice(deviceTarget)
                    return
                }
                requestHelpMessage("Pilih drive di sidebar terlebih dahulu.")
                return
            }
        } // End File Manager Context

        // 3. GLOBAL MENU FALLBACK (Applicable without active file manager)
        if (menu === 2001) { // File
            if (item === 1) { // New Window
                ShellUtils.openDetachedFileManager(rootWindow, "~")
                return
            }
            if (item === 2) { // New Tab
                ShellUtils.openDetachedFileManager(rootWindow, "~")
                return
            }
            if (item === 5) { Qt.quit(); return }
        } else if (menu === 2101) { // Go Fallback
            if (item === 1) { ShellUtils.openDetachedFileManager(rootWindow, "~"); return }
            if (item === 2) { ShellUtils.openDetachedFileManager(rootWindow, "~/Desktop"); return }
            if (item === 3) { ShellUtils.openDetachedFileManager(rootWindow, "~/Documents"); return }
            if (item === 4) { ShellUtils.openDetachedFileManager(rootWindow, "~/Downloads"); return }
            if (item === 5) { ShellUtils.openDetachedFileManager(rootWindow, "~/Pictures"); return }
            if (item === 6) { ShellUtils.openDetachedFileManager(rootWindow, "~/Videos"); return }
            if (item === 11) { ShellUtils.openDetachedFileManager(rootWindow, "__trash__"); return }
        } else if (menu === 2103) { // Storage Fallback
            if (item === 1) { ShellUtils.openDetachedFileManager(rootWindow, "~"); return }
            if (item === 2) { ShellUtils.openDetachedFileManager(rootWindow, "~/Downloads"); return }
            if (item === 3) { ShellUtils.openDetachedFileManager(rootWindow, "__trash__"); return }
        } else if (menu === 2006) { // Help
            requestHelpMessage("SLM Desktop Help: use Crown menu and shortcuts.")
            return
        }
    }
}
