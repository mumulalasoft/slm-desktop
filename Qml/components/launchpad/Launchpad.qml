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
    property int maxRowsPerPage: 5
    property int bottomSafeInset: 144
    property int minColumns: 4
    property int minRowsPerPage: 3
    readonly property int effectiveColumns: Math.max(minColumns,
                                                     Math.min(maxColumns,
                                                              Math.floor((appsGrid.width + gridSpacing)
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

    readonly property real gridSpacing: 16
    readonly property real minCellWidth: 122
    readonly property real appLabelHeight: Math.ceil(Theme.fontSize("menu") * 1.8)
    readonly property real appCellMinHeight: 114 + 10 + appLabelHeight + 8
    readonly property real gridCellWidth: Math.max(minCellWidth,
                                                   Math.floor((appsGrid.width - (gridSpacing * (effectiveColumns - 1)))
                                                              / Math.max(1, effectiveColumns)))
    readonly property real gridCellHeight: Math.max(appCellMinHeight,
                                                    Math.floor((appsFlick.height
                                                                - (gridSpacing * (effectiveRowsPerPage - 1)))
                                                               / Math.max(1, effectiveRowsPerPage)))

    function prefBool(key, fallback) {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return !!fallback
        }
        return !!UIPreferences.getPreference(key, fallback)
    }

    function refreshCompositorBlurPrefs() {
        compositorBlurHint = prefBool("launchpad.compositorBlurHint", true)
        compositorSceneFxEnabled = prefBool("windowing.sceneFxEnabled", false)
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
        pagedApps = appsModel.page(currentPage, pageSize, searchText)
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
        if (visible && appsModel && appsModel.refresh) {
            appsModel.refresh()
        }
        refreshCompositorBlurPrefs()
        if (visible) {
            recomputePaging()
            searchField.forceActiveFocus()
        }
    }

    Keys.onPressed: function(event) { root.handleNavigationKey(event) }

    Rectangle {
        anchors.fill: parent
        color: Theme.color("launchpadOrbTop")
        opacity: root.compositorBlurActive ? Theme.opacityFaint : Theme.cardSurfaceOpacity
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.color("launchpadSearchBg") }
            GradientStop { position: 0.55; color: Theme.color("launchpadOrbTop") }
            GradientStop { position: 1.0; color: Theme.color("launchpadOrbBottom") }
        }
        opacity: root.compositorBlurActive ? Theme.opacitySubtle : Theme.opacityMuted
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color("overlay")
        opacity: root.compositorBlurActive ? Theme.opacitySubtle : Theme.opacityFaint
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
        anchors.topMargin: Math.max(18, parent.height * 0.02)
        anchors.bottomMargin: Math.max(Math.max(20, parent.height * 0.02), bottomSafeInset)
        anchors.leftMargin: Math.max(36, parent.width * 0.035)
        anchors.rightMargin: Math.max(36, parent.width * 0.035)

        Column {
            anchors.fill: parent
            spacing: 16

            Rectangle {
                id: searchShell
                width: Math.min(640, parent.width * 0.58)
                height: 56
                anchors.horizontalCenter: parent.horizontalCenter
                radius: Theme.radiusXl
                color: Theme.color("launchpadSearchBg")
                opacity: root.compositorBlurActive ? Theme.opacityIconMuted : Theme.opacityGhost
                border.width: Theme.borderWidthThin
                border.color: Theme.color("launchpadSearchBorder")
                transform: Translate { x: root.pageTransitionHeaderOffset }

                Label {
                    text: "\u2315"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    font.pixelSize: Theme.fontSize("title")
                    color: Theme.color("textSecondary")
                }

                TextInput {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 44
                    anchors.rightMargin: 16
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: Theme.fontSize("title")
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
                    anchors.leftMargin: 44
                    text: "Search"
                    font.pixelSize: Theme.fontSize("title")
                    color: Theme.color("textSecondary")
                    opacity: Theme.opacityMuted
                }
            }

            Item {
                width: parent.width
                height: parent.height - searchShell.height - pageDots.height - 16

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

                                Rectangle {
                                    id: iconPlate
                                    width: 114
                                    height: 114
                                    anchors.horizontalCenter: parent.horizontalCenter
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
                                        width: 86
                                        height: 86
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
                                    anchors.topMargin: 10
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
                                        root.selectedIndex = index
                                        if (mouse.button === Qt.LeftButton) {
                                            root.appChosen(appEntry)
                                        } else if (mouse.button === Qt.RightButton) {
                                            appContextMenu.popup()
                                        }
                                    }
                                }

                                Menu {
                                    id: appContextMenu
                                    property var quickRows: []
                                    onAboutToShow: {
                                        if (typeof AppModel !== "undefined" && AppModel
                                                && AppModel.slmQuickActionsForEntry) {
                                            quickRows = AppModel.slmQuickActionsForEntry("launcher", {
                                                "desktopId": String(appEntry.desktopId || ""),
                                                "desktopFile": String(appEntry.desktopFile || ""),
                                                "executable": String(appEntry.executable || ""),
                                                "iconName": String(appEntry.iconName || ""),
                                                "iconSource": String(appEntry.iconSource || "")
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
                                                    "desktop_id": String(appEntry.desktopId || ""),
                                                    "desktop_file": String(appEntry.desktopFile || ""),
                                                    "executable": String(appEntry.executable || "")
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
                                        onTriggered: root.addToDockRequested(appEntry)
                                    }
                                    MenuItem {
                                        text: "Add to Desktop"
                                        onTriggered: root.addToDesktopRequested(appEntry)
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
        target: (typeof UIPreferences !== "undefined" && UIPreferences) ? UIPreferences : null
        ignoreUnknownSignals: true
        function onPreferenceChanged(key, value) {
            var k = String(key || "").toLowerCase()
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
