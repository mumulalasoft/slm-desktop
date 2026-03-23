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
    gapSpring: 4.8
    gapDamping: 0.43
    gapMass: 0.55
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

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: {
            appContextMenu.popup()
        }
    }

    Menu {
        id: appContextMenu
        property var quickRows: []
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
