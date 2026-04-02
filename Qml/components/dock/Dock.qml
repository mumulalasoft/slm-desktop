import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "."

Rectangle {
    id: root

    property var appsModel: []
    property real hoverX: width * 0.5
    property bool dockHovered: false
    readonly property bool hovered: dockHovered
    // Icon slot width driven by UIPreferences.dockIconSize: small=48, medium=58, large=72
    readonly property real iconSlotWidth: {
        const sz = (typeof UIPreferences !== "undefined" && UIPreferences
                    && UIPreferences.dockIconSize !== undefined)
                   ? UIPreferences.dockIconSize : "medium"
        if (sz === "small") return 48
        if (sz === "large") return 72
        return 58
    }
    readonly property bool magnificationEnabled: (typeof UIPreferences !== "undefined"
                                                  && UIPreferences
                                                  && UIPreferences.dockMagnificationEnabled !== undefined)
                                                 ? UIPreferences.dockMagnificationEnabled : true
    property real amplitude: magnificationEnabled ? 10 : 0
    property real sigma: 148
    property real hoverLift: 6
    property real glowWidth: 170
    readonly property real baseWidth: Math.max(392, (appsRepeater.count + 1) * 64 + 8)
    readonly property real contentVPadding: 3
    readonly property real baseHeight: 72 + (contentVPadding * 2)
    property string activeAppName: ""
    property alias draggingFromIndex: reorderState.draggingFromIndex
    property alias draggingToIndex: reorderState.draggingToIndex
    property alias draggingDeltaX: reorderState.draggingDeltaX
    property alias dragStartMarkerCenterX: reorderState.dragStartMarkerCenterX
    property alias reorderMarkerX: reorderState.reorderMarkerX
    property alias dragPreviewIconPath: reorderState.dragPreviewIconPath
    property bool externalDropActive: false
    property int externalDropIndex: -1
    property string externalDropIconPath: ""
    property int dropPulseIndex: -1
    property string motionPreset: (typeof UIPreferences !== "undefined"
                                   && UIPreferences
                                   && UIPreferences.dockMotionPreset !== undefined)
                                  ? UIPreferences.dockMotionPreset : "macos-lively" // "subtle" | "macos-lively"
    property bool dropPulseEnabled: (typeof UIPreferences !== "undefined"
                                     && UIPreferences
                                     && UIPreferences.dockDropPulseEnabled !== undefined)
                                    ? UIPreferences.dockDropPulseEnabled : true
    property int dragThresholdMousePx: (typeof UIPreferences !== "undefined"
                                        && UIPreferences
                                        && UIPreferences.dockDragThresholdMouse !== undefined)
                                       ? UIPreferences.dockDragThresholdMouse : 6
    property int dragThresholdTouchpadPx: (typeof UIPreferences !== "undefined"
                                           && UIPreferences
                                           && UIPreferences.dockDragThresholdTouchpad !== undefined)
                                          ? UIPreferences.dockDragThresholdTouchpad : 3
    property var separatorAfterDesktopFiles: []
    readonly property bool livelyMotion: motionPreset === "macos-lively" || motionPreset === "expressive"
    readonly property real gapWidthExtra: livelyMotion ? 32 : 24
    readonly property real gapSpring: livelyMotion ? 3.6 : 4.8
    readonly property real gapDamping: livelyMotion ? 0.33 : 0.43
    readonly property real gapMass: livelyMotion ? 0.64 : 0.55
    readonly property real dragSourceOpacity: livelyMotion ? 0.24 : 0.36
    readonly property real dropPulseSize: livelyMotion ? 56 : 48
    readonly property real dropPulsePeakOpacity: livelyMotion ? 0.88 : 0.72
    readonly property real dropPulseStartScale: livelyMotion ? 0.72 : 0.78
    readonly property real dropPulseEndScale: livelyMotion ? 1.28 : 1.16
    readonly property int dropPulseInDuration: livelyMotion ? 90 : 80
    readonly property int dropPulseOutDuration: livelyMotion ? 230 : 190
    readonly property int dropPulseFadeDuration: livelyMotion ? 210 : 170
    signal appActivated(string appName)
    signal launchpadRequested()

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    width: Math.max(baseWidth, dockRow.width + 12)
    height: baseHeight + amplitude + hoverLift + 8
    radius: Theme.radiusWindow
    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"
    clip: false

    DockReorderController {
        id: reorderState
    }

    function gaussianInfluence(iconCenterX, mouseX, amplitude, sigma) {
        var d = iconCenterX - mouseX
        return amplitude * Math.exp(-(d * d) / (2 * sigma * sigma))
    }

    function nearestPinnedIndex(globalX) {
        var localX = Number(globalX || 0)
        if (root.window) {
            localX -= Number(root.window.x || 0)
        }
        var nearest = -1
        var nearestDist = Number.MAX_VALUE
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (!item || !item.dragEnabled) {
                continue
            }
            var center = dockRow.x + item.x + item.width * 0.5
            var dist = Math.abs(center - localX)
            if (dist < nearestDist) {
                nearestDist = dist
                nearest = i
            }
        }
        return nearest
    }

    function maxPinnedModelIndex() {
        var maxIdx = -1
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (item && item.dragEnabled) {
                maxIdx = i
            }
        }
        return maxIdx
    }

    function pinnedCount() {
        var count = 0
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (item && item.dragEnabled) {
                count++
            }
        }
        return count
    }

    function pinnedModelIndexAtPinnedPos(pinnedPos) {
        var pos = 0
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (!item || !item.dragEnabled) {
                continue
            }
            if (pos === pinnedPos) {
                return i
            }
            pos++
        }
        return -1
    }

    function insertionPinnedPosFromGlobalX(globalX) {
        var localX = Number(globalX || 0)
        if (root.window) {
            localX -= Number(root.window.x || 0)
        }
        var centers = []
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (!item || !item.dragEnabled) {
                continue
            }
            centers.push(dockRow.x + item.x + item.width * 0.5)
        }
        return reorderState.insertionPinnedPosFromLocalX(localX, centers)
    }

    function clearExternalDrop() {
        externalDropActive = false
        externalDropIndex = -1
        externalDropIconPath = ""
    }

    function updateExternalDrop(active, globalX, iconPath) {
        if (!active) {
            clearExternalDrop()
            return
        }
        externalDropActive = true
        externalDropIconPath = iconPath || ""
        var pinnedPos = insertionPinnedPosFromGlobalX(globalX)
        if (pinnedPos < 0) {
            externalDropIndex = -1
            return
        }
        if (pinnedPos >= pinnedCount()) {
            externalDropIndex = maxPinnedModelIndex()
        } else {
            externalDropIndex = pinnedModelIndexAtPinnedPos(pinnedPos)
        }
    }

    function commitExternalDrop(desktopFile, globalX, iconPath) {
        var pinnedPos = insertionPinnedPosFromGlobalX(globalX)
        clearExternalDrop()
        if (!desktopFile || desktopFile.length === 0) {
            return false
        }
        var ok = false
        if (DockModel.addDesktopEntryAt) {
            ok = DockModel.addDesktopEntryAt(desktopFile, pinnedPos)
        } else {
            ok = DockModel.addDesktopEntry(desktopFile)
        }
        if (ok) {
            var targetPinnedPos = Math.max(0, pinnedPos)
            var modelIdx = pinnedModelIndexAtPinnedPos(targetPinnedPos)
            if (modelIdx < 0) {
                modelIdx = maxPinnedModelIndex()
            }
            triggerDropPulseForIndex(modelIdx)
        }
        return ok
    }

    function triggerDropPulseForIndex(modelIdx) {
        if (!dropPulseEnabled || modelIdx < 0) {
            return
        }
        dropPulseIndex = modelIdx
        dropPulseAnim.restart()
    }

    function beginReorderDrag(modelIndex, sourceCenter, iconPath) {
        reorderState.startDrag(modelIndex, sourceCenter, iconPath, reorderMarker.width)
    }

    function moveReorderDrag(deltaX) {
        reorderState.moveDrag(deltaX, maxPinnedModelIndex(), pinnedCenters(), reorderMarker.width)
    }

    function finishReorderDrag(deltaX) {
        var result = reorderState.finishDrag(deltaX, maxPinnedModelIndex())
        if (!result.valid) {
            return
        }
        if (result.target >= 0 && result.target !== result.fromIndex) {
            triggerDropPulseForIndex(result.target)
            Qt.callLater(function() {
                DockModel.movePinnedEntry(result.fromIndex, result.target)
            })
        }
    }

    function targetIndexFromDelta(fromIndex, deltaX) {
        var maxPinned = maxPinnedModelIndex()
        return reorderState.targetIndexFromDelta(fromIndex, deltaX, maxPinned)
    }

    function pinnedCenters() {
        var centers = []
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (!item || !item.dragEnabled) {
                continue
            }
            centers.push(dockRow.x + item.x + item.width * 0.5)
        }
        return centers
    }

    function markerCenterX() {
        return reorderState.markerCenterX(pinnedCenters())
    }

    function markerXFromCenter(centerX) {
        return reorderState.markerXFromCenter(centerX, reorderMarker.width)
    }

    function pulseCenterX() {
        if (dropPulseIndex < 0) {
            return -1000
        }
        var item = appsRepeater.itemAt(dropPulseIndex)
        if (!item) {
            return -1000
        }
        return dockRow.x + item.x + item.width * 0.5
    }

    function _norm(s) {
        if (s === undefined || s === null) {
            return ""
        }
        return ("" + s).toLowerCase()
    }

    function _desktopBase(desktopFile) {
        var s = _norm(desktopFile)
        if (s.length === 0) {
            return ""
        }
        var slash = s.lastIndexOf("/")
        if (slash >= 0 && slash + 1 < s.length) {
            s = s.slice(slash + 1)
        }
        if (s.endsWith(".desktop")) {
            s = s.slice(0, s.length - 8)
        }
        return s
    }

    function _escapeRe(s) {
        return ("" + s).replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
    }

    function _isWeakIdentity(token) {
        token = _norm(token)
        if (token.length < 4) {
            return true
        }
        return token === "code" ||
               token === "app" ||
               token === "application" ||
               token === "desktop" ||
               token === "editor" ||
               token === "browser" ||
               token === "flatpak" ||
               token === "snap"
    }

    function _hasIdToken(appId, token) {
        appId = _norm(appId)
        token = _norm(token)
        if (appId.length === 0 || token.length === 0) {
            return false
        }
        if (appId === token) {
            return true
        }
        var re = new RegExp("(^|[._-])" + _escapeRe(token) + "([._-]|$)")
        return re.test(appId)
    }

    function _activeWorkspaceId() {
        if (typeof SpacesManager !== "undefined" && SpacesManager && SpacesManager.activeSpace) {
            return Math.max(1, Number(SpacesManager.activeSpace || 1))
        }
        if (typeof CompositorStateModel !== "undefined" && CompositorStateModel && CompositorStateModel.activeSpace) {
            return Math.max(1, Number(CompositorStateModel.activeSpace || 1))
        }
        return 1
    }

    // Trigger a dock-item bounce animation when a window lifecycle event fires.
    // Called by DesktopScene when the compositor reports window-opened / window-minimized.
    function notifyWindowLifecycle(eventName, appId) {
        if (!appId || appId.length === 0 || !Theme.animationsEnabled) {
            return
        }
        var ev = String(eventName || "").toLowerCase()
        var isOpen = (ev === "window-opened" || ev === "window-shown" ||
                      ev === "window-unminimized" || ev === "window-created")
        var isMinimize = (ev === "window-minimized")
        if (!isOpen && !isMinimize) {
            return
        }
        var mode = isOpen ? "launch" : "focus"
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (item && item.matchesWindowAppId && item.matchesWindowAppId(appId)) {
                item.playBounce(mode)
                return
            }
        }
    }

    function _focusOrLaunchEntry(state, entry) {
        var s = state || {}
        var focusedViewId = String(s.preferredViewId || "")
        if (focusedViewId.length > 0 &&
                typeof WorkspaceManager !== "undefined" &&
                WorkspaceManager &&
                WorkspaceManager.PresentView) {
            WorkspaceManager.PresentView(focusedViewId)
            return "focus"
        }
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter &&
                AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopEntry", {
                                       desktopFile: String(entry.desktopFile || ""),
                                       executable: String(entry.executable || ""),
                                       name: String(entry.name || ""),
                                       iconName: String(entry.iconName || ""),
                                       iconSource: String(entry.iconSource || "")
                                   }, "dock")
            return "launch"
        }
        if (typeof AppExecutionGate !== "undefined" && AppExecutionGate &&
                AppExecutionGate.launchDesktopEntry) {
            AppExecutionGate.launchDesktopEntry(String(entry.desktopFile || ""),
                                                String(entry.executable || ""),
                                                String(entry.name || ""),
                                                String(entry.iconName || ""),
                                                String(entry.iconSource || ""),
                                                "dock")
            return "launch"
        }
        console.warn("Dock launch ignored: no router/gate available")
        return "launch"
    }

    function compositorStateForEntry(entryName, entryDesktopFile, entryExec) {
        if (typeof CompositorStateModel === "undefined" || !CompositorStateModel) {
            return {
                running: false,
                focused: false,
                hasActiveWorkspaceWindows: false,
                hasOtherWorkspaceWindows: false,
                hasWindowsInMultipleWorkspaces: false,
                activeWorkspaceCount: 0,
                otherWorkspaceCount: 0,
                preferredViewId: ""
            }
        }

        // Make QML binding reactive to IPC updates.
        var _tick = CompositorStateModel.lastEvent
        var _connected = CompositorStateModel.connected
        if (!_connected) {
            return { running: false, focused: false }
        }

        var name = _norm(entryName)
        var desktopBase = _desktopBase(entryDesktopFile)
        var exec = _norm(entryExec)
        var activeSpace = _activeWorkspaceId()
        if (exec.indexOf("/") >= 0) {
            exec = exec.slice(exec.lastIndexOf("/") + 1)
        }

        var running = false
        var focused = false
        var hasActive = false
        var hasOther = false
        var activeCount = 0
        var otherCount = 0
        var preferredInActive = ""
        var preferredInActiveAny = ""
        var preferredElsewhere = ""
        var focusedElsewhere = ""
        var workspaceSeen = {}
        var count = CompositorStateModel.windowCount ? CompositorStateModel.windowCount() : 0
        for (var i = 0; i < count; ++i) {
            var w = CompositorStateModel.windowAt(i)
            if (!w) {
                continue
            }
            var appId = _norm(w.appId)
            var match = false
            if (desktopBase.length > 0) {
                match = _hasIdToken(appId, desktopBase)
            } else if (exec.length > 0 && !_isWeakIdentity(exec)) {
                match = _hasIdToken(appId, exec)
            } else if (name.length > 0 && name.length >= 6 && !_isWeakIdentity(name)) {
                match = _hasIdToken(appId, name)
            }
            if (match) {
                running = true
                var winSpace = Math.max(1, Number(w.space || 1))
                workspaceSeen[String(winSpace)] = true
                var viewId = String(w.viewId || "")
                var isMapped = (w.mapped !== false)
                if (winSpace === activeSpace) {
                    hasActive = true
                    activeCount += 1
                    if (viewId.length > 0 && preferredInActiveAny.length === 0) {
                        preferredInActiveAny = viewId
                    }
                    if (viewId.length > 0 && preferredInActive.length === 0 && isMapped) {
                        preferredInActive = viewId
                    }
                } else {
                    hasOther = true
                    otherCount += 1
                    if (viewId.length > 0 && preferredElsewhere.length === 0 && isMapped) {
                        preferredElsewhere = viewId
                    }
                }
                if (w.focused === true) {
                    focused = true
                    if (winSpace !== activeSpace && viewId.length > 0) {
                        focusedElsewhere = viewId
                    }
                    if (winSpace === activeSpace && viewId.length > 0) {
                        preferredInActive = viewId
                    }
                }
            }
        }
        var preferredViewId = preferredInActive
        if (preferredViewId.length === 0) {
            preferredViewId = preferredInActiveAny
        }
        if (preferredViewId.length === 0 && focusedElsewhere.length > 0) {
            preferredViewId = focusedElsewhere
        }
        if (preferredViewId.length === 0) {
            preferredViewId = preferredElsewhere
        }
        var workspaceCount = 0
        for (var ws in workspaceSeen) {
            if (workspaceSeen[ws]) {
                workspaceCount += 1
            }
        }
        return {
            running: running,
            focused: focused,
            hasActiveWorkspaceWindows: hasActive,
            hasOtherWorkspaceWindows: hasOther,
            hasWindowsInMultipleWorkspaces: workspaceCount > 1,
            activeWorkspaceCount: activeCount,
            otherWorkspaceCount: otherCount,
            preferredViewId: preferredViewId
        }
    }

    Behavior on color {
        enabled: root.microAnimationAllowed()
        ColorAnimation {
            duration: Theme.transitionDuration
            easing.type: Theme.easingStandard
        }
    }
    Behavior on width {
        enabled: root.microAnimationAllowed()
        NumberAnimation {
            duration: Theme.durationMd
            easing.type: Theme.easingDecelerate
        }
    }
    Behavior on height {
        enabled: root.microAnimationAllowed()
        NumberAnimation {
            duration: Theme.durationMd
            easing.type: Theme.easingDecelerate
        }
    }
    Behavior on radius {
        enabled: root.microAnimationAllowed()
        NumberAnimation {
            duration: Theme.durationMd
            easing.type: Theme.easingDecelerate
        }
    }

    Row {
        id: dockRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.contentVPadding + 1

        spacing: 0

        DockItem {
            id: launchpadItem
            label: "Launchpad"
            iconPath: "qrc:/icons/launchpad.svg"
            baseSlotWidth: root.iconSlotWidth
            hoverIndicatorEnabled: true
            directHoverOverride: root.hovered
                                 && root.hoverX >= (dockRow.x + x)
                                 && root.hoverX <= (dockRow.x + x + width)
            showRunningDot: false
            hoverLift: root.hoverLift
            influence: root.hovered
                       ? root.gaussianInfluence(dockRow.x + x + width * 0.5,
                                                root.hoverX,
                                                root.amplitude,
                                                root.sigma)
                       : 0
            itemScale: 1.0
            onClicked: {
                launchpadItem.playBounce("focus")
                root.launchpadRequested()
            }
        }

        Repeater {
            id: appsRepeater
            model: root.appsModel

            delegate: DockAppDelegate {
                dockRoot: root
                modelCount: appsRepeater.count
                baseSlotWidth: root.iconSlotWidth
                hoverIndicatorEnabled: true
                directHoverOverride: root.hovered
                                     && root.hoverX >= (dockRow.x + x)
                                     && root.hoverX <= (dockRow.x + x + width)
                compositorState: root.compositorStateForEntry(name, desktopFile, executable)
                gapTarget: (root.draggingFromIndex >= 0
                            && root.draggingToIndex === index
                            && root.draggingToIndex !== root.draggingFromIndex)
                           || (root.externalDropActive && root.externalDropIndex === index)
                gapPreviewIconPath: root.externalDropActive ? root.externalDropIconPath : root.dragPreviewIconPath
                gapWidthExtra: root.gapWidthExtra
                gapSpring: root.gapSpring
                gapDamping: root.gapDamping
                gapMass: root.gapMass
                dragSourceOpacity: root.dragSourceOpacity
                dragThresholdMousePx: root.dragThresholdMousePx
                dragThresholdTouchpadPx: root.dragThresholdTouchpadPx
                separatorAfter: root.separatorAfterDesktopFiles.indexOf(desktopFile || "") >= 0
                hoverLift: root.hoverLift
                influence: root.hovered
                           ? root.gaussianInfluence(dockRow.x + x + width * 0.5,
                                                    root.hoverX,
                                                    root.amplitude,
                                                    root.sigma)
                           : 0
                itemScale: 1.0
            }
        }
    }

    Rectangle {
        width: root.glowWidth
        height: Math.max(22, root.height * 0.58)
        radius: height * 0.5
        x: Math.max(0, Math.min(root.width - width, root.hoverX - width * 0.5))
        y: Math.round((root.height - height) * 0.5)
        z: -2.5
        color: Theme.darkMode ? "#46cfe8ff" : "#55cde8ff"
        opacity: root.hovered ? 0.48 : 0.0

        Behavior on x {
            enabled: root.microAnimationAllowed()
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
        }
        Behavior on opacity {
            enabled: root.microAnimationAllowed()
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
        }
    }

    DockReorderMarker {
        id: reorderMarker
        active: root.draggingFromIndex >= 0 && root.draggingToIndex >= 0
        markerX: root.reorderMarkerX
        markerY: dockRow.y - 10
        draggingDeltaX: root.draggingDeltaX
    }

    DockDropPulse {
        id: dropPulse
        pulseSize: root.dropPulseSize
        startScale: root.dropPulseStartScale
        pulseCenterX: root.pulseCenterX()
        pulseY: dockRow.y + (livelyMotion ? 14 : 16)
    }

    SequentialAnimation {
        id: dropPulseAnim
        NumberAnimation {
            target: dropPulse
            property: "opacity"
            from: 0
            to: root.dropPulsePeakOpacity
            duration: root.dropPulseInDuration
            easing.type: Theme.easingDefault
        }
        ParallelAnimation {
            NumberAnimation {
                target: dropPulse
                property: "scale"
                from: root.dropPulseStartScale
                to: root.dropPulseEndScale
                duration: root.dropPulseOutDuration
                easing.type: Theme.easingLight
            }
            NumberAnimation {
                target: dropPulse
                property: "opacity"
                to: 0
                duration: root.dropPulseFadeDuration
                easing.type: Theme.easingLight
            }
        }
        ScriptAction {
            script: root.dropPulseIndex = -1
        }
        ScriptAction {
            script: dropPulse.scale = root.dropPulseStartScale
        }
    }

    HoverHandler {
        id: dockHoverHandler
        acceptedDevices: PointerDevice.Mouse
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
        onEntered: function(mouse) {
            root.dockHovered = true
            root.hoverX = mouse ? mouse.x : (width * 0.5)
        }
        onPositionChanged: function(mouse) {
            if (!mouse) {
                return
            }
            root.hoverX = mouse.x
        }
        onExited: {
            root.dockHovered = false
        }
    }
}
