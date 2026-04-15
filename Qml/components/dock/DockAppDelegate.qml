import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

DockItem {
    id: appItem

    property var dockRoot
    property var compositorState: ({
                                       running: false,
                                       focused: false,
                                       hasActiveWorkspaceWindows: false,
                                       hasOtherWorkspaceWindows: false,
                                       hasWindowsInMultipleWorkspaces: false,
                                       activeWorkspaceCount: 0,
                                       otherWorkspaceCount: 0,
                                       preferredViewId: ""
                                   })
    property int modelCount: 0
    readonly property bool compositorRunning: compositorState && compositorState.running === true
    readonly property bool compositorFocused: compositorState && compositorState.focused === true
    readonly property bool hasActiveWorkspaceWindows: compositorState && compositorState.hasActiveWorkspaceWindows === true
    readonly property bool hasOtherWorkspaceWindows: compositorState && compositorState.hasOtherWorkspaceWindows === true
    readonly property bool hasWindowsInMultipleWorkspaces: compositorState && compositorState.hasWindowsInMultipleWorkspaces === true
    readonly property bool effectiveRunning: compositorRunning || isRunning
    readonly property bool pinnedEntry: (typeof isPinned !== "undefined")
                                        ? (isPinned === true)
                                        : ((typeof pinned !== "undefined") ? (pinned === true) : false)

    function _desktopBase(file) {
        var f = String(file || "").toLowerCase()
        if (f.length === 0) return ""
        if (f.indexOf("/") >= 0) {
            var parts = f.split("/")
            f = parts[parts.length - 1]
        }
        if (f.endsWith(".desktop")) {
            return f.slice(0, f.length - 8)
        }
        return f
    }

    function _canonicalIdentity() {
        var did = (typeof desktopId !== "undefined") ? String(desktopId || "") : ""
        var dfile = String(desktopFile || "")
        var exec = String(executable || "")
        var nm = String(name || "")
        if (typeof AppModel !== "undefined" && AppModel && AppModel.canonicalAppIdentity) {
            var appId = String(AppModel.canonicalAppIdentity(did, dfile, exec, nm) || "")
            if (appId.length > 0) {
                return appId
            }
        }
        var fallback = _desktopBase(did)
        if (fallback.length > 0) return fallback
        fallback = _desktopBase(dfile)
        if (fallback.length > 0) return fallback
        fallback = String(exec || "").toLowerCase()
        if (fallback.length > 0) return fallback
        return "unknown.app"
    }

    function _dockBadgeFromService() {
        if (typeof BadgeService === "undefined" || !BadgeService || !BadgeService.getBadge) {
            return 0
        }
        return Number(BadgeService.getBadge(_canonicalIdentity()) || 0)
    }

    function _dockBadgeFromNotifications() {
        if (typeof NotificationManager === "undefined" || !NotificationManager) {
            return 0
        }
        var canonical = _canonicalIdentity()
        if (NotificationManager.unreadCountForAppId) {
            return Number(NotificationManager.unreadCountForAppId(canonical) || 0)
        }
        return 0
    }

    // Badge source is global and canonical-only: app identity from AppModel.
    badgeCount: {
        var _badgeSink = (typeof BadgeService !== "undefined" && BadgeService) ? BadgeService._counts : null
        var _notifSink = (typeof NotificationManager !== "undefined" && NotificationManager)
                         ? Number(NotificationManager.unreadCount || 0) : 0
        return Math.max(_dockBadgeFromService(), _dockBadgeFromNotifications())
    }

    label: name
    showRunningDot: effectiveRunning
    focusedApp: compositorFocused
    hasOtherWorkspaceDot: !hasActiveWorkspaceWindows && hasOtherWorkspaceWindows
    showMixedWorkspaceDot: hasWindowsInMultipleWorkspaces
    dragEnabled: pinnedEntry
    dragSource: dockRoot && dockRoot.draggingFromIndex === index
    gapTarget: false
    gapPreviewIconPath: ""
    gapWidthExtra: 28
    gapSpring: Theme.physicsSpringDefault
    gapDamping: Theme.physicsDampingDefault
    gapMass: Theme.physicsMassDefault
    dragSourceOpacity: 0.36
    dragThresholdMousePx: 6
    dragThresholdTouchpadPx: 3
    iconPath: (iconName && iconName.length > 0)
              ? ("image://themeicon/" + iconName + "?v=" +
                 ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                  ? ThemeIconController.revision : 0))
              : ((iconSource && iconSource.length > 0) ? iconSource : "qrc:/icons/logo.svg")
    itemScale: 1.0

    onClicked: {
        var itemId = String(desktopFile || name || "")
        DockController.onActivate(itemId, dockRoot ? dockRoot.hostName : "")
        if (typeof CursorController !== "undefined" && CursorController && CursorController.startBusy) {
            CursorController.startBusy(1300)
        }
        var mode = "launch"
        if (dockRoot) {
            dockRoot.activeAppName = name
        }
        if (dockRoot && dockRoot._focusOrLaunchEntry) {
            mode = String(dockRoot._focusOrLaunchEntry(compositorState, {
                                                           desktopFile: desktopFile || "",
                                                           executable: executable || "",
                                                           name: name || "",
                                                           iconName: iconName || "",
                                                           iconSource: iconSource || ""
                                                       }))
        } else if (typeof AppCommandRouter !== "undefined" && AppCommandRouter &&
                   AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopEntry", {
                                       desktopFile: desktopFile || "",
                                       executable: executable || "",
                                       name: name || "",
                                       iconName: iconName || "",
                                       iconSource: iconSource || ""
                                   }, "dock")
        }
        appItem.playBounce(mode === "focus" ? "focus" : "launch")
        if (dockRoot) {
            dockRoot.appActivated(name)
        }
    }

    onDragStarted: function() {
        if (!pinnedEntry || !dockRoot) {
            return
        }
        DockController.onDragStart(String(desktopFile || name || index),
                                   dockRoot ? dockRoot.hostName : "")
        dockRoot.beginReorderDrag(index, dockRoot.rowX + appItem.x + appItem.width * 0.5,
                                  appItem.iconPath)
    }

    onDragMoved: function(deltaX) {
        if (!dockRoot || dockRoot.draggingFromIndex < 0) {
            return
        }
        DockController.onDragMove(deltaX, dockRoot ? dockRoot.hostName : "")
        dockRoot.moveReorderDrag(deltaX)
    }

    onDragFinished: function(deltaX) {
        if (!dockRoot || dockRoot.draggingFromIndex < 0) {
            return
        }
        DockController.onDragEnd(dockRoot ? dockRoot.hostName : "")
        dockRoot.finishReorderDrag(deltaX)
    }

    // Returns true if the given compositor appId matches this dock entry.
    // Uses the same identity tokens as Dock._hasIdToken so the logic is consistent.
    function matchesWindowAppId(windowAppId) {
        if (!dockRoot || !windowAppId || windowAppId.length === 0) {
            return false
        }
        var desktopBase = dockRoot._desktopBase(String(desktopFile || ""))
        var exec = String(executable || "").toLowerCase()
        var n = String(name || "").toLowerCase()
        if (desktopBase.length > 0) {
            if (dockRoot._hasIdToken(windowAppId, desktopBase)) return true
        }
        if (exec.length > 0 && !dockRoot._isWeakIdentity(exec)) {
            if (dockRoot._hasIdToken(windowAppId, exec)) return true
        }
        if (n.length > 0) {
            if (dockRoot._hasIdToken(windowAppId, n)) return true
        }
        return false
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: {
            DockController.onContextMenu(String(desktopFile || name || ""),
                                         dockRoot ? dockRoot.hostName : "")
            appContextMenu.popup()
        }
    }

    Menu {
        id: appContextMenu
        property var quickRows: []
        // Window list: [{viewId, label, focused}] for open windows of this app.
        property var windowRows: []
        onAboutToShow: {
            if (typeof AppModel !== "undefined" && AppModel
                    && AppModel.slmQuickActionsForEntry) {
                quickRows = AppModel.slmQuickActionsForEntry("dock", {
                    "desktopId": ((typeof desktopId !== "undefined") ? String(desktopId || "") : ""),
                    "desktopFile": String(desktopFile || ""),
                    "executable": String(executable || ""),
                    "iconName": String(iconName || ""),
                    "iconSource": String(iconSource || "")
                }) || []
            } else if (typeof AppModel !== "undefined" && AppModel && AppModel.slmQuickActions) {
                quickRows = AppModel.slmQuickActions("dock") || []
            } else {
                quickRows = []
            }

            // Collect open windows for this app from CompositorStateModel.
            var wins = []
            if (typeof CompositorStateModel !== "undefined" && CompositorStateModel
                    && CompositorStateModel.windowCount && CompositorStateModel.windowAt) {
                var count = CompositorStateModel.windowCount()
                var winIndex = 0
                for (var i = 0; i < count; ++i) {
                    var w = CompositorStateModel.windowAt(i)
                    if (!w) continue
                    if (appItem.matchesWindowAppId(String(w.appId || ""))) {
                        winIndex += 1
                        wins.push({
                            viewId: String(w.viewId || ""),
                            label: "Window " + winIndex,
                            focused: w.focused === true
                        })
                    }
                }
            }
            windowRows = wins
        }

        Instantiator {
            model: appContextMenu.quickRows
            delegate: MenuItem {
                property var rowData: (typeof modelData !== "undefined") ? modelData : ({})
                text: String((rowData && rowData.name) ? rowData.name : "")
                icon.name: String((rowData && rowData.icon) ? rowData.icon : "")
                enabled: text.length > 0
                onTriggered: {
                    var actionId = String((rowData && rowData.id) ? rowData.id : "")
                    if (actionId.length <= 0 || !AppModel || !AppModel.invokeSlmQuickAction) {
                        return
                    }
                    AppModel.invokeSlmQuickAction(actionId, {
                        "scope": "dock",
                        "selection_count": 0,
                        "source_app": "org.slm.dock",
                        "desktop_id": ((typeof desktopId !== "undefined") ? String(desktopId || "") : ""),
                        "desktop_file": String(desktopFile || ""),
                        "executable": String(executable || "")
                    })
                }
            }
            onObjectAdded: function(index, object) {
                appContextMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                appContextMenu.removeItem(object)
            }
        }

        // Window list — only shown when the app has ≥2 open windows.
        Instantiator {
            model: appContextMenu.windowRows.length >= 2 ? appContextMenu.windowRows : []
            delegate: MenuItem {
                property var winData: (typeof modelData !== "undefined") ? modelData : ({})
                text: String(winData.label || "")
                font.weight: winData.focused === true ? Theme.fontWeight("semibold") : Theme.fontWeight("medium")
                enabled: text.length > 0 && String(winData.viewId || "").length > 0
                onTriggered: {
                    var vid = String(winData.viewId || "")
                    if (vid.length === 0) return
                    if (typeof AppCommandRouter !== "undefined" && AppCommandRouter
                            && AppCommandRouter.routeWithResult) {
                        var focusRes = AppCommandRouter.routeWithResult("workspace.presentview",
                                                                        { "viewId": vid },
                                                                        "dock-context")
                        if (focusRes && focusRes.ok) {
                            return
                        }
                    }
                    if (typeof WorkspaceManager !== "undefined" && WorkspaceManager
                            && WorkspaceManager.PresentView) {
                        WorkspaceManager.PresentView(vid)
                    }
                }
            }
            onObjectAdded: function(index, object) {
                appContextMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                appContextMenu.removeItem(object)
            }
        }

        MenuSeparator {
            visible: appContextMenu.windowRows.length >= 2
        }

        MenuSeparator {
            visible: appContextMenu.quickRows.length > 0
        }

        MenuItem {
            text: "Pin to Dock"
            visible: !pinnedEntry
            enabled: desktopFile && desktopFile.length > 0
            onTriggered: {
                if (desktopFile && desktopFile.length > 0) {
                    DockModel.addDesktopEntry(desktopFile)
                }
            }
        }
        MenuItem {
            text: "Unpin"
            visible: pinnedEntry
            enabled: desktopFile && desktopFile.length > 0
            onTriggered: {
                if (desktopFile && desktopFile.length > 0) {
                    DockModel.removeDesktopEntry(desktopFile)
                }
            }
        }
        MenuItem {
            text: "Move Left"
            visible: pinnedEntry
            enabled: pinnedEntry && index >= 0 && index > 0
            onTriggered: DockModel.movePinnedEntry(index, index - 1)
        }
        MenuItem {
            text: "Move Right"
            visible: pinnedEntry
            enabled: pinnedEntry && index >= 0 && index < (modelCount - 1)
            onTriggered: DockModel.movePinnedEntry(index, index + 1)
        }
    }
}
