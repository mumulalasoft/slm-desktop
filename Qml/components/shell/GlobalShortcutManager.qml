import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "../screenshot/ScreenshotController.js" as ScreenshotController
import "../screenshot/ScreenshotSaveController.js" as ScreenshotSaveController

Item {
    id: root

    required property var shellRoot
    property var topBarWindowRef: null
    property var desktopSceneRef: null
    property int _shortcutRev: 0

    function prefShortcut(key, fallbackValue) {
        _shortcutRev // reactive trigger
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.keyboardShortcut) {
            return String(fallbackValue || "")
        }
        return String(DesktopSettings.keyboardShortcut(String(key || ""), String(fallbackValue || "")) || "")
    }

    readonly property string screenshotQuickBind: prefShortcut("screenshot.bindQuickSave", "Print")
    readonly property string screenshotToolBind: prefShortcut("screenshot.bindTool", "Shift+Print")
    readonly property string screenshotAreaBind: prefShortcut("screenshot.bindArea", "Alt+Shift+S")
    readonly property string screenshotFullscreenBind: prefShortcut("screenshot.bindFullscreen", "Alt+Shift+F")
    readonly property string workspacePanelBind: prefShortcut("windowing.bindWorkspace", "Alt+F3")
    readonly property string workspaceOverviewBind: prefShortcut("shortcuts.workspaceOverview", "Meta+S")
    readonly property string workspacePrevBind: prefShortcut("shortcuts.workspacePrev", "Meta+Left")
    readonly property string workspaceNextBind: prefShortcut("shortcuts.workspaceNext", "Meta+Right")
    readonly property string moveWindowPrevBind: prefShortcut("shortcuts.moveWindowPrev", "Meta+Shift+Left")
    readonly property string moveWindowNextBind: prefShortcut("shortcuts.moveWindowNext", "Meta+Shift+Right")

    function triggerFullscreenQuickSave(sourceTag) {
        if (!shellRoot) {
            return
        }
        ScreenshotSaveController.beginRequest(shellRoot, String(sourceTag || "shortcut-print"))
        var router = shellRoot.appCommandRouterRef
        if (!router || !router.routeWithResult) {
            ScreenshotController.showResultNotification(shellRoot, false, "", "router-unavailable")
            return
        }
        var result = router.routeWithResult("screenshot.fullscreen", {}, String(sourceTag || "shortcut-print"))
        var payload = result && result.payload ? result.payload : {}
        ScreenshotController.showResultNotification(shellRoot,
                                                    !!(result && result.ok),
                                                    String(payload.path || ""),
                                                    String(payload.error || result.error || ""))
    }

    function openScreenshotOptionsOverlay() {
        if (topBarWindowRef && topBarWindowRef.openScreenshotPopup) {
            topBarWindowRef.openScreenshotPopup()
        }
    }

    function toggleWorkspaceOverview() {
        if (!desktopSceneRef) {
            return
        }
        if (desktopSceneRef.toggleWorkspaceOverview) {
            desktopSceneRef.toggleWorkspaceOverview()
        } else {
            desktopSceneRef.workspaceVisible = !desktopSceneRef.workspaceVisible
        }
    }

    Shortcut {
        sequence: root.screenshotQuickBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: root.triggerFullscreenQuickSave("shortcut-print")
    }

    Shortcut {
        sequence: root.screenshotToolBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: root.openScreenshotOptionsOverlay()
    }

    Shortcut {
        sequence: root.screenshotAreaBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: {
            if (!shellRoot) {
                return
            }
            ScreenshotSaveController.beginRequest(shellRoot, "shortcut-area-bind")
            ScreenshotController.beginAreaSelection(shellRoot)
        }
    }

    Shortcut {
        sequence: root.screenshotFullscreenBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: root.triggerFullscreenQuickSave("shortcut-fullscreen-bind")
    }

    Shortcut {
        sequence: root.workspacePanelBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: {
            if (typeof ShellInputRouter !== "undefined"
                    && !ShellInputRouter.canDispatch("shell.workspace_overview")) return
            root.toggleWorkspaceOverview()
        }
    }

    Shortcut {
        sequence: root.workspaceOverviewBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: {
            if (typeof ShellInputRouter !== "undefined"
                    && !ShellInputRouter.canDispatch("shell.workspace_overview")) return
            root.toggleWorkspaceOverview()
        }
    }

    Shortcut {
        sequence: root.workspacePrevBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("workspace.prev")) return
            if (desktopSceneRef && desktopSceneRef.switchWorkspaceByDelta) {
                desktopSceneRef.switchWorkspaceByDelta(-1)
            }
        }
    }

    Shortcut {
        sequence: root.workspaceNextBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("workspace.next")) return
            if (desktopSceneRef && desktopSceneRef.switchWorkspaceByDelta) {
                desktopSceneRef.switchWorkspaceByDelta(1)
            }
        }
    }

    Shortcut {
        sequence: root.moveWindowPrevBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: {
            if (typeof ShellInputRouter !== "undefined"
                    && !ShellInputRouter.canDispatch("window.move_workspace_prev")) return
            if (desktopSceneRef && desktopSceneRef.moveFocusedWindowByDelta) {
                desktopSceneRef.moveFocusedWindowByDelta(-1)
            }
        }
    }

    Shortcut {
        sequence: root.moveWindowNextBind
        context: Qt.ApplicationShortcut
        enabled: String(sequence || "").length > 0
        onActivated: {
            if (typeof ShellInputRouter !== "undefined"
                    && !ShellInputRouter.canDispatch("window.move_workspace_next")) return
            if (desktopSceneRef && desktopSceneRef.moveFocusedWindowByDelta) {
                desktopSceneRef.moveFocusedWindowByDelta(1)
            }
        }
    }

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onKeyboardShortcutChanged(path) {
            if (String(path || "").length > 0) {
                root._shortcutRev = root._shortcutRev + 1
            }
        }
    }
}
