import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    property var appsModel: []
    property string searchText: ""
    property int currentPage: 0
    property int maxColumns: 7
    property int maxRowsPerPage: 4
    property int topSafeInset: 0
    property int bottomSafeInset: 144
    property var desktopScene: null
    property int minColumns: 7
    property int minRowsPerPage: 4
    readonly property int effectiveColumns: Math.max(minColumns,
                                                     Math.min(maxColumns,
                                                              Math.floor((appsFlick.width + gridSpacing)
                                                                         / (minCellWidth + gridSpacing))))
    readonly property int effectiveRowsPerPage: Math.max(minRowsPerPage,
                                                         Math.min(maxRowsPerPage,
                                                                  Math.floor((appsFlick.height + gridSpacing)
                                                                             / (appCellMinHeight + gridSpacing))))
    property int pageSize: Math.max(1, effectiveColumns * effectiveRowsPerPage)
    property real pageTransitionOffset: 0
    property real pageTransitionOpacity: 1
    property real pageTransitionHeaderOffset: 0
    property real pageTransitionDotsOffset: 0
    property int pageTransitionDirection: 1
    property bool pageTransitionPending: false
    // Reveal progress (0..1) driven by MotionController for stagger-in animation.
    // 0 = fully hidden, 1 = fully visible. Set externally by LaunchpadWindow.
    property real revealProgress: 1.0
    readonly property real staggerDelayPerRow: 0.07
    property int filteredCount: 0
    property int totalPages: 1
    property var pagedApps: []
    property int selectedIndex: 0
    property bool compositorBlurHint: true
    property bool compositorSceneFxEnabled: false
    readonly property bool compositorBlurActive: compositorBlurHint && compositorSceneFxEnabled

    signal dismissRequested()
    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    readonly property string wallpaperSource: {
        const uri = String((typeof DesktopSettings !== "undefined" && DesktopSettings)
                           ? (DesktopSettings.wallpaperUri || "") : "")
        return uri.length > 0 ? uri : "qrc:/images/wallpaper.jpeg"
    }

    readonly property real gridSpacing: 16
    readonly property real minCellWidth: 122
    readonly property real appLabelHeight: Math.ceil(Theme.fontSize("menu") * 1.8)
    readonly property real iconPlateSize: Math.min(86, Math.floor(gridCellWidth * 0.55))
    readonly property real appCellMinHeight: iconPlateSize + 8 + appLabelHeight + 8
    readonly property real gridCellWidth: Math.max(minCellWidth,
                                                   Math.floor((appsFlick.width - (gridSpacing * (effectiveColumns - 1)))
                                                              / Math.max(1, effectiveColumns)))
    readonly property real gridCellHeight: Math.max(appCellMinHeight,
                                                    Math.floor((appsFlick.height
                                                                - (gridSpacing * (effectiveRowsPerPage - 1)))
                                                               / Math.max(1, effectiveRowsPerPage)))

    function prefBool(key, fallback) {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
            return !!fallback
        }
        return !!DesktopSettings.settingValue(String(key || ""), fallback)
    }

    function refreshCompositorBlurPrefs() {
        compositorBlurHint = prefBool("launchpad.compositorBlurHint", true)
        compositorSceneFxEnabled = prefBool("windowing.sceneFxEnabled", false)
    }

    function activeDesktopPath() {
        return "~/Desktop"
    }

    function sanitizeDesktopBaseName(raw) {
        var name = String(raw || "").trim()
        if (name.length <= 0) {
            name = "Application"
        }
        var forbidden = "\\/:*?\"<>|"
        for (var i = 0; i < forbidden.length; ++i) {
            name = name.split(forbidden.charAt(i)).join("_")
        }
        return name
    }

    function uniqueDesktopLauncherPath(targetDir, baseName) {
        var dir = String(targetDir || "").trim()
        if (dir.length <= 0) {
            dir = "~/Desktop"
        }
        var base = sanitizeDesktopBaseName(baseName)
        var candidate = dir + "/" + base + ".desktop"
        if (typeof FileManagerApi === "undefined" || !FileManagerApi || !FileManagerApi.statPath) {
            return candidate
        }
        var probe = FileManagerApi.statPath(candidate)
        if (!probe || !probe.ok || !probe.exists) {
            return candidate
        }
        for (var i = 2; i <= 9999; ++i) {
            candidate = dir + "/" + base + " (" + i + ").desktop"
            probe = FileManagerApi.statPath(candidate)
            if (!probe || !probe.ok || !probe.exists) {
                return candidate
            }
        }
        return dir + "/" + base + "-launcher.desktop"
    }

    function writeDesktopLauncher(targetDir, appData) {
        if (typeof FileManagerApi === "undefined" || !FileManagerApi || !FileManagerApi.writeTextFile) {
            return false
        }
        var executable = String(appData && appData.executable ? appData.executable : "").trim()
        if (executable.length <= 0) {
            return false
        }
        var name = String(appData && appData.name ? appData.name : "Application").trim()
        if (name.length <= 0) {
            name = "Application"
        }
        var icon = String(appData && appData.iconName ? appData.iconName : "").trim()
        var path = uniqueDesktopLauncherPath(targetDir, name)
        var text = "[Desktop Entry]\n"
        text += "Type=Application\n"
        text += "Name=" + name + "\n"
        text += "Exec=" + executable + "\n"
        if (icon.length > 0) {
            text += "Icon=" + icon + "\n"
        }
        text += "Terminal=false\n"
        text += "NoDisplay=false\n"
        var writeResult = FileManagerApi.writeTextFile(path, text, false)
        return !!(writeResult && writeResult.ok)
    }

    function requestAddToDesktop(appData) {
        if (!appData) {
            return false
        }

        var data = cloneAppEntry(appData)
        if (root.desktopScene
                && root.desktopScene.desktopFileManagerContent
                && root.desktopScene.desktopFileManagerContent.addAppEntryToDesktop) {
            return !!root.desktopScene.desktopFileManagerContent.addAppEntryToDesktop(data, -1, -1)
        }

        var targetDir = activeDesktopPath()
        var added = false

        if (typeof FileManagerApi !== "undefined" && FileManagerApi) {
            if (FileManagerApi.createDirectory) {
                FileManagerApi.createDirectory(targetDir, true)
            }
            var desktopFilePath = String(data.desktopFile || "").trim()
            if (desktopFilePath.length > 0 && FileManagerApi.copyPaths) {
                var copyRes = FileManagerApi.copyPaths([desktopFilePath], targetDir, false)
                added = !!(copyRes && copyRes.ok)
            }
            if (!added) {
                added = writeDesktopLauncher(targetDir, data)
            }
        }

        if (!added && typeof ShortcutModel !== "undefined" && ShortcutModel) {
            if (data.desktopFile && data.desktopFile.length > 0 && ShortcutModel.addDesktopShortcut) {
                added = ShortcutModel.addDesktopShortcut(data.desktopFile)
            }
            if (!added && data.desktopId && data.desktopId.length > 0 && ShortcutModel.addDesktopShortcutById) {
                added = ShortcutModel.addDesktopShortcutById(data.desktopId)
            }
            if (!added && data.executable && data.executable.length > 0 && ShortcutModel.addDesktopShortcut) {
                added = ShortcutModel.addDesktopShortcut(data.executable)
            }
            if (!added && data.name && data.name.length > 0 && ShortcutModel.addDesktopShortcut) {
                added = ShortcutModel.addDesktopShortcut(data.name)
            }
            if (!added && data.executable && data.executable.length > 0 && ShortcutModel.addAppShortcut) {
                added = ShortcutModel.addAppShortcut(data.name || "Application",
                                                     data.executable,
                                                     data.iconName || "")
            }
        }

        return !!added
    }

    function cloneAppEntry(source) {
        var entry = source ? source : ({})
        return {
            "name": String(entry.name || ""),
            "iconSource": String(entry.iconSource || ""),
            "iconName": String(entry.iconName || ""),
            "desktopId": String(entry.desktopId || ""),
            "desktopFile": String(entry.desktopFile || ""),
            "executable": String(entry.executable || "")
        }
    }

    function clampSelectedIndex() {
        var count = pagedApps ? pagedApps.length : 0
        if (count <= 0) {
            selectedIndex = 0
            return
        }
        if (selectedIndex < 0) {
            selectedIndex = 0
        }
        if (selectedIndex >= count) {
            selectedIndex = count - 1
        }
    }

    function activateSelectedApp() {
        var count = pagedApps ? pagedApps.length : 0
        if (count <= 0) {
            return
        }
        root.appChosen(pagedApps[selectedIndex])
    }

    function moveHorizontal(step) {
        var count = pagedApps ? pagedApps.length : 0
        if (count <= 0) {
            return
        }
        var next = selectedIndex + step
        if (next < 0) {
            if (currentPage > 0) {
                selectedIndex = pageSize - 1
                currentPage = currentPage - 1
            } else {
                selectedIndex = 0
            }
            return
        }
        if (next >= count) {
            if (currentPage < totalPages - 1) {
                selectedIndex = 0
                currentPage = currentPage + 1
            } else {
                selectedIndex = count - 1
            }
            return
        }
        selectedIndex = next
    }

    function moveVertical(direction) {
        var count = pagedApps ? pagedApps.length : 0
        if (count <= 0) {
            return
        }
        var step = Math.max(1, effectiveColumns)
        var next = selectedIndex + (direction * step)
        if (next < 0) {
            selectedIndex = 0
            return
        }
        if (next >= count) {
            selectedIndex = count - 1
            return
        }
        selectedIndex = next
    }

    function nextPage() {
        if (totalPages <= 1) {
            return
        }
        pageTransitionDirection = 1
        pageTransitionPending = true
        currentPage = (currentPage + 1) % totalPages
        selectedIndex = 0
    }

    function prevPage() {
        if (totalPages <= 1) {
            return
        }
        pageTransitionDirection = -1
        pageTransitionPending = true
        currentPage = (currentPage - 1 + totalPages) % totalPages
        selectedIndex = 0
    }

    function runPageTransition() {
        var dir = pageTransitionDirection === 0 ? 1 : pageTransitionDirection
        pageTransitionOffset = dir > 0 ? 44 : -44
        pageTransitionOpacity = 0.72
        pageTransitionHeaderOffset = dir > 0 ? -14 : 14
        pageTransitionDotsOffset = dir > 0 ? 12 : -12
        pageTransitionAnim.restart()
    }

    function handleNavigationKey(event) {
        if (event.key === Qt.Key_Escape) {
            root.dismissRequested()
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            activateSelectedApp()
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Right) {
            moveHorizontal(1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Left) {
            moveHorizontal(-1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Down) {
            moveVertical(1)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Up) {
            moveVertical(-1)
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_PageDown) {
            if (totalPages > 1) {
                currentPage = (currentPage + 1) % totalPages
                selectedIndex = 0
            }
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_PageUp) {
            if (totalPages > 1) {
                currentPage = (currentPage - 1 + totalPages) % totalPages
                selectedIndex = 0
            }
            event.accepted = true
        }
    }

    function recomputePaging() {
        if (!appsModel || !appsModel.countMatching || !appsModel.page) {
            filteredCount = 0
            totalPages = 1
            currentPage = 0
            pagedApps = []
            selectedIndex = 0
            return
        }

        filteredCount = appsModel.countMatching(searchText)
        totalPages = Math.max(1, Math.ceil(filteredCount / pageSize))
        if (currentPage >= totalPages) {
            currentPage = totalPages - 1
        }
        if (currentPage < 0) {
            currentPage = 0
        }
        var rawPage = appsModel.page(currentPage, pageSize, searchText)
        var normalized = []
        if (rawPage && rawPage.length !== undefined) {
            for (var i = 0; i < rawPage.length; ++i) {
                var entry = rawPage[i] || {}
                normalized.push({
                    "name": String(entry.name || ""),
                    "iconSource": String(entry.iconSource || ""),
                    "iconName": String(entry.iconName || ""),
                    "desktopId": String(entry.desktopId || ""),
                    "desktopFile": String(entry.desktopFile || ""),
                    "executable": String(entry.executable || ""),
                    "score": Number(entry.score || 0),
                    "launchCount7d": Number(entry.launchCount7d || 0),
                    "fileOpenCount7d": Number(entry.fileOpenCount7d || 0),
                    "lastLaunch": String(entry.lastLaunch || "")
                })
            }
        }
        pagedApps = normalized
        clampSelectedIndex()
    }

    onSearchTextChanged: {
        currentPage = 0
        selectedIndex = 0
        recomputePaging()
    }

    onPageSizeChanged: {
        currentPage = 0
        selectedIndex = 0
        recomputePaging()
    }

    onCurrentPageChanged: {
        recomputePaging()
        if (pageTransitionPending) {
            pageTransitionPending = false
            runPageTransition()
        }
    }

    onVisibleChanged: {
        refreshCompositorBlurPrefs()
        if (visible) {
            recomputePaging()
            searchField.forceActiveFocus()
        }
    }

    Keys.onPressed: function(event) { root.handleNavigationKey(event) }

    // Wallpaper as base layer — same source as the desktop background.
    // Fills the full frame so the dock (visible below via ApplicationWindow)
    // appears seamlessly over the same wallpaper image.
    Image {
        anchors.fill: parent
        source: root.wallpaperSource
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
    }


    MouseArea {
        anchors.fill: parent
        onClicked: root.dismissRequested()
    }

    DragHandler {
        id: pageSwipeDrag
        target: null
        xAxis.enabled: true
        yAxis.enabled: true
        property real startX: 0
        property real startY: 0
        property real lastDx: 0
        property real lastDy: 0
        onActiveChanged: {
            if (active) {
                startX = centroid.position.x
                startY = centroid.position.y
                lastDx = 0
                lastDy = 0
            } else {
                var absDx = Math.abs(lastDx)
                var absDy = Math.abs(lastDy)
                if (absDx >= 90 && absDx > (absDy * 1.2)) {
                    if (lastDx < 0) {
                        root.nextPage()
                    } else {
                        root.prevPage()
                    }
                }
            }
        }
        onCentroidChanged: {
            if (active) {
                lastDx = centroid.position.x - startX
                lastDy = centroid.position.y - startY
            }
        }
    }

    WheelHandler {
        id: pageSwipeWheel
        target: null
        orientation: Qt.Horizontal
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        property real accumX: 0
        onWheel: function(event) {
            accumX += event.pixelDelta.x
            if (Math.abs(accumX) >= 140) {
                if (accumX < 0) {
                    root.nextPage()
                } else {
                    root.prevPage()
                }
                accumX = 0
                event.accepted = true
            }
        }
    }

    Item {
        id: contentLayer
        anchors.fill: parent
        anchors.topMargin: Math.max(root.topSafeInset + 6, 14)
        anchors.bottomMargin: Math.max(
                                  8,
                                  Math.min(Math.round(parent.height * 0.28),
                                           Math.max(Math.round(parent.height * 0.01),
                                                    bottomSafeInset))
                              )
        anchors.leftMargin: Math.max(56, parent.width * 0.07)
        anchors.rightMargin: Math.max(56, parent.width * 0.07)

        Column {
            anchors.fill: parent
            spacing: 8

            Rectangle {
                id: searchShell
                width: Math.min(240, parent.width * 0.22)
                height: 34
                anchors.horizontalCenter: parent.horizontalCenter
                radius: height * 0.5
                color: Theme.color("launchpadSearchBg")
                opacity: root.compositorBlurActive ? Theme.opacityMuted : 0.22
                border.width: Theme.borderWidthThin
                border.color: Qt.rgba(1, 1, 1, 0.18)
                transform: Translate { x: root.pageTransitionHeaderOffset }

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    width: 13
                    height: 13
                    source: "image://themeicon/edit-find-symbolic?v="
                            + ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                               ? ThemeIconController.revision : 0)
                    fillMode: Image.PreserveAspectFit
                    opacity: Theme.opacitySeparator
                }

                TextInput {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 28
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textPrimary")
                    selectionColor: Theme.color("accent")
                    selectedTextColor: Theme.color("accentText")
                    clip: true
                    text: root.searchText
                    onTextChanged: root.searchText = text
                    Keys.onPressed: function(event) { root.handleNavigationKey(event) }
                }

                Label {
                    visible: searchField.text.length === 0
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 28
                    text: "Search"
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textSecondary")
                    opacity: Theme.opacityMuted
                }
            }

            Item {
                width: parent.width
                height: Math.max(160, parent.height - searchShell.height - pageDots.height - 16)

                Flickable {
                    id: appsFlick
                    anchors.fill: parent
                    clip: true
                    contentWidth: width
                    contentHeight: appsGrid.implicitHeight + 12
                    boundsBehavior: Flickable.StopAtBounds

                    Grid {
                        id: appsGrid
                        width: appsFlick.width
                        columns: root.effectiveColumns
                        horizontalItemAlignment: Grid.AlignHCenter
                        spacing: root.gridSpacing
                        opacity: root.pageTransitionOpacity
                        transform: Translate { x: root.pageTransitionOffset }

                        Repeater {
                            model: root.pagedApps

                            delegate: Item {
                                width: root.gridCellWidth
                                height: root.gridCellHeight
                                readonly property var appEntry: modelData
                                readonly property var dragEntryPayload: ({
                                                                               "name": String(appEntry && appEntry.name ? appEntry.name : ""),
                                                                               "desktopId": String(appEntry && appEntry.desktopId ? appEntry.desktopId : ""),
                                                                               "desktopFile": String(appEntry && appEntry.desktopFile ? appEntry.desktopFile : ""),
                                                                               "executable": String(appEntry && appEntry.executable ? appEntry.executable : ""),
                                                                               "iconName": String(appEntry && appEntry.iconName ? appEntry.iconName : ""),
                                                                               "iconSource": String(appEntry && appEntry.iconSource ? appEntry.iconSource : ""),
                                                                               "source": "launchpad"
                                                                           })
                                // Per-row stagger: row N starts revealing after row (N-1) is underway.
                                readonly property int rowIndex: Math.floor(index / Math.max(1, root.effectiveColumns))
                                readonly property real rowDelay: rowIndex * root.staggerDelayPerRow
                                readonly property real rowProgress: rowDelay >= 1.0 ? 0.0
                                    : Math.max(0.0, Math.min(1.0,
                                          (root.revealProgress - rowDelay) / (1.0 - rowDelay)))
                                opacity: rowProgress
                                transform: Translate {
                                    y: (1.0 - rowProgress) * 18
                                }
                                Drag.active: appEntryDragHandler.active
                                Drag.supportedActions: Qt.CopyAction
                                Drag.proposedAction: Qt.CopyAction
                                Drag.hotSpot.x: width * 0.5
                                Drag.hotSpot.y: height * 0.5
                                Drag.mimeData: ({
                                                    "application/x-slm-app-entry": JSON.stringify(dragEntryPayload),
                                                    "application/x-slm-desktop-item": JSON.stringify(dragEntryPayload),
                                                    "text/plain": JSON.stringify(dragEntryPayload)
                                                })

                                Rectangle {
                                    id: iconPlate
                                    width: root.iconPlateSize
                                    height: width
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    y: Math.max(0, Math.round((parent.height - root.iconPlateSize - 8 - root.appLabelHeight) * 0.5))
                                    radius: Theme.radiusMax
                                    readonly property bool selected: root.selectedIndex === index
                                    color: (selected || appMouse.containsMouse) ? Theme.color("launchpadSegmentActive") : "transparent"
                                    border.width: (selected || appMouse.containsMouse) ? Theme.borderWidthThin : Theme.borderWidthNone
                                    border.color: Theme.color("launchpadIconRing")
                                    scale: (selected || appMouse.containsMouse) ? 1.06 : 1.0

                                    Behavior on scale {
                                        NumberAnimation {
                                            duration: Theme.durationFast
                                            easing.type: Theme.easingLight
                                        }
                                    }

                                    Image {
                                        id: appIcon
                                        anchors.centerIn: parent
                                        width: Math.round(parent.width * 0.88)
                                        height: width
                                        source: (appEntry.iconName && appEntry.iconName.length > 0)
                                                ? ("image://themeicon/" + appEntry.iconName + "?v=" +
                                                   ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                                    ? ThemeIconController.revision : 0))
                                                : ((appEntry.iconSource && appEntry.iconSource.length > 0)
                                                       ? appEntry.iconSource
                                                       : "")
                                        fillMode: Image.PreserveAspectFit
                                        visible: true
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        visible: appIcon.status === Image.Error || appIcon.source.toString().length === 0
                                        text: appEntry.name && appEntry.name.length > 0 ? appEntry.name.charAt(0).toUpperCase() : "?"
                                        font.pixelSize: Theme.fontSize("display")
                                        font.weight: Theme.fontWeight("bold")
                                        color: Theme.color("accentText")
                                    }
                                }

                                Label {
                                    anchors.top: iconPlate.bottom
                                    anchors.topMargin: 8
                                    anchors.horizontalCenter: iconPlate.horizontalCenter
                                    width: parent.width
                                    height: root.appLabelHeight
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignTop
                                    elide: Text.ElideRight
                                    text: appEntry.name
                                    color: Theme.color("textOnGlass")
                                    font.pixelSize: Theme.fontSize("menu")
                                    font.weight: Theme.fontWeight("bold")
                                }

                                MouseArea {
                                    id: appMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onEntered: root.selectedIndex = index
                                    onClicked: function(mouse) {
                                        if (appEntryDragHandler.active || appDragClickBlock.running) {
                                            return
                                        }
                                        root.selectedIndex = index
                                        if (mouse.button === Qt.LeftButton) {
                                            root.appChosen(appEntry)
                                        } else if (mouse.button === Qt.RightButton) {
                                            appContextMenu.actionEntrySnapshot = root.cloneAppEntry(appEntry)
                                            appContextMenu.popup()
                                        }
                                    }
                                }

                                DragHandler {
                                    id: appEntryDragHandler
                                    target: null
                                    acceptedButtons: Qt.LeftButton
                                    onActiveChanged: {
                                        if (active) {
                                            root.selectedIndex = index
                                            if (appContextMenu.visible) {
                                                appContextMenu.close()
                                            }
                                        } else {
                                            appDragClickBlock.restart()
                                        }
                                    }
                                }

                                Timer {
                                    id: appDragClickBlock
                                    interval: 160
                                    repeat: false
                                }

                                Menu {
                                    id: appContextMenu
                                    property var quickRows: []
                                    property var actionEntrySnapshot: ({})
                                    onClosed: {
                                        actionEntrySnapshot = ({})
                                        quickRows = []
                                    }
                                    onAboutToShow: {
                                        if (!actionEntrySnapshot || String(actionEntrySnapshot.name || "").length <= 0) {
                                            actionEntrySnapshot = root.cloneAppEntry(appEntry)
                                        }
                                        if (typeof AppModel !== "undefined" && AppModel
                                                && AppModel.slmQuickActionsForEntry) {
                                            quickRows = AppModel.slmQuickActionsForEntry("launcher", {
                                                "desktopId": String(actionEntrySnapshot.desktopId || ""),
                                                "desktopFile": String(actionEntrySnapshot.desktopFile || ""),
                                                "executable": String(actionEntrySnapshot.executable || ""),
                                                "iconName": String(actionEntrySnapshot.iconName || ""),
                                                "iconSource": String(actionEntrySnapshot.iconSource || "")
                                            }) || []
                                        } else if (typeof AppModel !== "undefined" && AppModel && AppModel.slmQuickActions) {
                                            quickRows = AppModel.slmQuickActions("launcher") || []
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
                                                    "scope": "launcher",
                                                    "selection_count": 0,
                                                    "source_app": "org.slm.launchpad",
                                                    "desktop_id": String(appContextMenu.actionEntrySnapshot.desktopId || ""),
                                                    "desktop_file": String(appContextMenu.actionEntrySnapshot.desktopFile || ""),
                                                    "executable": String(appContextMenu.actionEntrySnapshot.executable || "")
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
                                        text: "Tambah ke Dock"
                                        onTriggered: root.addToDockRequested(appContextMenu.actionEntrySnapshot)
                                    }
                                    MenuItem {
                                        text: "Add to Desktop"
                                        onTriggered: {
                                            root.requestAddToDesktop(appContextMenu.actionEntrySnapshot)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Row {
                id: pageDots
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                transform: Translate { x: root.pageTransitionDotsOffset }

                Repeater {
                    model: root.totalPages
                    delegate: Rectangle {
                        width: index === root.currentPage ? 18 : 7
                        height: 7
                        radius: Theme.radiusSm
                        color: index === root.currentPage ? Theme.color("dockSpecLine") : Theme.color("dockBorder")
                        opacity: index === root.currentPage ? Theme.opacityGhost : Theme.opacitySeparator

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        root.pageTransitionDirection = (index >= root.currentPage) ? 1 : -1
                                        root.pageTransitionPending = true
                                        root.currentPage = index
                                        root.selectedIndex = 0
                                    }
                                }
                            }
                }
            }
        }
    }

    Connections {
        target: root.appsModel
        function onModelReset() { root.recomputePaging() }
        function onRowsInserted() { root.recomputePaging() }
        function onRowsRemoved() { root.recomputePaging() }
        function onAppScoresChanged() { root.recomputePaging() }
    }


    Connections {
        target: (typeof DesktopSettings !== "undefined" && DesktopSettings) ? DesktopSettings : null
        ignoreUnknownSignals: true
        function onSettingChanged(path) {
            var k = String(path || "").toLowerCase()
            if (k === "launchpad/compositorblurhint"
                    || k === "launchpad.compositorblurhint"
                    || k === "windowing/scenefxenabled"
                    || k === "windowing.scenefxenabled") {
                root.refreshCompositorBlurPrefs()
            }
        }
    }

    ParallelAnimation {
        id: pageTransitionAnim
        NumberAnimation {
            target: root
            property: "pageTransitionOffset"
            to: 0
            duration: Theme.durationMd
            easing.type: Theme.easingDefault
        }
        NumberAnimation {
            target: root
            property: "pageTransitionOpacity"
            to: 1
            duration: Theme.durationMd
            easing.type: Theme.easingLight
        }
        NumberAnimation {
            target: root
            property: "pageTransitionHeaderOffset"
            to: 0
            duration: Theme.durationNormal
            easing.type: Theme.easingDefault
        }
        NumberAnimation {
            target: root
            property: "pageTransitionDotsOffset"
            to: 0
            duration: Theme.durationNormal
            easing.type: Theme.easingDefault
        }
    }

    Component.onCompleted: refreshCompositorBlurPrefs()
}
