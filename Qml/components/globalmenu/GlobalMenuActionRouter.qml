import QtQuick 2.15
import "../shell/ShellUtils.js" as ShellUtils

QtObject {
    id: root

    property var rootWindow: null
    property var fileManagerContent: null
    property var detachedFileManagerWindow: null

    signal requestFocusTothespot()
    signal requestHelpMessage(string message)

    function _route(action, payload, source) {
        if (typeof AppCommandRouter === "undefined" || !AppCommandRouter || !AppCommandRouter.routeWithResult) {
            return { "ok": false, "error": "router-unavailable" }
        }
        return AppCommandRouter.routeWithResult(String(action || ""),
                                                payload ? payload : ({}),
                                                String(source || "global-menu"))
    }

    function _routeOrWarn(action, payload, source, hint) {
        var res = _route(action, payload, source)
        if (!res || !res.ok) {
            var err = String((res && res.error) ? res.error : "action-failed")
            var label = String(hint || "Global Menu action failed")
            requestHelpMessage(label + ": " + err)
        }
        return res
    }

    function _resolveStorageDeviceTarget() {
        if (!fileManagerContent) {
            return ""
        }
        var contextDevice = String(fileManagerContent.sidebarContextDevice || "").trim()
        if (contextDevice.length > 0) {
            return contextDevice
        }
        var currentPath = (fileManagerContent.fileModel && fileManagerContent.fileModel.currentPath)
                ? String(fileManagerContent.fileModel.currentPath) : ""
        if (currentPath.indexOf("__mount__:") === 0) {
            return decodeURIComponent(currentPath.slice(10))
        }
        if (currentPath.indexOf("/dev/") === 0) {
            return currentPath
        }
        if (fileManagerContent.fileManagerApiRef && fileManagerContent.fileManagerApiRef.storageLocations) {
            var rows = fileManagerContent.fileManagerApiRef.storageLocations() || []
            for (var i = 0; i < rows.length; ++i) {
                var row = rows[i] || ({})
                var rowPath = String(row.path || row.rootPath || "").trim()
                var rowDevice = String(row.device || "").trim()
                if (rowDevice.length <= 0) {
                    continue
                }
                if (rowPath.length > 0 && rowPath === currentPath) {
                    return rowDevice
                }
            }
        }
        return ""
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
            var deviceTarget = _resolveStorageDeviceTarget()
            if (item === 1 && deviceTarget.length > 0) {
                var mountRes = _routeOrWarn("storage.mount",
                                            { "devicePath": deviceTarget },
                                            "global-menu",
                                            "Storage mount failed")
                if ((!mountRes || !mountRes.ok)
                        && fileManagerContent.openStorageVolumeChoice) {
                    fileManagerContent.openStorageVolumeChoice({
                                                                   "device": deviceTarget,
                                                                   "mounted": false
                                                               })
                }
                return
            }
            if (item === 2 && deviceTarget.length > 0) {
                var unmountRes = _routeOrWarn("storage.unmount",
                                              { "devicePath": deviceTarget },
                                              "global-menu",
                                              "Storage unmount failed")
                if ((!unmountRes || !unmountRes.ok)
                        && fileManagerContent.fileManagerApiRef
                        && fileManagerContent.fileManagerApiRef.startUnmountStorageDevice) {
                    fileManagerContent.fileManagerApiRef.startUnmountStorageDevice(deviceTarget)
                }
                return
            }
        } else if (menu === 2006) { // Help
            requestHelpMessage("SLM Desktop Help: use top bar menu and shortcuts.")
            return
        }
    }
}
