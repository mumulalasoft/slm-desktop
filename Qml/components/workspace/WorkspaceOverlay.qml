import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root
    anchors.fill: parent

    property bool active: false
    readonly property int smoothMotionDuration: Theme.durationWorkspace
    property real transitionProgress: active ? 1.0 : 0.0
    property int lastAnimatedSpace: (typeof SpacesManager !== "undefined" && SpacesManager)
                                    ? Number(SpacesManager.activeSpace || 1) : 1
    property real workspaceSwitchOffset: 0
    property var activeWindows: []
    property bool dragInProgress: false
    property real dragCenterX: -1
    property real dragCenterY: -1
    property int dragEdgeDirection: 0 // -1: left, +1: right
    property bool dragEdgeSwitchArmed: false
    property int dragEdgeDwellMs: {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return 350
        }
        var v = Number(UIPreferences.getPreference("workspace.dragEdgeDwellMs", 350))
        return Math.max(120, Math.min(1200, Math.round(v)))
    }
    property int dragEdgeRepeatMs: {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return 260
        }
        var v = Number(UIPreferences.getPreference("workspace.dragEdgeRepeatMs", 260))
        return Math.max(80, Math.min(700, Math.round(v)))
    }
    signal dismissed()

    Behavior on transitionProgress {
        NumberAnimation { duration: root.smoothMotionDuration; easing.type: Theme.easingStandard }
    }

    Behavior on workspaceSwitchOffset {
        NumberAnimation { duration: root.smoothMotionDuration; easing.type: Theme.easingDecelerate }
    }

    function isShellWindow(appId, title) {
        var app = String(appId || "").toLowerCase()
        var ttl = String(title || "").toLowerCase()
        var shellApps = [
            "appdesktop_shell",
            "desktop_shell",
            "desktopshell"
        ]
        for (var i = 0; i < shellApps.length; ++i) {
            if (app === shellApps[i]) {
                return true
            }
        }
        if (app.indexOf("desktop_shell") >= 0) {
            return true
        }
        // Canonical window-title aliases for shell surfaces.
        // Keep legacy "overview" entry for backward compatibility.
        var shellTitles = [
            "desktop shell",
            "desktop topbar",
            "desktop dock",
            "desktop workspace",
            "desktop overview",
            "desktop launchpad"
        ]
        for (var j = 0; j < shellTitles.length; ++j) {
            if (ttl === shellTitles[j]) {
                return true
            }
        }
        return false
    }

    function windowIconSource(w) {
        if (!w) {
            return ""
        }
        var iconName = String(w.iconName || "")
        if (iconName.length > 0) {
            return "image://themeicon/" + iconName + "?v=" +
                    ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                     ? ThemeIconController.revision : 0)
        }
        var iconSource = String(w.iconSource || "")
        if (iconSource.length > 0) {
            return iconSource
        }
        return "qrc:/icons/logo.svg"
    }

    function focusWindowById(viewId) {
        var id = String(viewId || "")
        if (!id.length) {
            return
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.PresentView) {
            WorkspaceManager.PresentView(id)
        } else if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            WindowingBackend.sendCommand("focus-view " + id)
        }
        root.dismissed()
    }

    function closeWindowById(viewId) {
        var id = String(viewId || "")
        if (!id.length) {
            return
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.CloseView) {
            WorkspaceManager.CloseView(id)
        } else if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            WindowingBackend.sendCommand("close-view " + id)
        }
    }

    function moveWindowToWorkspaceId(viewId, workspaceId) {
        var id = String(viewId || "")
        var target = Number(workspaceId || 0)
        if (!id.length || target <= 0) {
            return false
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.MoveWindowToWorkspace) {
            var ok = !!WorkspaceManager.MoveWindowToWorkspace(id, target)
            if (ok && typeof SpacesManager !== "undefined" && SpacesManager &&
                    SpacesManager.enforceInvariants) {
                SpacesManager.enforceInvariants()
            }
            return ok
        }
        if (typeof SpacesManager !== "undefined" && SpacesManager &&
                SpacesManager.assignWindowToSpace) {
            SpacesManager.assignWindowToSpace(id, target)
            if (SpacesManager.enforceInvariants) {
                SpacesManager.enforceInvariants()
            }
            return true
        }
        return false
    }

    function windowWorkspaceId(viewId) {
        var id = String(viewId || "")
        if (!id.length || typeof SpacesManager === "undefined" || !SpacesManager ||
                !SpacesManager.windowSpace) {
            return 0
        }
        return Number(SpacesManager.windowSpace(id) || 0)
    }

    function beginWindowDrag(viewId, centerPoint) {
        dragInProgress = true
        dragCenterX = Number(centerPoint ? centerPoint.x : -1)
        dragCenterY = Number(centerPoint ? centerPoint.y : -1)
        updateDragEdgeState()
    }

    function updateWindowDrag(centerPoint) {
        if (!dragInProgress) {
            return
        }
        dragCenterX = Number(centerPoint ? centerPoint.x : dragCenterX)
        dragCenterY = Number(centerPoint ? centerPoint.y : dragCenterY)
        updateDragEdgeState()
    }

    function endWindowDrag() {
        dragInProgress = false
        dragCenterX = -1
        dragCenterY = -1
        dragEdgeDirection = 0
        dragEdgeSwitchArmed = false
        autoSwitchLeftDelayTimer.stop()
        autoSwitchRightDelayTimer.stop()
        autoSwitchLeftTimer.stop()
        autoSwitchRightTimer.stop()
    }

    function updateDragEdgeState() {
        if (!dragInProgress || !workspaceBar) {
            dragEdgeDirection = 0
            dragEdgeSwitchArmed = false
            autoSwitchLeftDelayTimer.stop()
            autoSwitchRightDelayTimer.stop()
            autoSwitchLeftTimer.stop()
            autoSwitchRightTimer.stop()
            return
        }

        var barTopLeft = workspaceBar.mapToItem(root, 0, 0)
        var barBottomY = barTopLeft.y + workspaceBar.height
        var barLeftX = barTopLeft.x
        var barRightX = barTopLeft.x + workspaceBar.width
        var edgeThreshold = 52
        var inVerticalBand = dragCenterY >= (barTopLeft.y - 22) &&
                             dragCenterY <= (barBottomY + 22)

        var nearLeft = inVerticalBand &&
                       dragCenterX >= (barLeftX - 8) &&
                       dragCenterX <= (barLeftX + edgeThreshold)
        var nearRight = inVerticalBand &&
                        dragCenterX <= (barRightX + 8) &&
                        dragCenterX >= (barRightX - edgeThreshold)

        if (nearLeft) {
            dragEdgeDirection = -1
            if (!autoSwitchLeftTimer.running && !autoSwitchLeftDelayTimer.running) {
                dragEdgeSwitchArmed = true
                autoSwitchRightTimer.stop()
                autoSwitchRightDelayTimer.stop()
                autoSwitchLeftDelayTimer.start()
            }
            return
        }
        if (nearRight) {
            dragEdgeDirection = 1
            if (!autoSwitchRightTimer.running && !autoSwitchRightDelayTimer.running) {
                dragEdgeSwitchArmed = true
                autoSwitchLeftTimer.stop()
                autoSwitchLeftDelayTimer.stop()
                autoSwitchRightDelayTimer.start()
            }
            return
        }

        dragEdgeDirection = 0
        dragEdgeSwitchArmed = false
        autoSwitchLeftDelayTimer.stop()
        autoSwitchRightDelayTimer.stop()
        autoSwitchLeftTimer.stop()
        autoSwitchRightTimer.stop()
    }

    function nudgeWorkspace(delta) {
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.SwitchWorkspaceByDelta) {
            WorkspaceManager.SwitchWorkspaceByDelta(delta)
            return
        }
        if (typeof SpacesManager === "undefined" || !SpacesManager) {
            return
        }
        var activeSpace = Number(SpacesManager.activeSpace || 1)
        var count = Number(SpacesManager.spaceCount || 1)
        var target = Math.max(1, Math.min(count, activeSpace + delta))
        if (target === activeSpace) {
            return
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.SwitchWorkspace) {
            WorkspaceManager.SwitchWorkspace(target)
        } else if (SpacesManager.setActiveSpace) {
            SpacesManager.setActiveSpace(target)
        }
    }

    function addWorkspace() {
        if (typeof SpacesManager === "undefined" || !SpacesManager || !SpacesManager.spaceCount) {
            return
        }
        var current = Number(SpacesManager.spaceCount || 1)
        var next = Math.max(1, Math.min(12, current + 1))
        if (next === current) {
            return
        }
        if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            WindowingBackend.sendCommand("space count " + next)
        }
        if (SpacesManager.setSpaceCount) {
            SpacesManager.setSpaceCount(next)
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.SwitchWorkspace) {
            WorkspaceManager.SwitchWorkspace(Math.min(next, Number(SpacesManager.activeSpace || 1)))
        }
    }

    function rebuildWorkspaceState() {
        var windows = []
        if (typeof CompositorStateModel === "undefined" || !CompositorStateModel ||
                !CompositorStateModel.windowCount || !CompositorStateModel.windowAt) {
            activeWindows = []
            return
        }
        var n = CompositorStateModel.windowCount()
        for (var i = 0; i < n; ++i) {
            var w = CompositorStateModel.windowAt(i)
            if (!w || isShellWindow(w.appId, w.title) || !!w.minimized || !w.mapped) {
                continue
            }
            if (typeof SpacesManager !== "undefined" && SpacesManager &&
                    SpacesManager.windowBelongsToActive &&
                    !SpacesManager.windowBelongsToActive(String(w.viewId || ""))) {
                continue
            }
            windows.push({
                             "viewId": String(w.viewId || ""),
                             "title": String(w.title || ""),
                             "appId": String(w.appId || ""),
                             "iconSource": String(w.iconSource || ""),
                             "iconName": String(w.iconName || ""),
                             "focused": !!w.focused,
                             "x": Number(w.x || 0),
                             "y": Number(w.y || 0),
                             "width": Math.max(60, Number(w.width || 60)),
                             "height": Math.max(40, Number(w.height || 40)),
                             "previewSource": (typeof WorkspacePreviewManager !== "undefined" &&
                                               WorkspacePreviewManager &&
                                               WorkspacePreviewManager.capturePreviewSource)
                                              ? WorkspacePreviewManager.capturePreviewSource(
                                                    String(w.viewId || ""),
                                                    Number(w.x || 0),
                                                    Number(w.y || 0),
                                                    Math.max(60, Number(w.width || 60)),
                                                    Math.max(40, Number(w.height || 40)))
                                              : ((typeof OverviewPreviewManager !== "undefined" &&
                                                  OverviewPreviewManager &&
                                                  OverviewPreviewManager.capturePreviewSource)
                                                 ? OverviewPreviewManager.capturePreviewSource(
                                                    String(w.viewId || ""),
                                                    Number(w.x || 0),
                                                    Number(w.y || 0),
                                                    Math.max(60, Number(w.width || 60)),
                                                    Math.max(40, Number(w.height || 40)))
                                                 : "")
                         })
        }
        activeWindows = windows
    }

    // Compatibility alias while older internal callsites are migrated.
    function rebuildOverviewState() {
        rebuildWorkspaceState()
    }

    visible: active || transitionProgress > 0.01
    opacity: transitionProgress
    z: 440

    Keys.onEscapePressed: root.dismissed()

    Rectangle {
        anchors.fill: parent
        color: Theme.color("workspaceBackdrop")
        opacity: Theme.opacityElevated * root.transitionProgress
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.dismissed()
    }

    WorkspaceStrip {
        id: workspaceBar
        width: Math.min(parent.width - 120, 720)
        height: 54
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 14 - Math.round((1.0 - root.transitionProgress) * 8)
        opacity: root.transitionProgress
        scale: 0.96 + (0.04 * root.transitionProgress)
        transformOrigin: Item.Top
        dragEnabled: true
        dragInProgress: root.dragInProgress
        workspaceModel: (typeof WorkspaceStripModel !== "undefined" && WorkspaceStripModel && WorkspaceStripModel.rows)
                        ? WorkspaceStripModel.rows
                        : ((typeof SpacesManager !== "undefined" && SpacesManager)
                           ? SpacesManager.workspaceModel
                        : [{ "id": 1, "index": 1, "isActive": true, "isEmpty": true, "isTrailingEmpty": true, "windowCount": 0 }]
                          )
        onWorkspaceActivated: function(spaceNo) {
            if (typeof WorkspaceStripModel !== "undefined" && WorkspaceStripModel &&
                    WorkspaceStripModel.switchToWorkspace) {
                WorkspaceStripModel.switchToWorkspace(spaceNo)
                return
            }
            if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                    WorkspaceManager.SwitchWorkspace) {
                WorkspaceManager.SwitchWorkspace(spaceNo)
            } else if (typeof SpacesManager !== "undefined" && SpacesManager &&
                       SpacesManager.setActiveSpace) {
                SpacesManager.setActiveSpace(spaceNo)
            }
        }
        onWindowDroppedToWorkspace: function(viewId, spaceNo) {
            // Edge case: drop back to the same workspace should be a no-op and
            // must not churn assignments or trigger unnecessary rebuilds.
            if (root.windowWorkspaceId(viewId) === Number(spaceNo)) {
                root.endWindowDrag()
                return
            }
            // Drop on trailing placeholder is intentionally handled by backend
            // invariant logic: target becomes occupied then a new empty trailing
            // workspace is appended; empty non-last source is compacted away.
            if (!root.moveWindowToWorkspaceId(viewId, spaceNo)) {
                return
            }
            if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                    WorkspaceManager.SwitchWorkspace) {
                WorkspaceManager.SwitchWorkspace(spaceNo)
            } else if (typeof SpacesManager !== "undefined" && SpacesManager &&
                       SpacesManager.setActiveSpace) {
                SpacesManager.setActiveSpace(spaceNo)
            }
            root.endWindowDrag()
        }
    }

    Rectangle {
        id: edgeHintLeft
        width: 24
        anchors.left: workspaceBar.left
        anchors.top: workspaceBar.top
        anchors.bottom: workspaceBar.bottom
        radius: Theme.radiusControlLarge
        color: Theme.color("accent")
        opacity: root.dragInProgress && root.dragEdgeDirection < 0 ? 0.22 : 0.0
        Behavior on opacity { NumberAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDecelerate } }
    }

    Rectangle {
        id: edgeHintRight
        width: 24
        anchors.right: workspaceBar.right
        anchors.top: workspaceBar.top
        anchors.bottom: workspaceBar.bottom
        radius: Theme.radiusControlLarge
        color: Theme.color("accent")
        opacity: root.dragInProgress && root.dragEdgeDirection > 0 ? 0.22 : 0.0
        Behavior on opacity { NumberAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDecelerate } }
    }

    WorkspaceOverviewScene {
        id: windowOverlayLayer
        z: 5
        active: root.active
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: workspaceBar.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 38
        anchors.rightMargin: 38
        anchors.topMargin: 22
        anchors.bottomMargin: 46
        x: root.workspaceSwitchOffset
        opacity: root.transitionProgress
        scale: 0.99 + (0.01 * root.transitionProgress)
        windows: root.activeWindows
        dragMapTarget: root
        iconResolver: root.windowIconSource
        onWindowActivated: function(viewId) { root.focusWindowById(viewId) }
        onWindowCloseRequested: function(viewId) { root.closeWindowById(viewId) }
        onWindowDragStarted: function(viewId, center) { root.beginWindowDrag(viewId, center) }
        onWindowDragMoved: function(center) { root.updateWindowDrag(center) }
        onWindowDragEnded: root.endWindowDrag()
    }

    Timer {
        id: autoSwitchLeftDelayTimer
        interval: root.dragEdgeDwellMs
        repeat: false
        onTriggered: {
            if (!root.dragInProgress || root.dragEdgeDirection >= 0 || !root.dragEdgeSwitchArmed) {
                return
            }
            root.nudgeWorkspace(-1)
            autoSwitchLeftTimer.start()
        }
    }

    Timer {
        id: autoSwitchRightDelayTimer
        interval: root.dragEdgeDwellMs
        repeat: false
        onTriggered: {
            if (!root.dragInProgress || root.dragEdgeDirection <= 0 || !root.dragEdgeSwitchArmed) {
                return
            }
            root.nudgeWorkspace(1)
            autoSwitchRightTimer.start()
        }
    }

    Timer {
        id: autoSwitchLeftTimer
        interval: root.dragEdgeRepeatMs
        repeat: true
        onTriggered: root.nudgeWorkspace(-1)
    }

    Timer {
        id: autoSwitchRightTimer
        interval: root.dragEdgeRepeatMs
        repeat: true
        onTriggered: root.nudgeWorkspace(1)
    }

    Timer {
        id: workspaceSwitchOffsetReset
        interval: 34
        repeat: false
        onTriggered: root.workspaceSwitchOffset = 0
    }

    onActiveChanged: {
        if (active) {
            rebuildWorkspaceState()
            forceActiveFocus()
        }
    }

    Connections {
        target: CompositorStateModel
        function onLastEventChanged() { root.rebuildWorkspaceState() }
        function onConnectedChanged() { root.rebuildWorkspaceState() }
    }

    Connections {
        target: (typeof SpacesManager !== "undefined") ? SpacesManager : null
        function onActiveSpaceChanged() {
            root.rebuildWorkspaceState()
            if (!root.active) {
                root.lastAnimatedSpace = Number(SpacesManager.activeSpace || root.lastAnimatedSpace || 1)
                return
            }
            var nextSpace = Number(SpacesManager.activeSpace || 1)
            var prevSpace = Number(root.lastAnimatedSpace || nextSpace)
            root.lastAnimatedSpace = nextSpace
            var direction = nextSpace > prevSpace ? 1 : (nextSpace < prevSpace ? -1 : 0)
            if (direction !== 0) {
                root.workspaceSwitchOffset = direction > 0 ? 28 : -28
                workspaceSwitchOffsetReset.restart()
            }
        }
        function onAssignmentsChanged() { root.rebuildWorkspaceState() }
        function onWorkspacesChanged() { root.rebuildWorkspaceState() }
    }
}
