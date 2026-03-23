import QtQuick 2.15
import "../shell/ShellUtils.js" as ShellUtils

QtObject {
    id: root

    property var rootWindow: null
    property var fileManagerContent: null
    property var detachedFileManagerWindow: null

    signal requestFocusTothespot()
    signal requestHelpMessage(string message)

    function handleMenuItem(menuId, itemId, label, context) {
        var menu = Number(menuId || 0)
        var item = Number(itemId || 0)
        var ctx = String(context || "")
        if (ctx !== "slm-dbus-menu") {
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
        } else if (menu === 2004) { // Go
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
        } else if (menu === 2005) { // Window
            if (item === 1 && rootWindow && rootWindow.detachedFileManagerVisible && detachedFileManagerWindow && detachedFileManagerWindow.showMinimized) {
                detachedFileManagerWindow.showMinimized()
                return
            }
            if (item === 2 && rootWindow && rootWindow.detachedFileManagerVisible && detachedFileManagerWindow && detachedFileManagerWindow.toggleMaximizeRestore) {
                detachedFileManagerWindow.toggleMaximizeRestore()
                return
            }
            if (item === 3 && rootWindow && rootWindow.detachedFileManagerVisible && detachedFileManagerWindow && detachedFileManagerWindow.requestActivate) {
                detachedFileManagerWindow.requestActivate()
                return
            }
        } else if (menu === 2006) { // Help
            requestHelpMessage("SLM Desktop Help: use top bar menu and shortcuts.")
            return
        }
    }
}
