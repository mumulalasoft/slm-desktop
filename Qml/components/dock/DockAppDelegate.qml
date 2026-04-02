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

    // Bind badge count from BadgeService using the desktop ID or name as the key.
    badgeCount: {
        var _sink = (typeof BadgeService !== "undefined" && BadgeService) ? BadgeService._counts : null
        if (typeof BadgeService === "undefined" || !BadgeService) return 0
        var key = String(desktopId || desktopFile || name || "")
        if (key.length === 0) return 0
        return BadgeService.getBadge(key)
    }

    label: name
    showRunningDot: effectiveRunning
    focusedApp: compositorFocused
    hasOtherWorkspaceDot: !hasActiveWorkspaceWindows && hasOtherWorkspaceWindows
    showMixedWorkspaceDot: hasWindowsInMultipleWorkspaces
    dragEnabled: isPinned
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
        if (!isPinned || !dockRoot) {
            return
        }
        dockRoot.beginReorderDrag(index, dockRoot.dockRow.x + appItem.x + appItem.width * 0.5,
                                  appItem.iconPath)
    }

    onDragMoved: function(deltaX) {
        if (!dockRoot || dockRoot.draggingFromIndex < 0) {
            return
        }
        dockRoot.moveReorderDrag(deltaX)
    }

    onDragFinished: function(deltaX) {
        if (!dockRoot || dockRoot.draggingFromIndex < 0) {
            return
        }
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
                    "desktopId": String(desktopId || ""),
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
                        "desktop_id": String(desktopId || ""),
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
            visible: !isPinned
            enabled: desktopFile && desktopFile.length > 0
            onTriggered: {
                if (desktopFile && desktopFile.length > 0) {
                    DockModel.addDesktopEntry(desktopFile)
                }
            }
        }
        MenuItem {
            text: "Unpin"
            visible: isPinned
            enabled: desktopFile && desktopFile.length > 0
            onTriggered: {
                if (desktopFile && desktopFile.length > 0) {
                    DockModel.removeDesktopEntry(desktopFile)
                }
            }
        }
        MenuItem {
            text: "Move Left"
            visible: !!isPinned
            enabled: !!isPinned && index >= 0 && index > 0
            onTriggered: DockModel.movePinnedEntry(index, index - 1)
        }
        MenuItem {
            text: "Move Right"
            visible: !!isPinned
            enabled: !!isPinned && index >= 0 && index < (modelCount - 1)
            onTriggered: DockModel.movePinnedEntry(index, index + 1)
        }
    }
}
