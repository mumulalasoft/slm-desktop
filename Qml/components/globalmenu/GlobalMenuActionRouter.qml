import QtQuick 2.15
import "../shell/ShellUtils.js" as ShellUtils

QtObject {
    id: root

    property var rootWindow: null
    property var fileManagerContent: null
    property var detachedFileManagerWindow: null

    signal requestFocusTothespot()
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
        if (ctx.length <= 0) {
            return
        }

        if (menu === 2001) { // File
            if (item === 1) { // New Window
                ShellUtils.openDetachedFileManager(rootWindow,
                                                   fileManagerContent && fileManagerContent.fileModel && fileManagerContent.fileModel.currentPath
                                                   ? String(fileManagerContent.fileModel.currentPath) : "~")
                return
            }
            if (item === 2) { // New Tab
                if (fileManagerContent && fileManagerContent.openPathInNewTab) {
                    var tabPath = (fileManagerContent.fileModel && fileManagerContent.fileModel.currentPath)
                            ? String(fileManagerContent.fileModel.currentPath) : "~"
                    fileManagerContent.openPathInNewTab(tabPath)
                } else {
                    ShellUtils.openDetachedFileManager(rootWindow, "~")
                }
                return
            }
            if (item === 3) { // Open...
                requestFocusTothespot()
                return
            }
            if (item === 6) { // Print...
                if (fileManagerContent && fileManagerContent.canPrintSelection
                        && fileManagerContent.requestPrintSelection
                        && fileManagerContent.canPrintSelection()) {
                    fileManagerContent.requestPrintSelection()
                }
                return
            }
            if (item === 4) { // Close Window
                if (rootWindow && rootWindow.detachedFileManagerVisible !== undefined) {
                    rootWindow.detachedFileManagerVisible = false
                }
                return
            }
            if (item === 5) { // Quit SLM Desktop
                Qt.quit()
                return
            }
        } else if (menu === 2002) { // Edit
            if (!fileManagerContent) {
                return
            }
            if (item === 3 && fileManagerContent.copySelected) { // Cut
                fileManagerContent.copySelected(true)
                return
            }
            if (item === 4 && fileManagerContent.copySelected) { // Copy
                fileManagerContent.copySelected(false)
                return
            }
            if (item === 5 && fileManagerContent.pasteIntoCurrent) { // Paste
                fileManagerContent.pasteIntoCurrent()
                return
            }
            if (item === 6 && fileManagerContent.selectAllVisibleEntries) { // Select All
                fileManagerContent.selectAllVisibleEntries()
                return
            }
        } else if (menu === 2003) { // View
            if (!fileManagerContent) {
                return
            }
            if (item === 1) {
                fileManagerContent.viewMode = "grid"
                return
            }
            if (item === 2) {
                fileManagerContent.viewMode = "list"
                return
            }
            if (item === 3) {
                fileManagerContent.viewMode = "columns"
                return
            }
            return
        } else if (menu === 2004) { // Tools
            if (!fileManagerContent) {
                return
            }
            if (item === 3 && fileManagerContent.openInTerminal) {
                fileManagerContent.openInTerminal()
                return
            }
            if (item === 4) {
                requestFocusTothespot()
                return
            }
            return
        } else if (menu === 2005) { // Workspace
            if (item === 1) {
                _routeOrWarn("workspace.presentview", { "viewId": "1" }, "global-menu", "Workspace action failed")
                return
            }
            if (item === 2) {
                _routeOrWarn("workspace.presentview", { "viewId": "2" }, "global-menu", "Workspace action failed")
                return
            }
            if (item === 3) {
                _routeOrWarn("workspace.split_left", {}, "global-menu", "Workspace split failed")
                return
            }
            if (item === 4) {
                _routeOrWarn("workspace.split_right", {}, "global-menu", "Workspace split failed")
                return
            }
            if (item === 5) {
                _routeOrWarn("workspace.pin_current", {}, "global-menu", "Workspace pin failed")
                return
            }
            return
        } else if (menu === 2101) { // Go (file-manager contextual)
            if (!fileManagerContent) {
                return
            }
            if (item === 1 && fileManagerContent.navigateBack) {
                fileManagerContent.navigateBack()
                return
            }
            if (item === 2 && fileManagerContent.navigateForward) {
                fileManagerContent.navigateForward()
                return
            }
            if (item === 3 && fileManagerContent.openHome) {
                fileManagerContent.openHome()
                return
            }
            if (item === 4 && fileManagerContent.openPath) {
                fileManagerContent.openPath("~/Documents")
                return
            }
            if (item === 5 && fileManagerContent.openPath) {
                fileManagerContent.openPath("~/Downloads")
                return
            }
            if (item === 6 && fileManagerContent.openPath) {
                fileManagerContent.openPath("~/Pictures")
                return
            }
        } else if (menu === 2102) { // Devices (file-manager contextual)
            if (!fileManagerContent) {
                return
            }
            var target = _resolveStorageDeviceTarget()
            var deviceTarget = String(target.device || "").trim()
            if (item === 1 && target.row && fileManagerContent.openStorageVolumeChoice) {
                fileManagerContent.openStorageVolumeChoice(target.row)
                _notify(true,
                        "Drive opened",
                        String(target.row.label || target.row.name || "Storage"),
                        "drive-removable-media-symbolic")
                return
            }
            if (item === 1 && deviceTarget.length > 0) {
                var mountRes = _routeOrWarn("storage.mount",
                                            { "devicePath": deviceTarget },
                                            "global-menu",
                                            "Storage mount failed")
                if (mountRes && mountRes.ok) {
                    _notify(true, "Drive mounted", deviceTarget, "drive-removable-media-symbolic")
                }
                if ((!mountRes || !mountRes.ok)
                        && fileManagerContent.openStorageVolumeChoice) {
                    fileManagerContent.openStorageVolumeChoice({
                                                                   "device": deviceTarget,
                                                                   "path": String(target.path || ""),
                                                                   "mounted": !!target.mounted
                                                               })
                }
                return
            }
            if (item === 2 && deviceTarget.length > 0) {
                var unmountRes = _routeOrWarn("storage.unmount",
                                              { "devicePath": deviceTarget },
                                              "global-menu",
                                              "Storage unmount failed")
                if (unmountRes && unmountRes.ok) {
                    _notify(true, "Drive ejected", deviceTarget, "media-eject-symbolic")
                }
                if ((!unmountRes || !unmountRes.ok)
                        && fileManagerContent.fileManagerApiRef
                        && fileManagerContent.fileManagerApiRef.startUnmountStorageDevice) {
                    fileManagerContent.fileManagerApiRef.startUnmountStorageDevice(deviceTarget)
                }
                return
            }
            requestHelpMessage("Pilih drive di sidebar terlebih dahulu.")
        } else if (menu === 2006) { // Help
            requestHelpMessage("SLM Desktop Help: use top bar menu and shortcuts.")
            return
        }
    }
}
