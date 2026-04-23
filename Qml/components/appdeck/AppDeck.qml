import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Effects
import Slm_Desktop
import "."

// Branding definition:
// - AppDeck = user-facing name for the bottom app bar.
// - Internal keys/components keep "appdeck" naming to avoid contract breakage.
// - Use "AppDeck" in visible labels, docs, and settings text.

Rectangle {
    id: root

    property string hostName: "appdeck"
    property bool acceptsInput: true
    property bool rendererActive: true
    property bool hideBackgroundBorder: false
    property bool forceTransparentBackground: false
    property real layoutIconSlotWidth: -1
    property real layoutItemSpacing: -1
    property real layoutEdgePadding: -1
    property real layoutHoverLift: -1
    property real layoutGlowWidth: -1
    property real layoutMagnificationAmplitude: -1
    property real layoutMagnificationSigma: -1
    property real liftMax: 10
    property real influenceRadius: 140
    property int animationDuration: 150
    property var appsModel: []
    // hoverX and dockHovered live in AppDeckController — renderer is state-free.
    readonly property real hoverX: AppDeckController.hoverX
    readonly property bool hovered: AppDeckController.dockHovered
    // Icon slot width driven by DesktopSettings.dockIconSize: small=48, medium=58, large=72
    readonly property real iconSlotWidth: {
        if (layoutIconSlotWidth > 0) {
            return layoutIconSlotWidth
        }
        const sz = (typeof DesktopSettings !== "undefined" && DesktopSettings
                    && DesktopSettings.dockIconSize !== undefined)
                   ? DesktopSettings.dockIconSize : "medium"
        if (sz === "small") return 48
        if (sz === "large") return 72
        return 58
    }
    readonly property bool magnificationEnabled: (typeof DesktopSettings !== "undefined"
                                                  && DesktopSettings
                                                  && DesktopSettings.dockMagnificationEnabled !== undefined)
                                                 ? DesktopSettings.dockMagnificationEnabled : true
    readonly property real amplitude: layoutMagnificationAmplitude >= 0
                                     ? layoutMagnificationAmplitude
                                     : (magnificationEnabled ? 10 : 0)
    readonly property real sigma: layoutMagnificationSigma >= 0 ? layoutMagnificationSigma : 148
    readonly property real hoverLift: layoutHoverLift >= 0 ? layoutHoverLift : 6
    readonly property real glowWidth: layoutGlowWidth >= 0 ? layoutGlowWidth : 170
    readonly property real baseWidth: Math.max(320, (appsRepeater.count + 1) * iconSlotWidth + 8)
    readonly property real contentVPadding: 3
    readonly property real backgroundHeightBoost: Math.max(0, iconSlotWidth - 58)
    readonly property real baseHeight: 68 + (contentVPadding * 2) + backgroundHeightBoost
    readonly property real rowX: dockRow.x
    // Export precise interactive bounds so host surface can map input region
    // to the visible dock body instead of full component bounds.
    readonly property real inputRegionX: dockBackground.x
    readonly property real inputRegionY: Math.max(0, dockBackground.y - Math.max(0, Number(liftMax || 0)))
    readonly property real inputRegionWidth: dockBackground.width
    readonly property real inputRegionHeight: dockBackground.height + Math.max(0, Number(liftMax || 0))
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
    property string motionPreset: (typeof DesktopSettings !== "undefined"
                                   && DesktopSettings
                                   && DesktopSettings.dockMotionPreset !== undefined)
                                  ? DesktopSettings.dockMotionPreset : "macos-lively" // "subtle" | "macos-lively"
    property bool dropPulseEnabled: (typeof DesktopSettings !== "undefined"
                                     && DesktopSettings
                                     && DesktopSettings.dockDropPulseEnabled !== undefined)
                                    ? DesktopSettings.dockDropPulseEnabled : true
    property int dragThresholdMousePx: (typeof DesktopSettings !== "undefined"
                                        && DesktopSettings
                                        && DesktopSettings.dockDragThresholdMouse !== undefined)
                                       ? DesktopSettings.dockDragThresholdMouse : 6
    property int dragThresholdTouchpadPx: (typeof DesktopSettings !== "undefined"
                                           && DesktopSettings
                                           && DesktopSettings.dockDragThresholdTouchpad !== undefined)
                                          ? DesktopSettings.dockDragThresholdTouchpad : 3
    property var separatorAfterDesktopFiles: []
    property var iconRectsCache: []
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
    readonly property real edgePadding: layoutEdgePadding >= 0 ? layoutEdgePadding : 6
    signal appActivated(string appName)
    signal apphubRequested()
    signal iconRectsChanged(var rects)

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    width: Math.max(baseWidth, dockRow.width + (edgePadding * 2))
    height: baseHeight + Math.max(0, Number(liftMax || 10)) + 8
    radius: Theme.radiusWindow
    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"
    clip: false

    readonly property bool dockTransparent: forceTransparentBackground
                                             || ((typeof DesktopSettings !== "undefined"
                                                  && DesktopSettings
                                                  && DesktopSettings.dockTransparent !== undefined)
                                                 ? !!DesktopSettings.dockTransparent : false)

    AppDeckReorderController {
        id: reorderState
    }

    function nearestPinnedIndex(globalX) {
        var localX = Number(globalX || 0)
        if (root.window) {
            localX -= Number(root.window.x || 0)
        }
        var rects = root.iconRectsCache || []
        var x = Number(localX || 0)
        var bestDist = Number.MAX_VALUE
        var bestModelIndex = -1
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r || r.pinned !== true) continue
            var cx = Number(r.x || 0) + (Number(r.width || 0) * 0.5)
            var dist = Math.abs(cx - x)
            if (dist < bestDist) {
                bestDist = dist
                bestModelIndex = Number(r.modelIndex)
            }
        }
        return bestModelIndex
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
        var rects = root.iconRectsCache || []
        var centers = []
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r || r.pinned !== true) continue
            centers.push(Number(r.x || 0) + (Number(r.width || 0) * 0.5))
        }
        centers.sort(function(a, b) { return a - b })
        if (centers.length <= 0) return 0
        var x = Number(localX || 0)
        for (var idx = 0; idx < centers.length; ++idx) {
            if (x < centers[idx]) return idx
        }
        return centers.length
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
        if (AppDeckModel.addDesktopEntryAt) {
            ok = AppDeckModel.addDesktopEntryAt(desktopFile, pinnedPos)
        } else {
            ok = AppDeckModel.addDesktopEntry(desktopFile)
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
                AppDeckModel.movePinnedEntry(result.fromIndex, result.target)
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

    function collectIconRects() {
        var rects = []
        rects.push({
            "id": "apphub",
            "x": dockRow.x + apphubItem.x,
            "y": dockRow.y + apphubItem.y,
            "width": apphubItem.width,
            "height": apphubItem.height,
            "pinned": false,
            "modelIndex": -1
        })
        for (var i = 0; i < appsRepeater.count; ++i) {
            var item = appsRepeater.itemAt(i)
            if (!item) {
                continue
            }
            rects.push({
                "id": String(item.desktopFile || item.name || i),
                "x": dockRow.x + item.x,
                "y": dockRow.y + item.y,
                "width": item.width,
                "height": item.height,
                "pinned": item.dragEnabled === true,
                "modelIndex": i
            })
        }
        return rects
    }

    function scheduleIconRectsEmit() {
        iconRectEmitDebounce.restart()
    }

    function syncHoverFromController() {
        if (!rendererActive || !acceptsInput) {
            AppDeckController.dockHovered = false
            return
        }
        if (AppDeckController.inputOwnerHost !== root.hostName) {
            return
        }
        if (String(AppDeckController.hoveredItemId || "").length > 0) {
            AppDeckController.dockHovered = true
            var nextX = -1
            var targetId = String(AppDeckController.hoveredItemId || "")
            var rects = root.iconRectsCache || []
            for (var i = 0; i < rects.length; ++i) {
                var r = rects[i]
                if (!r) continue
                if (String(r.id || "") !== targetId) continue
                nextX = Number(r.x || 0) + (Number(r.width || 0) * 0.5)
                break
            }
            if (nextX < 0) {
                nextX = Number(AppDeckController.lastHoverPosition || (width * 0.5))
            }
            AppDeckController.hoverX = Math.max(0, Math.min(width, nextX))
        } else if (!dockHoverHandler.hovered) {
            AppDeckController.dockHovered = false
        }
    }

    function clearOwnedHover() {
        AppDeckController.dockHovered = false
        if (AppDeckController.inputOwnerHost === root.hostName
                && String(AppDeckController.hoveredItemId || "").length > 0) {
            AppDeckController.onHover("", AppDeckController.hoverX, root.hostName)
        }
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

    function computeLiftForCenterX(centerX) {
        if (!hovered) {
            return 0
        }
        var cx = Number(centerX || 0)
        var distance = Math.abs(cx - Number(hoverX || 0))
        var radius = Math.max(1, Number(influenceRadius || 140))
        if (distance >= radius) {
            return 0
        }
        var normalized = 1.0 - (distance / radius)
        return Math.max(0, Number(liftMax || 10) * normalized)
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

    // Trigger a appdeck-item bounce animation when a window lifecycle event fires.
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
                typeof AppCommandRouter !== "undefined" &&
                AppCommandRouter &&
                AppCommandRouter.routeWithResult) {
            var focusRes = AppCommandRouter.routeWithResult("workspace.presentview", {
                                                                "viewId": focusedViewId
                                                            }, "appdeck")
            if (focusRes && focusRes.ok) {
                return "focus"
            }
        }
        if (focusedViewId.length > 0 &&
                typeof WorkspaceManager !== "undefined" &&
                WorkspaceManager &&
                WorkspaceManager.PresentView) {
            if (WorkspaceManager.PresentView(focusedViewId)) {
                return "focus"
            }
        }
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter &&
                AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopEntry", {
                                       desktopFile: String(entry.desktopFile || ""),
                                       executable: String(entry.executable || ""),
                                       name: String(entry.name || ""),
                                       iconName: String(entry.iconName || ""),
                                       iconSource: String(entry.iconSource || "")
                                   }, "appdeck")
            return "launch"
        }
        if (typeof AppExecutionGate !== "undefined" && AppExecutionGate &&
                AppExecutionGate.launchDesktopEntry) {
            AppExecutionGate.launchDesktopEntry(String(entry.desktopFile || ""),
                                                String(entry.executable || ""),
                                                String(entry.name || ""),
                                                String(entry.iconName || ""),
                                                String(entry.iconSource || ""),
                                                "appdeck")
            return "launch"
        }
        console.warn("AppDeck launch ignored: no router/gate available")
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

    Rectangle {
        id: dockBackground
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: dockRow.width + (root.edgePadding * 2)
        height: root.baseHeight
        radius: Theme.radiusWindow
        visible: !root.dockTransparent
        color: root.dockTransparent ? "transparent" : Theme.color("dockBg")
        border.color: (root.dockTransparent || root.hideBackgroundBorder)
                      ? "transparent"
                      : Theme.color("panelBorder")
        border.width: (root.dockTransparent || root.hideBackgroundBorder)
                      ? Theme.borderWidthNone
                      : Theme.borderWidthThin
        layer.enabled: visible
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.34 : 0.18)
            shadowBlur: 0.42
            shadowVerticalOffset: 6
            shadowHorizontalOffset: 0
        }

        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.color("dockGlassTop") }
            GradientStop { position: 1.0; color: Theme.color("dockGlassBottom") }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.topMargin: 1
            height: 1
            radius: Theme.radiusHairline
            color: Theme.color("dockSpecLine")
            opacity: root.hideBackgroundBorder ? 0.0 : 0.68
        }

        Behavior on color {
            enabled: root.microAnimationAllowed()
            ColorAnimation { duration: Theme.transitionDuration; easing.type: Theme.easingStandard }
        }
        Behavior on border.color {
            enabled: root.microAnimationAllowed()
            ColorAnimation { duration: Theme.transitionDuration; easing.type: Theme.easingStandard }
        }
    }

    Row {
        id: dockRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0

        spacing: layoutItemSpacing >= 0 ? layoutItemSpacing : 0

        AppDeckItem {
            id: apphubItem
            label: "AppHub"
            iconPath: "qrc:/icons/apphub.svg"
            baseSlotWidth: root.iconSlotWidth
            hoverIndicatorEnabled: true
            directHoverOverride: AppDeckController.inputOwnerHost === root.hostName
                                 && AppDeckController.hoveredItemId === "apphub"
            showRunningDot: false
            hoverLift: root.liftMax
            influence: 0
            liftOffset: (AppDeckController.inputOwnerHost === root.hostName
                         && AppDeckController.hoveredItemId === "apphub")
                        ? root.liftMax : 0
            animationDuration: root.animationDuration
            itemScale: 1.0
            onClicked: {
                AppDeckController.onActivate("apphub", root.hostName)
                apphubItem.playBounce("focus")
                root.apphubRequested()
            }
        }

        Repeater {
            id: appsRepeater
            model: root.appsModel
            onCountChanged: root.scheduleIconRectsEmit()

            delegate: AppDeckAppDelegate {
                dockRoot: root
                modelCount: appsRepeater.count
                baseSlotWidth: root.iconSlotWidth
                hoverIndicatorEnabled: true
                directHoverOverride: AppDeckController.inputOwnerHost === root.hostName
                                     && AppDeckController.hoveredItemId === String(desktopFile || name || "")
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
                hoverLift: root.liftMax
                influence: 0
                liftOffset: (AppDeckController.inputOwnerHost === root.hostName
                             && AppDeckController.hoveredItemId === String(desktopFile || name || ""))
                            ? root.liftMax : 0
                animationDuration: root.animationDuration
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
        color: Theme.color("dockRevealHint")
        opacity: root.hovered ? 0.34 : 0.0

        Behavior on x {
            enabled: root.microAnimationAllowed()
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
        }
        Behavior on opacity {
            enabled: root.microAnimationAllowed()
            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
        }
    }

    AppDeckReorderMarker {
        id: reorderMarker
        active: root.draggingFromIndex >= 0 && root.draggingToIndex >= 0
        markerX: root.reorderMarkerX
        markerY: dockRow.y - 10
        draggingDeltaX: root.draggingDeltaX
    }

    AppDeckDropPulse {
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
        enabled: root.acceptsInput
        onHoveredChanged: {
            AppDeckController.dockHovered = hovered
            if (AppDeckController.inputOwnerHost === root.hostName) {
                if (!hovered) {
                    AppDeckController.onHover("", AppDeckController.hoverX, root.hostName)
                } else {
                    var nearestId = ""
                    var bestDist = Number.MAX_VALUE
                    var rects = root.iconRectsCache || []
                    var x = Number(AppDeckController.hoverX || 0)
                    for (var i = 0; i < rects.length; ++i) {
                        var r = rects[i]
                        if (!r) continue
                        var id = String(r.id || "")
                        if (!id.length) continue
                        var cx = Number(r.x || 0) + (Number(r.width || 0) * 0.5)
                        var dist = Math.abs(cx - x)
                        if (dist < bestDist) {
                            bestDist = dist
                            nearestId = id
                        }
                    }
                    AppDeckController.onHover(nearestId, AppDeckController.hoverX, root.hostName)
                }
            }
        }
        onPointChanged: {
            if (hovered) {
                AppDeckController.hoverX = Number(point.position.x || AppDeckController.hoverX)
                if (AppDeckController.inputOwnerHost === root.hostName) {
                    var nearestId = ""
                    var bestDist = Number.MAX_VALUE
                    var rects = root.iconRectsCache || []
                    var x = Number(AppDeckController.hoverX || 0)
                    for (var i = 0; i < rects.length; ++i) {
                        var r = rects[i]
                        if (!r) continue
                        var id = String(r.id || "")
                        if (!id.length) continue
                        var cx = Number(r.x || 0) + (Number(r.width || 0) * 0.5)
                        var dist = Math.abs(cx - x)
                        if (dist < bestDist) {
                            bestDist = dist
                            nearestId = id
                        }
                    }
                    AppDeckController.onHover(nearestId, AppDeckController.hoverX, root.hostName)
                }
            }
        }
    }

    Component.onCompleted: {
        console.info("[AppDeck] DOCK_CREATED renderer ptr=" + root + " host=" + hostName)
        scheduleIconRectsEmit()
    }
    onRendererActiveChanged: {
        if (rendererActive) {
            syncHoverFromController()
        } else {
            clearOwnedHover()
        }
        scheduleIconRectsEmit()
    }
    onAcceptsInputChanged: {
        if (acceptsInput) {
            syncHoverFromController()
        } else {
            clearOwnedHover()
        }
    }
    Connections {
        target: AppDeckController
        function onDockHoveredChanged() {
            if (!AppDeckController.dockHovered) {
                root.clearOwnedHover()
            }
        }
    }
    onWidthChanged: scheduleIconRectsEmit()
    onHeightChanged: scheduleIconRectsEmit()
    onXChanged: scheduleIconRectsEmit()
    onYChanged: scheduleIconRectsEmit()

    Timer {
        id: iconRectEmitDebounce
        interval: 0
        repeat: false
        onTriggered: {
            var rects = root.collectIconRects()
            root.iconRectsCache = rects
            root.iconRectsChanged(rects)
        }
    }

    Connections {
        target: AppDeckController
        ignoreUnknownSignals: true
        function onInputOwnerHostChanged() { root.syncHoverFromController() }
        function onHoveredItemIdChanged() { root.syncHoverFromController() }
        function onLastHoverPositionChanged() { root.syncHoverFromController() }
    }
}
