import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"
import "components/compositor" as CompositorComp
import "components/shell" as ShellComp
import "components/style" as StyleComp

Item {
    id: root
    signal shellContextMenuRequested(real x, real y)

    property var dockItem: null
    property bool launchpadVisible: false
    property bool styleGalleryVisible: false
    property bool workspaceVisible: MultitaskingController.workspaceVisible
    // Compatibility alias during rebrand (overview -> workspace).
    property alias overviewVisible: root.workspaceVisible
    property int lastPushedWorkspaceState: -1
    // Compatibility alias for older scripts/logics.
    property alias lastPushedOverviewState: root.lastPushedWorkspaceState
    property int panelIconSize: 24
    property int panelHeight : Theme.metric("topBarHeight")
    property int dockBottomMargin: 0
    property color transitionColor: Theme.color("transitionTint")
    property string dockHideMode: "duration_hide" // no_hide | duration_hide | smart_hide
    property string lastPushedDockHideMode: ""
    property int lastPushedActiveSpace: -1
    property int lastKnownActiveSpace: -1
    property bool spaceHudBootstrapped: false
    property int lastPushedLaunchpadState: -1
    property int dockHideDurationMs: 450
    property bool dockSmartOccluded: false
    property bool shellPointerBlocked: false
    property bool workspaceSwipeActive: MultitaskingController.workspaceSwipeActive
    property bool workspaceSwipeSettling: MultitaskingController.workspaceSwipeSettling
    property bool workspaceLifecycleActive: false
    property bool workspaceSwitchLifecycleActive: false
    property bool windowLifecycleActive: false
    property bool windowFocusLifecycleActive: false
    property bool windowLifecycleProfilePinned: false
    property string windowLifecyclePrevChannel: ""
    property string windowLifecyclePrevPreset: ""
    property int windowLifecycleReleaseMs: Theme.durationNormal
    property real workspaceSwipeOffset: MultitaskingController.workspaceSwipeOffset
    property int workspaceSwipeStartSpace: MultitaskingController.workspaceSwipeStartSpace
    property int workspaceSwipeTargetSpace: MultitaskingController.workspaceSwipeTargetSpace
    readonly property real workspaceSwipeThreshold: Math.max(96, width * 0.14)
    readonly property real workspaceSwipeVelocityThreshold: 720
    readonly property real workspaceSwipeMaxOffset: Math.max(140, width * 0.30)
    property bool spaceSwitchHudVisible: MultitaskingController.spaceSwitchHudVisible
    property string spaceSwitchHudText: MultitaskingController.spaceSwitchHudText
    property int spaceSwitchDirection: MultitaskingController.spaceSwitchDirection
    readonly property bool dockModeNoHide: dockHideMode === "no_hide"
    readonly property bool dockModeDurationHide: dockHideMode === "duration_hide"
    readonly property bool dockModeSmartHide: dockHideMode === "smart_hide"
    readonly property int dockHideDelayMs: dockModeDurationHide ? dockHideDurationMs : 120
    readonly property real dockWidth: (dockItem && dockItem.width) ? dockItem.width : 520
    readonly property real dockHeight: (dockItem && dockItem.height) ? dockItem.height : 120
    readonly property real dockX: Math.round((width - dockWidth) / 2)
    readonly property real dockY: dockShownY
    readonly property bool dockHovered: !!(dockItem && dockItem.hovered)
    readonly property bool layerShellAvailable: (typeof WlrLayerShell !== "undefined") && !!WlrLayerShell
    readonly property bool layerShellSupported: layerShellAvailable && !!WlrLayerShell.isSupported()
    readonly property bool dockLayerReady: !layerShellAvailable
                                           || !layerShellSupported
                                           || ((typeof DockBootstrapState !== "undefined" && DockBootstrapState)
                                               ? !!DockBootstrapState.readyToRender
                                               : true)
    property real dockShownY: height - dockHeight - dockBottomMargin
    property real dockHiddenY: height - 10
    readonly property bool pointerNearDock: (
                                                pointerTracker.mouseX >= (dockX - 120) &&
                                                pointerTracker.mouseX <= (dockX + dockWidth + 120) &&
                                                pointerTracker.mouseY >= (dockY - 140) &&
                                                pointerTracker.mouseY <= height
                                            )
    property bool dockShownState: true
    readonly property bool dockUserRevealWanted: (
                                                    bottomEdgeHover.containsMouse ||
                                                    pointerTracker.mouseY >= (height - 44) ||
                                                    pointerNearDock ||
                                                    dockHovered
                                                )
    readonly property bool dockSmartRevealAllowed: (!dockModeSmartHide || !dockSmartOccluded)
    readonly property bool dockRevealWanted: (
                                                workspaceVisible ||
                                                dockModeNoHide ||
                                                (dockSmartRevealAllowed && dockUserRevealWanted)
                                            )
    // Note: launchpadVisible is intentionally excluded from dockRevealWanted.
    // DockWindow (z=200) is above LaunchpadWindow (z=190) in the scene graph —
    // the dock renders on top of the launchpad without any exclusion zone.

    ShellComp.Shell {
        id: shell
        anchors.fill: parent
        transform: Translate {
            x: root.workspaceSwipeOffset
        }
        dockTopY: root.dockShownY
        inputEnabled: true
        contextMenuOnly: false
        onAddToDockRequested: function(appData) {
            if (appData && appData.desktopFile && appData.desktopFile.length > 0) {
                DockModel.addDesktopEntry(appData.desktopFile)
            }
        }
        onDockDropHover: function(active, globalX, iconPath) {
            if (root.dockItem && root.dockItem.updateExternalDrop) {
                var gx = Number(globalX || 0)
                var gp = root.mapToGlobal(gx, 0)
                if (gp && gp.x !== undefined) {
                    gx = Number(gp.x)
                }
                root.dockItem.updateExternalDrop(active, gx, iconPath)
            }
        }
        onDockDropCommit: function(desktopFile, globalX, iconPath) {
            if (root.dockItem && root.dockItem.commitExternalDrop) {
                var gx = Number(globalX || 0)
                var gp = root.mapToGlobal(gx, 0)
                if (gp && gp.x !== undefined) {
                    gx = Number(gp.x)
                }
                root.dockItem.commitExternalDrop(desktopFile, gx, iconPath)
            }
        }
        onDockDropClear: {
            if (root.dockItem && root.dockItem.clearExternalDrop) {
                root.dockItem.clearExternalDrop()
            }
        }
        onAppLaunched: function(appData) {
            if (!appData) {
                return
            }
            if (typeof AppExecutionGate !== "undefined" && AppExecutionGate && AppExecutionGate.noteLaunch) {
                AppExecutionGate.noteLaunch(appData, "shell")
            } else {
                console.warn("Shell launch note ignored: AppExecutionGate.noteLaunch unavailable")
            }
        }
        onShellContextMenuRequested: function(x, y) {
            root.shellContextMenuRequested(x, y)
        }
    }

    MouseArea {
        id: pointerTracker
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        z: 900
    }

    DragHandler {
        id: workspaceSwipeDrag
        target: null
        enabled: !root.shellPointerBlocked &&
                 !root.launchpadVisible &&
                 !root.workspaceVisible &&
                 !root.styleGalleryVisible &&
                 !!(typeof SpacesManager !== "undefined" && SpacesManager && Number(SpacesManager.spaceCount || 1) > 1)
        xAxis.enabled: true
        yAxis.enabled: false
        property real dragDx: 0
        property real dragVx: 0

        onActiveChanged: {
            if (active) {
                dragDx = 0
                dragVx = 0
                MultitaskingController.beginSwipe()
                root.prepareWorkspaceSwipeChannel()
                if (typeof MotionController !== "undefined" && MotionController) {
                    MotionController.cancelAndSettle(root.workspaceSwipeOffset)
                }
            } else {
                MultitaskingController.finishSwipe(dragDx, dragVx)
            }
        }

        onTranslationChanged: {
            if (!active) {
                return
            }
            dragDx = translation.x
            var next = Math.max(-root.workspaceSwipeMaxOffset,
                                Math.min(root.workspaceSwipeMaxOffset, dragDx))
            MultitaskingController.updateSwipe(next)
        }

        onCentroidChanged: {
            if (active) {
                dragVx = Number(centroid.velocity.x || 0)
            }
        }
    }

    Timer {
        id: dockShowTimer
        interval: 20
        repeat: false
        onTriggered: root.dockShownState = true
    }

    Timer {
        id: dockHideTimer
        interval: root.dockHideDelayMs
        repeat: false
        onTriggered: root.dockShownState = false
    }

    onDockRevealWantedChanged: {
        if (dockModeNoHide) {
            dockHideTimer.stop()
            dockShowTimer.stop()
            root.dockShownState = true
            return
        }
        if (dockRevealWanted) {
            dockHideTimer.stop()
            dockShowTimer.restart()
        } else {
            dockShowTimer.stop()
            dockHideTimer.restart()
        }
    }
    onDockShownYChanged: refreshDockSmartOcclusion()

    Component.onCompleted: {
        syncDockHidePrefs()
        root.dockShownState = true
        if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            WindowingBackend.sendCommand("shellpopup off")
        }
        refreshShellPointerBlock()
        if (typeof ThemeIconController !== "undefined" && ThemeIconController &&
                ThemeIconController.applyForDarkMode) {
            ThemeIconController.applyForDarkMode(Theme.darkMode)
        }
        if (typeof NotificationManager !== "undefined" && NotificationManager &&
                typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference &&
                NotificationManager.setBubbleDurationMs) {
            var duration = Number(UIPreferences.getPreference("notifications.bubbleDurationMs", 5000))
            if (!isNaN(duration)) {
                NotificationManager.setBubbleDurationMs(duration)
            }
        }
        root.pushSpaceToCompositor()
        root.pushWorkspaceVisibilityToCompositor()
        root.syncWorkspaceLifecycleState()
        root.pushLaunchpadToCompositor()
        root.lastKnownActiveSpace = Number(SpacesManager && SpacesManager.activeSpace ? SpacesManager.activeSpace : 1)
    }
    Component.onDestruction: {
        if (root.workspaceLifecycleActive &&
                typeof MotionController !== "undefined" && MotionController &&
                MotionController.endLifecycleTransition) {
            MotionController.endLifecycleTransition("workspace.overview")
        }
        if (root.workspaceSwitchLifecycleActive &&
                typeof MotionController !== "undefined" && MotionController &&
                MotionController.endLifecycleTransition) {
            MotionController.endLifecycleTransition("workspace.switch")
        }
        if (root.windowLifecycleActive &&
                typeof MotionController !== "undefined" && MotionController &&
                MotionController.endLifecycleTransition) {
            MotionController.endLifecycleTransition("window.lifecycle")
        }
        if (root.windowFocusLifecycleActive &&
                typeof MotionController !== "undefined" && MotionController &&
                MotionController.endLifecycleTransition) {
            MotionController.endLifecycleTransition("window.focus")
        }
    }
    onStyleGalleryVisibleChanged: {
        if (ShellStateController) ShellStateController.setStyleGalleryVisible(styleGalleryVisible)
        if (!styleGalleryVisible) {
            syncDockHidePrefs()
        }
    }
    onDockHideModeChanged: pushDockModeToCompositor()

    MouseArea {
        id: bottomEdgeHover
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 30
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        z: 300
    }

    Rectangle {
        id: dockRevealHint
        visible: !root.dockShownState && !root.launchpadVisible && !root.workspaceVisible
        width: 120
        height: 6
        radius: Theme.radiusXs
        x: Math.round((root.width - width) / 2)
        y: root.height - height - 4
        z: 301
        color: Theme.color("dockRevealHint")
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingLight
            }
        }
    }

    Connections {
        target: root.dockItem
        ignoreUnknownSignals: true
        function onWidthChanged() { root.refreshDockSmartOcclusion() }
        function onHeightChanged() { root.refreshDockSmartOcclusion() }
        function onAppActivated() { root.launchpadVisible = false }
        function onLaunchpadRequested() { root.launchpadVisible = !root.launchpadVisible }
    }

    MouseArea {
        id: styleGalleryDismissArea
        anchors.fill: parent
        z: 429
        visible: styleGallery.visible
        enabled: visible
        onClicked: root.styleGalleryVisible = false
    }

    Rectangle {
        id: spaceSwitchHud
        z: 445
        visible: root.spaceSwitchHudVisible || opacity > 0.01
        opacity: root.spaceSwitchHudVisible ? 1.0 : 0.0
        width: 170
        height: 56
        radius: Theme.radiusWindow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.panelHeight + 14
        color: Theme.color("spaceHudBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("spaceHudBorder")
        transform: Translate {
            id: spaceSwitchHudTranslate
            x: root.spaceSwitchHudVisible ? 0
                                          : ((root.spaceSwitchDirection > 0) ? 24
                                                                             : ((root.spaceSwitchDirection < 0) ? -24 : 0))
        }

        Row {
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: root.spaceSwitchDirection > 0 ? "\u25b6"
                                                    : (root.spaceSwitchDirection < 0 ? "\u25c0" : "\u25c6")
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("bold")
            }

            Text {
                text: root.spaceSwitchHudText
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("bold")
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationNormal
                easing.type: Theme.easingDefault
            }
        }
    }


    SequentialAnimation {
        id: spaceSwitchShellAnim
        NumberAnimation {
            target: shell
            property: "opacity"
            from: 1.0
            to: 0.90
            duration: Theme.durationMicro
            easing.type: Theme.easingLight
        }
        NumberAnimation {
            target: shell
            property: "opacity"
            to: 1.0
            duration: Theme.durationFast
            easing.type: Theme.easingDefault
        }
    }

    CompositorComp.CompositorSwitcherOverlay {
        anchors.fill: parent
    }

    StyleComp.StyleGallery {
        id: styleGallery
        z: 430
        width: Math.min(parent.width - 120, 860)
        height: Math.min(parent.height - root.panelHeight - 80, 620)
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.panelHeight + 24
        visible: root.styleGalleryVisible
        onCloseRequested: root.styleGalleryVisible = false
        onDockHidePrefsApplied: root.syncDockHidePrefs()
    }

    Rectangle {
        id: themeTransition
        anchors.fill: parent
        z: 500
        visible: opacity > 0.01
        opacity: 0
        color: root.transitionColor
    }

    SequentialAnimation {
        id: themeTransitionAnim
        NumberAnimation {
            target: themeTransition
            property: "opacity"
            from: 0.0
            to: 0.20
            duration: Theme.durationFast
            easing.type: Theme.easingDefault
        }
        NumberAnimation {
            target: themeTransition
            property: "opacity"
            to: 0.0
            duration: Theme.durationNormal
            easing.type: Theme.easingDefault
        }
    }

    Connections {
        target: Theme
        function onDarkModeChanged() {
            root.transitionColor = Theme.color("transitionTint")
            themeTransitionAnim.restart()
            if (typeof ThemeIconController !== "undefined" && ThemeIconController &&
                    ThemeIconController.applyForDarkMode) {
                ThemeIconController.applyForDarkMode(Theme.darkMode)
            }
        }
    }

    Connections {
        target: (typeof WindowingBackend !== "undefined") ? WindowingBackend : null
        function onEventReceived(event, payload) {
            // Accept both workspace-* (canonical) and overview-* (legacy).
            if (event === "workspace-open" || event === "overview-open") {
                root.lastPushedWorkspaceState = 1
                root.workspaceVisible = true
            } else if (event === "workspace-close" || event === "overview-close") {
                root.lastPushedWorkspaceState = 0
                root.workspaceVisible = false
            } else if (event === "workspace-toggle" || event === "overview-toggle") {
                root.lastPushedWorkspaceState = root.workspaceVisible ? 0 : 1
                root.workspaceVisible = !root.workspaceVisible
            } else if (event === "launchpad-open") {
                root.lastPushedLaunchpadState = 1
                root.launchpadVisible = true
            } else if (event === "launchpad-close") {
                root.lastPushedLaunchpadState = 0
                root.launchpadVisible = false
            }
        }
    }

    Connections {
        target: (typeof WorkspaceManager !== "undefined") ? WorkspaceManager : null
        ignoreUnknownSignals: true
        function onWorkspaceVisibilityChanged(visible) {
            var state = visible ? 1 : 0
            root.lastPushedWorkspaceState = state
            if (root.workspaceVisible !== visible) {
                root.workspaceVisible = visible
            }
        }
        function onWorkspaceShown() {
            root.lastPushedWorkspaceState = 1
            if (!root.workspaceVisible) {
                root.workspaceVisible = true
            }
        }
    }

    Connections {
        target: CompositorStateModel
        ignoreUnknownSignals: true
        function onLastEventChanged() {
            root.refreshDockSmartOcclusion()
            root.refreshShellPointerBlock()
            if (CompositorStateModel && CompositorStateModel.lastEvent) {
                root.processCompositorLifecycleEvent(CompositorStateModel.lastEvent)
            }
        }
        function onConnectedChanged() {
            root.refreshShellPointerBlock()
        }
        function onSpaceChanged() {
            if (typeof SpacesManager !== "undefined" && SpacesManager &&
                    SpacesManager.setActiveSpace && CompositorStateModel) {
                var nextSpace = Number(CompositorStateModel.activeSpace || 1)
                var prevSpace = Number(root.lastKnownActiveSpace > 0 ? root.lastKnownActiveSpace : nextSpace)
                root.lastPushedActiveSpace = nextSpace
                SpacesManager.setActiveSpace(nextSpace)
                if (SpacesManager.setSpaceCount) {
                    SpacesManager.setSpaceCount(CompositorStateModel.spaceCount)
                }
                if (root.spaceHudBootstrapped) {
                    root.showSpaceSwitchHud(nextSpace, prevSpace)
                } else {
                    root.spaceHudBootstrapped = true
                }
                root.lastKnownActiveSpace = nextSpace
            }
        }
    }

    Connections {
        target: (typeof SpacesManager !== "undefined") ? SpacesManager : null
        function onActiveSpaceChanged() {
            root.pushSpaceToCompositor()
            var nextSpace = Number(SpacesManager.activeSpace || 1)
            var prevSpace = Number(root.lastKnownActiveSpace > 0 ? root.lastKnownActiveSpace : nextSpace)
            if (root.spaceHudBootstrapped) {
                root.showSpaceSwitchHud(nextSpace, prevSpace)
            } else {
                root.spaceHudBootstrapped = true
            }
            root.lastKnownActiveSpace = nextSpace
            if (nextSpace !== prevSpace &&
                    typeof MotionController !== "undefined" && MotionController &&
                    MotionController.beginLifecycleTransition) {
                MotionController.beginLifecycleTransition("workspace.switch", MotionController.MediumPriority)
                root.workspaceSwitchLifecycleActive = true
                workspaceSwitchLifecycleReleaseTimer.restart()
            }
        }
    }

    onLaunchpadVisibleChanged: {
        if (launchpadVisible) {
            console.info("LAUNCHPAD show requested")
            if (!dockLayerReady) {
                console.info("LAUNCHPAD show allowed dockReady=false")
                launchpadVisible = false
                return
            }
            console.info("LAUNCHPAD show allowed dockReady=true")
        }
        if (ShellStateController) ShellStateController.setLaunchpadVisible(launchpadVisible)
        pushLaunchpadToCompositor()
    }
    onWorkspaceVisibleChanged: {
        if (ShellStateController) ShellStateController.setWorkspaceOverviewVisible(workspaceVisible)
        syncWorkspaceLifecycleState()
        pushWorkspaceVisibilityToCompositor()
    }
    onWorkspaceSwipeActiveChanged: syncWorkspaceLifecycleState()
    onWorkspaceSwipeSettlingChanged: syncWorkspaceLifecycleState()

    Timer {
        id: shellPointerBlockPoll
        interval: 300
        repeat: true
        running: true
        onTriggered: root.refreshShellPointerBlock()
    }

    Timer {
        id: windowLifecycleReleaseTimer
        interval: Math.max(1, Number(root.windowLifecycleReleaseMs || Theme.durationNormal))
        repeat: false
        onTriggered: {
            if (root.windowLifecycleActive &&
                    typeof MotionController !== "undefined" && MotionController &&
                    MotionController.endLifecycleTransition) {
                MotionController.endLifecycleTransition("window.lifecycle")
                root.windowLifecycleActive = false
            }
            root.releaseWindowLifecycleProfile()
        }
    }

    Timer {
        id: windowFocusLifecycleReleaseTimer
        interval: Theme.durationFast
        repeat: false
        onTriggered: {
        if (root.windowFocusLifecycleActive &&
                typeof MotionController !== "undefined" && MotionController &&
                MotionController.endLifecycleTransition) {
            MotionController.endLifecycleTransition("window.focus")
            root.windowFocusLifecycleActive = false
        }
        root.releaseWindowLifecycleProfile()
    }
    }

    Timer {
        id: workspaceSwitchLifecycleReleaseTimer
        interval: Theme.durationWorkspace
        repeat: false
        onTriggered: {
            if (root.workspaceSwitchLifecycleActive &&
                    typeof MotionController !== "undefined" && MotionController &&
                    MotionController.endLifecycleTransition) {
                MotionController.endLifecycleTransition("workspace.switch")
                root.workspaceSwitchLifecycleActive = false
            }
        }
    }

    Connections {
        target: (typeof WindowingBackend !== "undefined") ? WindowingBackend : null
        function onConnectedChanged() {
            root.pushDockModeToCompositor()
        }
    }

    function syncDockHidePrefs() {
        var mode = "duration_hide"
        var duration = 450
        if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference) {
            var legacyAuto = !!UIPreferences.getPreference("dock.autoHideEnabled", false)
            mode = String(UIPreferences.getPreference("dock.hideMode",
                                                      legacyAuto ? "duration_hide" : "no_hide"))
            duration = Number(UIPreferences.getPreference("dock.hideDurationMs", 450))
        }
        mode = mode.trim().toLowerCase()
        if (mode === "snart_hide") {
            mode = "smart_hide"
        }
        if (mode !== "no_hide" && mode !== "duration_hide" && mode !== "smart_hide") {
            mode = "duration_hide"
        }
        if (isNaN(duration)) {
            duration = 450
        }
        duration = Math.max(100, Math.min(5000, Math.round(duration)))
        root.dockHideMode = mode
        root.dockHideDurationMs = duration
        pushDockModeToCompositor()
        refreshDockSmartOcclusion()
    }

    function pushDockModeToCompositor() {
        var mode = String(root.dockHideMode || "").trim().toLowerCase()
        if (mode === "snart_hide") {
            mode = "smart_hide"
        }
        if (mode !== "no_hide" && mode !== "duration_hide" && mode !== "smart_hide") {
            return
        }
        if (mode === root.lastPushedDockHideMode) {
            return
        }
        if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            if (WindowingBackend.sendCommand("dock mode " + mode)) {
                root.lastPushedDockHideMode = mode
            }
        }
    }

    function pushSpaceToCompositor() {
        if (typeof SpacesManager === "undefined" || !SpacesManager) {
            return
        }
        var active = Number(SpacesManager.activeSpace || 1)
        if (isNaN(active) || active < 1) {
            active = 1
        }
        if (active === root.lastPushedActiveSpace) {
            return
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.SwitchWorkspace) {
            WorkspaceManager.SwitchWorkspace(active)
            root.lastPushedActiveSpace = active
            return
        }
        if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            if (WindowingBackend.sendCommand("space set " + active)) {
                root.lastPushedActiveSpace = active
            }
        }
    }

    function prepareWorkspaceSwipeChannel() {
        if (typeof MotionController === "undefined" || !MotionController) {
            return
        }
        if (MotionController.channel !== "workspace.swipe.offset") {
            MotionController.channel = "workspace.swipe.offset"
        }
        if (MotionController.preset !== "responsive") {
            MotionController.preset = "responsive"
        }
    }

    function syncWorkspaceLifecycleState() {
        if (typeof MotionController === "undefined" || !MotionController ||
                !MotionController.beginLifecycleTransition || !MotionController.endLifecycleTransition) {
            return
        }
        var shouldBeActive = !!root.workspaceVisible || !!root.workspaceSwipeActive || !!root.workspaceSwipeSettling
        if (shouldBeActive && !root.workspaceLifecycleActive) {
            MotionController.beginLifecycleTransition("workspace.overview", MotionController.MediumPriority)
            root.workspaceLifecycleActive = true
        } else if (!shouldBeActive && root.workspaceLifecycleActive) {
            MotionController.endLifecycleTransition("workspace.overview")
            root.workspaceLifecycleActive = false
        }
    }

    function processCompositorLifecycleEvent(payload) {
        if (!payload || typeof MotionController === "undefined" || !MotionController) {
            return
        }
        if (!MotionController.beginLifecycleTransition || !MotionController.endLifecycleTransition) {
            return
        }
        var eventName = String(payload.event || payload.eventName || payload.type || payload.action || "")
        eventName = eventName.toLowerCase().replace(/_/g, "-")
        if (eventName.length <= 0) {
            return
        }
        var focusMatch = (eventName === "window-focused" ||
                          eventName === "window-unfocused" ||
                          eventName === "window-activated" ||
                          eventName === "window-deactivated" ||
                          eventName === "focus-changed")
        if (focusMatch) {
            var focusKey = "window-focus:" + eventName + ":" + String(payload.viewId || payload.windowId || "")
            if (MotionController.shouldCoalesceEvent && MotionController.shouldCoalesceEvent(focusKey, 90)) {
                return
            }
            MotionController.beginLifecycleTransition("window.focus", MotionController.LowPriority)
            root.windowFocusLifecycleActive = true
            windowFocusLifecycleReleaseTimer.restart()
            return
        }
        var lifecycleMatch = (eventName === "window-created" ||
                              eventName === "window-opened" ||
                              eventName === "window-shown" ||
                              eventName === "window-closing" ||
                              eventName === "window-closed" ||
                              eventName === "window-minimized" ||
                              eventName === "window-unminimized")
        if (!lifecycleMatch) {
            return
        }
        var viewId = String(payload.viewId || payload.viewid || payload.windowId || payload.windowid || "")
        var coalesceKey = "window-lifecycle:" + eventName + ":" + viewId
        if (MotionController.shouldCoalesceEvent && MotionController.shouldCoalesceEvent(coalesceKey, 120)) {
            return
        }
        root.applyWindowLifecycleProfile(eventName)
        MotionController.beginLifecycleTransition("window.lifecycle", MotionController.HighPriority)
        root.windowLifecycleActive = true
        windowLifecycleReleaseTimer.restart()

        // Dock visual feedback: bounce the matching icon on open/minimize.
        var appId = String(payload.appId || payload.app_id || payload.appid || "")
        if (appId.length > 0 && root.dockItem && root.dockItem.notifyWindowLifecycle) {
            root.dockItem.notifyWindowLifecycle(eventName, appId)
        }
    }

    function windowLifecycleProfileForEvent(eventName) {
        var event = String(eventName || "").toLowerCase()
        if (event === "window-minimized") {
            // Minimize: quick settle (no genie-like distortion).
            return {
                "channel": "window.minimize",
                "preset": "snappy",
                "releaseMs": Theme.durationFast
            }
        }
        if (event === "window-closing" || event === "window-closed") {
            // Close: fast decay with crisp response.
            return {
                "channel": "window.close",
                "preset": "snappy",
                "releaseMs": Theme.durationFast
            }
        }
        if (event === "window-created" || event === "window-opened" ||
                event === "window-shown" || event === "window-unminimized") {
            // Open/restore: smooth scale+fade style profile.
            return {
                "channel": "window.open",
                "preset": "smooth",
                "releaseMs": Theme.durationNormal
            }
        }
        return {
            "channel": "window.lifecycle",
            "preset": "smooth",
            "releaseMs": Theme.durationNormal
        }
    }

    function applyWindowLifecycleProfile(eventName) {
        if (typeof MotionController === "undefined" || !MotionController) {
            return
        }
        var profile = root.windowLifecycleProfileForEvent(eventName)
        var nextChannel = String(profile.channel || "window.lifecycle")
        var nextPreset = String(profile.preset || "smooth")
        var releaseMs = Number(profile.releaseMs || Theme.durationNormal)
        if (isNaN(releaseMs) || releaseMs < 1) {
            releaseMs = Theme.durationNormal
        }
        root.windowLifecycleReleaseMs = Math.max(1, Math.round(releaseMs))
        if (!root.windowLifecycleProfilePinned) {
            root.windowLifecyclePrevChannel = String(MotionController.channel || "")
            root.windowLifecyclePrevPreset = String(MotionController.preset || "")
            root.windowLifecycleProfilePinned = true
        }
        if (MotionController.channel !== nextChannel) {
            MotionController.channel = nextChannel
        }
        if (MotionController.preset !== nextPreset) {
            MotionController.preset = nextPreset
        }
    }

    function releaseWindowLifecycleProfile() {
        if (!root.windowLifecycleProfilePinned ||
                typeof MotionController === "undefined" || !MotionController) {
            return
        }
        if (root.windowLifecyclePrevChannel.length > 0 &&
                MotionController.channel !== root.windowLifecyclePrevChannel) {
            MotionController.channel = root.windowLifecyclePrevChannel
        }
        if (root.windowLifecyclePrevPreset.length > 0 &&
                MotionController.preset !== root.windowLifecyclePrevPreset) {
            MotionController.preset = root.windowLifecyclePrevPreset
        }
        root.windowLifecycleProfilePinned = false
        root.windowLifecyclePrevChannel = ""
        root.windowLifecyclePrevPreset = ""
        root.windowLifecycleReleaseMs = Theme.durationNormal
    }


    function showSpaceSwitchHud(nextSpace, prevSpace) {
        var next = Number(nextSpace || 1)
        var prev = Number(prevSpace || next)
        MultitaskingController.showSpaceSwitchHud(next, prev)
        if (spaceSwitchShellAnim.running) {
            spaceSwitchShellAnim.stop()
        }
        spaceSwitchShellAnim.restart()
    }

    function pushLaunchpadToCompositor() {
        var state = root.launchpadVisible ? 1 : 0
        if (state === root.lastPushedLaunchpadState) {
            return
        }
        if (root.launchpadVisible &&
                typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.ShowAppGrid) {
            WorkspaceManager.ShowAppGrid()
            root.lastPushedLaunchpadState = state
            return
        }
        if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            if (WindowingBackend.sendCommand("launchpad " + (root.launchpadVisible ? "on" : "off"))) {
                root.lastPushedLaunchpadState = state
            }
        }
    }

    function pushWorkspaceVisibilityToCompositor() {
        var state = root.workspaceVisible ? 1 : 0
        if (state === root.lastPushedWorkspaceState) {
            return
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager) {
            if (root.workspaceVisible) {
                if (WorkspaceManager.ShowWorkspace) {
                    WorkspaceManager.ShowWorkspace()
                    root.lastPushedWorkspaceState = state
                    return
                }
            } else {
                if (WorkspaceManager.HideWorkspace) {
                    WorkspaceManager.HideWorkspace()
                    root.lastPushedWorkspaceState = state
                    return
                }
            }
        }
        if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
            if (WindowingBackend.sendCommand("workspace " + (root.workspaceVisible ? "on" : "off"))
                    || WindowingBackend.sendCommand("overview " + (root.workspaceVisible ? "on" : "off"))) {
                root.lastPushedWorkspaceState = state
            }
        }
    }

    // Backward-compatible alias while older callsites still reference
    // legacy overview-oriented naming.
    function pushOverviewToCompositor() {
        pushWorkspaceVisibilityToCompositor()
    }

    function toggleWorkspaceOverview() {
        // Rebrand contract: workspace surface is the multitasking UI.
        root.workspaceVisible = !root.workspaceVisible
        return root.workspaceVisible
    }

    function exitWorkspaceOverview() {
        if (!root.workspaceVisible) {
            return false
        }
        root.workspaceVisible = false
        return true
    }

    function switchWorkspaceByDelta(delta) {
        var step = Number(delta || 0)
        if (step === 0) {
            return false
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.SwitchWorkspaceByDelta) {
            WorkspaceManager.SwitchWorkspaceByDelta(step)
            return true
        }
        if (typeof SpacesManager === "undefined" || !SpacesManager) {
            return false
        }
        var active = Number(SpacesManager.activeSpace || 1)
        var count = Number(SpacesManager.spaceCount || 1)
        var target = Math.max(1, Math.min(count, active + step))
        if (target === active) {
            return false
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.SwitchWorkspace) {
            WorkspaceManager.SwitchWorkspace(target)
            return true
        }
        if (SpacesManager.setActiveSpace) {
            SpacesManager.setActiveSpace(target)
            return true
        }
        return false
    }

    function focusedWorkspaceViewId() {
        if (typeof CompositorStateModel === "undefined" || !CompositorStateModel ||
                !CompositorStateModel.windowCount || !CompositorStateModel.windowAt) {
            return ""
        }
        var n = CompositorStateModel.windowCount()
        for (var i = 0; i < n; ++i) {
            var w = CompositorStateModel.windowAt(i)
            if (!w || !w.focused || isShellWindow(w.appId, w.title)) {
                continue
            }
            return String(w.viewId || "")
        }
        return ""
    }

    function moveFocusedWindowByDelta(delta) {
        var step = Number(delta || 0)
        if (step === 0) {
            return false
        }
        if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                WorkspaceManager.MoveFocusedWindowByDelta) {
            return !!WorkspaceManager.MoveFocusedWindowByDelta(step)
        }
        if (typeof SpacesManager === "undefined" || !SpacesManager) {
            return false
        }
        var viewId = focusedWorkspaceViewId()
        if (viewId.length <= 0) {
            return false
        }
        var current = 0
        if (SpacesManager.windowSpace) {
            current = Number(SpacesManager.windowSpace(viewId) || 0)
        }
        if (current <= 0) {
            current = Number(SpacesManager.activeSpace || 1)
        }
        var count = Number(SpacesManager.spaceCount || 1)
        var target = Math.max(1, Math.min(count, current + step))
        if (target === current || typeof WorkspaceManager === "undefined" ||
                !WorkspaceManager || !WorkspaceManager.MoveWindowToWorkspace) {
            return false
        }
        var ok = !!WorkspaceManager.MoveWindowToWorkspace(viewId, target)
        if (!ok) {
            return false
        }
        // Keep focus continuity after move by following the moved window's workspace.
        if (WorkspaceManager.SwitchWorkspace) {
            WorkspaceManager.SwitchWorkspace(target)
        } else if (SpacesManager.setActiveSpace) {
            SpacesManager.setActiveSpace(target)
        }
        return true
    }

    function isShellWindow(appId, title) {
        var app = String(appId || "").toLowerCase()
        var ttl = String(title || "").toLowerCase()
        return app === "appdesktop_shell" ||
               app === "desktop_shell" ||
               app === "desktopshell" ||
               app.indexOf("desktop_shell") >= 0 ||
               ttl === "desktop shell"
    }

    function rectsIntersect(ax, ay, aw, ah, bx, by, bw, bh) {
        if (aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0) {
            return false
        }
        return ax < (bx + bw) &&
               bx < (ax + aw) &&
               ay < (by + bh) &&
               by < (ay + ah)
    }

    function refreshDockSmartOcclusion() {
        if (!dockModeSmartHide ||
                typeof CompositorStateModel === "undefined" || !CompositorStateModel ||
                !CompositorStateModel.windowCount || !CompositorStateModel.windowAt) {
            root.dockSmartOccluded = false
            return
        }

        var dockX = root.dockX
        var dockY = root.dockShownY
        var dockW = root.dockWidth
        var dockH = root.dockHeight
        var occluded = false
        var count = CompositorStateModel.windowCount()
        for (var i = 0; i < count; ++i) {
            var w = CompositorStateModel.windowAt(i)
            if (!w) {
                continue
            }
            if (!!w.minimized || !w.mapped || isShellWindow(w.appId, w.title)) {
                continue
            }
            if (rectsIntersect(Number(w.x || 0), Number(w.y || 0),
                               Number(w.width || 0), Number(w.height || 0),
                               dockX, dockY, dockW, dockH)) {
                occluded = true
                break
            }
        }
        root.dockSmartOccluded = occluded
    }

    function refreshShellPointerBlock() {
        if (typeof CompositorStateModel === "undefined" || !CompositorStateModel ||
                !CompositorStateModel.windowCount || !CompositorStateModel.windowAt) {
            root.shellPointerBlocked = false
            return
        }
        var blocked = false
        var count = CompositorStateModel.windowCount()
        for (var i = 0; i < count; ++i) {
            var w = CompositorStateModel.windowAt(i)
            if (!w) {
                continue
            }
            if (!!w.minimized || !w.mapped || isShellWindow(w.appId, w.title)) {
                continue
            }
            blocked = true
            break
        }
        root.shellPointerBlocked = blocked
    }

    function arrangeShellShortcuts() {
        if (shell && shell.arrangeShortcuts) {
            shell.arrangeShortcuts()
        }
    }

    function refreshShellShortcuts() {
        if (shell && shell.refreshShortcuts) {
            shell.refreshShortcuts()
        }
    }

    Connections {
        target: (typeof MotionController !== "undefined" && MotionController) ? MotionController : null
        ignoreUnknownSignals: true
        function onValueChanged() {
            if (!root.workspaceSwipeSettling) {
                return
            }
            if (MotionController.channel !== "workspace.swipe.offset") {
                return
            }
            root.workspaceSwipeOffset = Number(MotionController.value || 0)
            if (Math.abs(root.workspaceSwipeOffset) <= 0.2) {
                root.workspaceSwipeOffset = 0
                root.workspaceSwipeSettling = false
            }
        }
    }

    Shortcut {
        sequence: "Meta+S"
        onActivated: MultitaskingController.toggleWorkspace()
    }

    Shortcut {
        sequence: "Meta+Left"
        onActivated: MultitaskingController.switchWorkspaceDelta(-1)
    }

    Shortcut {
        sequence: "Meta+Right"
        onActivated: MultitaskingController.switchWorkspaceDelta(1)
    }

    Shortcut {
        sequence: "Meta+Shift+Left"
        onActivated: {
            if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                    WorkspaceManager.MoveFocusedWindowByDelta) {
                WorkspaceManager.MoveFocusedWindowByDelta(-1)
            }
        }
    }

    Shortcut {
        sequence: "Meta+Shift+Right"
        onActivated: {
            if (typeof WorkspaceManager !== "undefined" && WorkspaceManager &&
                    WorkspaceManager.MoveFocusedWindowByDelta) {
                WorkspaceManager.MoveFocusedWindowByDelta(1)
            }
        }
    }

    Shortcut {
        sequence: "Meta+N"
        onActivated: {
            if (typeof NotificationManager !== "undefined" && NotificationManager &&
                    NotificationManager.toggleCenter) {
                NotificationManager.toggleCenter()
            }
        }
    }
}
