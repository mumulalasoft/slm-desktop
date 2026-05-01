import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as AppDeckComp

Item {
    id: root

    property var appModel: null
    property string filterText: ""
    property var filteredApps: []
    property var pagedApps: []
    property int selectedIndex: -1
    property int currentPage: 0
    property int pendingPage: -1
    property int pendingPageDirection: 0
    property real pageShiftX: 0
    property bool pinFlightVisible: false
    property string pinFlightIcon: ""
    property real pinFlightX: 0
    property real pinFlightY: 0
    property real pinFlightMidY: 0
    property real pinFlightTargetX: 0
    property real pinFlightTargetY: 0
    property real pinFlightScale: 1
    property real pinFlightOpacity: 0
    property real pinBeaconScale: 0
    property real pinBeaconOpacity: 0

    readonly property int filteredCount: filteredApps.length
    readonly property bool hasResults: filteredApps.length > 0
    readonly property bool noResultState: !hasResults && String(filterText || "").trim().length > 0
    readonly property bool emptyState: !hasResults && String(filterText || "").trim().length === 0
    readonly property int minCellWidth: 112
    readonly property int maxCellWidth: 148
    readonly property int gridSpacing: 14
    readonly property int gridSideInset: 6
    readonly property int footerHeight: 24
    readonly property bool showPagination: pageCount > 1
    readonly property real usableGridWidth: Math.max(1, grid.width - (gridSideInset * 2))
    readonly property real usableGridHeight: Math.max(1, grid.height)
    readonly property int columnCount: Math.max(
                                           1,
                                           Math.floor((usableGridWidth + gridSpacing)
                                                      / (Math.max(1, minCellWidth) + gridSpacing))
                                       )
    readonly property int rowCount: Math.max(1, Math.floor(usableGridHeight / Math.max(1, grid.cellHeight)))
    readonly property int pageSize: Math.max(1, columnCount * rowCount)
    readonly property int pageCount: Math.max(1, Math.ceil(filteredApps.length / Math.max(1, pageSize)))
    readonly property bool hasPreviousPage: currentPage > 0
    readonly property bool hasNextPage: currentPage + 1 < pageCount
    readonly property int pageButtonSize: 34
    readonly property string pageRangeText: {
        if (!showPagination || filteredApps.length <= 0) {
            return ""
        }
        return String(pageStart(currentPage) + 1)
                + "-"
                + String(pageEnd(currentPage))
                + " of "
                + String(filteredApps.length)
    }
    readonly property real pageSwitchOffset: Math.max(28, Math.round(grid.width * 0.16))
    readonly property real swipeThreshold: Math.max(36, grid.width * 0.14)

    signal appActivated(var appData)
    signal collapseRequested()
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    function _iconFrom(entry) {
        var iconName = String(entry && entry.iconName ? entry.iconName : "")
        if (iconName.length > 0) {
            var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                       ? ThemeIconController.revision : 0)
            return "image://themeicon/" + iconName + "?v=" + rev
        }
        return String(entry && (entry.icon || entry.iconSource) ? (entry.icon || entry.iconSource) : "")
    }

    function _normalize(entry) {
        return {
            "appId": String((entry && (entry.appId || entry.desktopId || entry.desktopFile || entry.executable || entry.name)) || ""),
            "display": String((entry && (entry.display || entry.name)) || ""),
            "icon": _iconFrom(entry),
            "running": !!(entry && entry.running),
            "favorite": !!(entry && entry.favorite),
            "category": String((entry && entry.category) || ""),
            "desktopId": String((entry && entry.desktopId) || ""),
            "desktopFile": String((entry && entry.desktopFile) || ""),
            "executable": String((entry && entry.executable) || ""),
            "name": String((entry && entry.name) || "")
        }
    }

    function _matchesFilter(entry, needle) {
        if (needle.length <= 0) {
            return true
        }
        var title = String((entry && (entry.display || entry.name)) || "").toLowerCase()
        var category = String((entry && entry.category) || "").toLowerCase()
        return title.indexOf(needle) >= 0 || category.indexOf(needle) >= 0
    }

    function _loadAppsByPage(needle) {
        if (!appModel || !appModel.page) {
            return []
        }
        var query = String(needle || "")
        var count = 0
        if (appModel.countMatching) {
            count = Math.max(0, Number(appModel.countMatching(query) || 0))
        } else if (typeof appModel.count !== "undefined") {
            count = Math.max(0, Number(appModel.count || 0))
        }
        if (count <= 0) {
            return []
        }
        var rows = appModel.page(0, count, query)
        return (rows && rows.length !== undefined) ? rows : []
    }

    function _loadAppsByIterating(needle) {
        if (!appModel || !appModel.get || typeof appModel.count === "undefined") {
            return []
        }
        var rows = []
        var q = String(needle || "")
        for (var i = 0; i < Number(appModel.count || 0); ++i) {
            var row = appModel.get(i)
            if (!row || !_matchesFilter(row, q)) {
                continue
            }
            rows.push(row)
        }
        return rows
    }

    function _pageForGlobalIndex(globalIndex) {
        if (globalIndex < 0) {
            return 0
        }
        return Math.floor(globalIndex / Math.max(1, pageSize))
    }

    function _updatePagedApps() {
        var size = Math.max(1, pageSize)
        var start = Math.max(0, Math.min(filteredApps.length, currentPage * size))
        var end = Math.max(start, Math.min(filteredApps.length, start + size))
        var next = []
        for (var i = start; i < end; ++i) {
            next.push(filteredApps[i])
        }
        pagedApps = next
        var localIndex = selectedIndex - start
        grid.currentIndex = (localIndex >= 0 && localIndex < next.length) ? localIndex : -1
    }

    function _ensureSelectionOnCurrentPage() {
        if (pagedApps.length <= 0) {
            grid.currentIndex = -1
            return
        }
        var start = currentPage * Math.max(1, pageSize)
        var endExclusive = start + pagedApps.length
        if (selectedIndex < start || selectedIndex >= endExclusive) {
            selectedIndex = start
        }
        grid.currentIndex = selectedIndex - start
    }

    function switchToPage(pageIndex, animated, directionHint) {
        if (filteredApps.length <= 0) {
            return false
        }
        var clamped = Math.max(0, Math.min(pageCount - 1, pageIndex))
        if (clamped === currentPage) {
            pageShiftX = 0
            _updatePagedApps()
            return false
        }
        if (animated) {
            pendingPage = clamped
            pendingPageDirection = (directionHint !== 0)
                                   ? directionHint
                                   : (clamped > currentPage ? 1 : -1)
            pageSwitchAnim.restart()
            return true
        }
        currentPage = clamped
        pageShiftX = 0
        _updatePagedApps()
        _ensureSelectionOnCurrentPage()
        return true
    }

    function pageStart(pageIndex) {
        return Math.max(0, Math.min(filteredApps.length, pageIndex * Math.max(1, pageSize)))
    }

    function pageEnd(pageIndex) {
        return Math.max(pageStart(pageIndex),
                        Math.min(filteredApps.length, pageStart(pageIndex) + Math.max(1, pageSize)))
    }

    function _syncPaginationToSelection() {
        if (filteredApps.length <= 0) {
            currentPage = 0
            pagedApps = []
            grid.currentIndex = -1
            return
        }
        var targetPage = Math.max(0, Math.min(pageCount - 1, _pageForGlobalIndex(selectedIndex)))
        currentPage = targetPage
        pageShiftX = 0
        _updatePagedApps()
        _ensureSelectionOnCurrentPage()
    }

    function _rebuildModel() {
        var needle = String(filterText || "").trim().toLowerCase()
        var raw = _loadAppsByPage(needle)
        if (raw.length <= 0) {
            raw = _loadAppsByIterating(needle)
        }
        var next = []
        for (var i = 0; i < raw.length; ++i) {
            next.push(_normalize(raw[i]))
        }
        filteredApps = next
        if (filteredApps.length <= 0) {
            selectedIndex = -1
            currentPage = 0
            pagedApps = []
            pageShiftX = 0
            grid.currentIndex = -1
            return
        }
        if (selectedIndex < 0 || selectedIndex >= filteredApps.length) {
            selectedIndex = 0
        }
        _syncPaginationToSelection()
    }

    function _pinFlightSize() {
        return Theme.controlHeightLarge + Theme.spacingXxl
    }

    function _dockTargetPoint() {
        var s = _pinFlightSize()
        return {
            "x": Math.round((width - s) * 0.5),
            "y": Math.max(0, Math.round(height - s - Theme.spacingXxl))
        }
    }

    function playPinToDockFlight(sourceItem, appData) {
        if (!sourceItem || !sourceItem.mapToItem) {
            return
        }
        var s = _pinFlightSize()
        var origin = sourceItem.mapToItem(root,
                                          Math.round(sourceItem.width * 0.5),
                                          Math.round(sourceItem.height * 0.5))
        var target = _dockTargetPoint()
        pinFlightIcon = String((appData && appData.icon) || "")
        pinFlightX = Math.round(origin.x - (s * 0.5))
        pinFlightY = Math.round(origin.y - (s * 0.5))
        pinFlightTargetX = target.x
        pinFlightTargetY = target.y
        pinFlightMidY = Math.max(0, Math.min(pinFlightY, pinFlightTargetY) - Math.max(Theme.spacingXxl, Math.round(height * 0.08)))
        pinFlightScale = 1.0 + (Theme.opacitySubtle * 0.5)
        pinFlightOpacity = Theme.opacitySurfaceStrong
        pinBeaconScale = Theme.opacityMuted
        pinBeaconOpacity = 0
        pinFlightVisible = true
        pinFlightAnim.stop()
        pinFlightAnim.start()
    }

    function activateIndex(indexValue) {
        if (indexValue < 0 || indexValue >= filteredApps.length) {
            return
        }
        selectedIndex = indexValue
        switchToPage(_pageForGlobalIndex(indexValue), false, 0)
        grid.currentIndex = indexValue - (currentPage * Math.max(1, pageSize))
        appActivated(filteredApps[indexValue])
    }

    onPageSizeChanged: _syncPaginationToSelection()
    onPageCountChanged: {
        if (filteredApps.length <= 0) {
            return
        }
        if (currentPage >= pageCount) {
            currentPage = Math.max(0, pageCount - 1)
        }
        _updatePagedApps()
    }

    onFilterTextChanged: _rebuildModel()
    onAppModelChanged: _rebuildModel()
    onVisibleChanged: if (visible) _rebuildModel()

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            collapseRequested()
            event.accepted = true
        }
    }

    Connections {
        target: root.appModel ? root.appModel : null
        ignoreUnknownSignals: true
        function onCountChanged() { root._rebuildModel() }
        function onDataChanged() { root._rebuildModel() }
        function onModelReset() { root._rebuildModel() }
        function onRowsInserted() { root._rebuildModel() }
        function onRowsRemoved() { root._rebuildModel() }
    }

    SequentialAnimation {
        id: pageSwitchAnim
        NumberAnimation {
            target: root
            property: "pageShiftX"
            to: -root.pendingPageDirection * root.pageSwitchOffset
            duration: Theme.durationFast
            easing.type: Theme.easingLight
        }
        ScriptAction {
            script: {
                root.currentPage = root.pendingPage
                root._updatePagedApps()
                root._ensureSelectionOnCurrentPage()
                root.pageShiftX = root.pendingPageDirection * root.pageSwitchOffset
            }
        }
        NumberAnimation {
            target: root
            property: "pageShiftX"
            to: 0
            duration: Theme.durationNormal
            easing.type: Theme.easingDecelerate
        }
        onStopped: {
            root.pendingPage = -1
            root.pendingPageDirection = 0
            root.pageShiftX = 0
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Item {
            Layout.fillWidth: true
            implicitHeight: Math.max(headerLabel.implicitHeight, countLabel.implicitHeight)

            Label {
                id: headerLabel
                text: "Applications"
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("semibold")
                color: Theme.color("textPrimary")
                opacity: Theme.opacityHint
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                id: countLabel
                text: String(root.filteredCount) + (root.filteredCount === 1 ? " app" : " apps")
                font.pixelSize: Theme.fontSize("small")
                color: Theme.color("textSecondary")
                opacity: Theme.opacityMuted
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: root.collapseRequested()
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            x: root.pageShiftX
            model: root.pagedApps
            cellWidth: Math.max(
                       root.minCellWidth,
                       Math.min(root.maxCellWidth,
                                Math.floor((root.usableGridWidth - Math.max(0, root.columnCount - 1) * root.gridSpacing)
                                           / Math.max(1, root.columnCount)))
                   )
            cellHeight: 132
            leftMargin: root.gridSideInset
            rightMargin: root.gridSideInset
            interactive: false
            currentIndex: -1
            focus: true
            keyNavigationWraps: false
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                width: grid.cellWidth
                height: grid.cellHeight
                readonly property int globalIndex: (root.currentPage * Math.max(1, root.pageSize)) + index

                AppDeckComp.AppDeckItemDelegate {
                    id: delegateItem
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(96, grid.cellWidth - 8)
                    height: grid.cellHeight - 10
                    appData: modelData
                    title: String((modelData && modelData.display) || "")
                    iconSource: String((modelData && modelData.icon) || "")
                    running: !!(modelData && modelData.running)
                    selected: root.selectedIndex === parent.globalIndex
                    onHovered: {
                        root.selectedIndex = parent.globalIndex
                        grid.currentIndex = index
                    }
                    onActivated: function(appData) { root.activateIndex(parent.globalIndex) }
                    onContextMenuRequested: function(appData) {
                        menu.actionAppData = appData || ({})
                        console.log("[appdeck] context menu requested app=", JSON.stringify(menu.actionAppData))
                        menu.popup()
                    }
                }

                Menu {
                    id: menu
                    property var actionAppData: ({})
                    MenuItem {
                        text: "Pin to AppDeck"
                        onTriggered: {
                            var payload = menu.actionAppData || modelData || ({})
                            console.log("[appdeck] pin triggered payload=", JSON.stringify(payload))
                            root.playPinToDockFlight(delegateItem, payload)
                            root.addToDockRequested(payload)
                        }
                    }
                    MenuItem {
                        text: "Add to Desktop"
                        onTriggered: {
                            var payload = menu.actionAppData || modelData || ({})
                            root.addToDesktopRequested(payload)
                        }
                    }
                }
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    var globalIndex = (root.currentPage * Math.max(1, root.pageSize)) + grid.currentIndex
                    root.activateIndex(globalIndex)
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Left) {
                    if (grid.currentIndex > 0) {
                        return
                    }
                    if (root.switchToPage(root.currentPage - 1, true, -1)) {
                        event.accepted = true
                    }
                    return
                }
                if (event.key === Qt.Key_Right) {
                    if (grid.currentIndex + 1 < root.pagedApps.length) {
                        return
                    }
                    if (root.switchToPage(root.currentPage + 1, true, 1)) {
                        event.accepted = true
                    }
                    return
                }
                if (event.key === Qt.Key_Escape) {
                    root.collapseRequested()
                    event.accepted = true
                }
            }

            DragHandler {
                id: pageSwipe
                target: null
                enabled: root.pageCount > 1 && !pageSwitchAnim.running
                xAxis.enabled: true
                onTranslationChanged: {
                    if (!active) {
                        return
                    }
                    if (Math.abs(translation.y) > Math.abs(translation.x) * 0.85) {
                        root.pageShiftX = 0
                        return
                    }
                    var damped = translation.x * 0.35
                    if ((damped > 0 && !root.hasPreviousPage) || (damped < 0 && !root.hasNextPage)) {
                        damped = damped * 0.4
                    }
                    root.pageShiftX = Math.max(-root.pageSwitchOffset, Math.min(root.pageSwitchOffset, damped))
                }
                onActiveChanged: {
                    if (active) {
                        return
                    }
                    var deltaX = translation.x
                    root.pageShiftX = 0
                    if (Math.abs(deltaX) < root.swipeThreshold) {
                        return
                    }
                    if (deltaX < 0) {
                        root.switchToPage(root.currentPage + 1, true, 1)
                    } else {
                        root.switchToPage(root.currentPage - 1, true, -1)
                    }
                }
            }

            MouseArea {
                id: emptyGridClickArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                z: 99
                preventStealing: true
                propagateComposedEvents: true
                onPressed: function(mouse) {
                    var idx = grid.indexAt(mouse.x + grid.contentX, mouse.y + grid.contentY)
                    mouse.accepted = (idx < 0)
                }
                onClicked: function(mouse) {
                    root.collapseRequested()
                    mouse.accepted = true
                }
            }
        }

        Item {
            id: paginationRow
            Layout.fillWidth: true
            Layout.preferredHeight: root.showPagination ? root.pageButtonSize : 0
            Layout.alignment: Qt.AlignHCenter
            visible: root.showPagination
            opacity: root.showPagination ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDecelerate }
            }

            Rectangle {
                id: previousButton
                width: root.pageButtonSize
                height: root.pageButtonSize
                radius: Math.min(8, Theme.radiusControl)
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: previousHover.containsMouse && root.hasPreviousPage
                       ? Theme.color("controlBgHover")
                       : Theme.color("controlBg")
                border.width: Theme.borderWidthThin
                border.color: root.hasPreviousPage ? Theme.color("panelBorder") : Theme.color("controlDisabledBorder")
                opacity: root.hasPreviousPage ? 1.0 : 0.45

                Label {
                    anchors.centerIn: parent
                    text: "<"
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("bold")
                    color: root.hasPreviousPage ? Theme.color("textPrimary") : Theme.color("textDisabled")
                }

                MouseArea {
                    id: previousHover
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: root.hasPreviousPage
                    acceptedButtons: Qt.LeftButton
                    onClicked: root.switchToPage(root.currentPage - 1, true, -1)
                }
            }

            Row {
                anchors.centerIn: parent
                spacing: 7

                Repeater {
                    model: root.pageCount
                    delegate: Item {
                        width: index === root.currentPage ? 20 : 12
                        height: root.footerHeight

                        Rectangle {
                            anchors.centerIn: parent
                            width: index === root.currentPage ? 18 : 6
                            height: 6
                            radius: height * 0.5
                            color: index === root.currentPage ? Theme.color("accent") : Theme.color("textSecondary")
                            opacity: index === root.currentPage ? Theme.opacityHint : Theme.opacityFaint

                            Behavior on width {
                                NumberAnimation {
                                    duration: Theme.durationFast
                                    easing.type: Theme.easingDecelerate
                                }
                            }
                            Behavior on opacity {
                                NumberAnimation {
                                    duration: Theme.durationFast
                                    easing.type: Theme.easingLight
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            onClicked: {
                                if (index !== root.currentPage) {
                                    root.switchToPage(index, true, index > root.currentPage ? 1 : -1)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: nextButton
                width: root.pageButtonSize
                height: root.pageButtonSize
                radius: Math.min(8, Theme.radiusControl)
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: nextHover.containsMouse && root.hasNextPage
                       ? Theme.color("controlBgHover")
                       : Theme.color("controlBg")
                border.width: Theme.borderWidthThin
                border.color: root.hasNextPage ? Theme.color("panelBorder") : Theme.color("controlDisabledBorder")
                opacity: root.hasNextPage ? 1.0 : 0.45

                Label {
                    anchors.centerIn: parent
                    text: ">"
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("bold")
                    color: root.hasNextPage ? Theme.color("textPrimary") : Theme.color("textDisabled")
                }

                MouseArea {
                    id: nextHover
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: root.hasNextPage
                    acceptedButtons: Qt.LeftButton
                    onClicked: root.switchToPage(root.currentPage + 1, true, 1)
                }
            }

            Label {
                anchors.right: nextButton.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: root.pageRangeText
                font.pixelSize: Theme.fontSize("small")
                color: Theme.color("textSecondary")
                opacity: Theme.opacityMuted
            }
        }
    }

    Item {
        id: pinFlightLayer
        anchors.fill: parent
        z: 100
        visible: root.pinFlightVisible

        Rectangle {
            id: pinFlight
            width: root._pinFlightSize()
            height: width
            x: root.pinFlightX
            y: root.pinFlightY
            radius: Theme.radiusCard
            opacity: root.pinFlightOpacity
            scale: root.pinFlightScale
            color: Theme.color("windowCard")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("windowCardBorder")

            Image {
                anchors.fill: parent
                source: root.pinFlightIcon
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: false
            }
        }

        Rectangle {
            id: pinTargetBeacon
            width: Math.round(root._pinFlightSize() * 0.76)
            height: width
            x: Math.round(root.pinFlightTargetX + (root._pinFlightSize() - width) * 0.5)
            y: Math.round(root.pinFlightTargetY + (root._pinFlightSize() - height) * 0.5)
            radius: width * 0.5
            color: "transparent"
            border.width: Theme.borderWidthThick
            border.color: Theme.color("accent")
            opacity: root.pinBeaconOpacity
            scale: root.pinBeaconScale
        }

        ParallelAnimation {
            id: pinFlightAnim
            NumberAnimation {
                target: root
                property: "pinFlightX"
                to: root.pinFlightTargetX
                duration: Theme.durationNormal + Theme.durationMd
                easing.type: Theme.easingDecelerate
            }
            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "pinFlightY"
                    to: root.pinFlightMidY
                    duration: Theme.durationMd
                    easing.type: Theme.easingLight
                }
                NumberAnimation {
                    target: root
                    property: "pinFlightY"
                    to: root.pinFlightTargetY
                    duration: Theme.durationLg
                    easing.type: Theme.easingDecelerate
                }
            }
            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "pinFlightScale"
                    to: 1.0 + Theme.opacitySubtle
                    duration: Theme.durationMd
                    easing.type: Theme.easingLight
                }
                NumberAnimation {
                    target: root
                    property: "pinFlightScale"
                    to: Theme.opacitySubtle + Theme.opacityFaint + Theme.opacitySubtle
                    duration: Theme.durationLg
                    easing.type: Theme.easingDefault
                }
            }
            NumberAnimation {
                target: root
                property: "pinBeaconOpacity"
                to: Theme.opacityHint
                duration: Theme.durationFast
                easing.type: Theme.easingDecelerate
            }
            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "pinFlightOpacity"
                    to: Theme.opacityHint
                    duration: Theme.durationMd
                    easing.type: Theme.easingLight
                }
                NumberAnimation {
                    target: root
                    property: "pinFlightOpacity"
                    to: 0
                    duration: Theme.durationMd
                    easing.type: Theme.easingLight
                }
            }
            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "pinBeaconScale"
                    to: 1.0
                    duration: Theme.durationMd
                    easing.type: Theme.easingLight
                }
                NumberAnimation {
                    target: root
                    property: "pinBeaconScale"
                    to: 1.0 + Theme.opacitySubtle + Theme.opacityFaint
                    duration: Theme.durationLg
                    easing.type: Theme.easingDecelerate
                }
            }
            SequentialAnimation {
                PauseAnimation { duration: Theme.durationMd }
                NumberAnimation {
                    target: root
                    property: "pinBeaconOpacity"
                    to: 0
                    duration: Theme.durationLg
                    easing.type: Theme.easingLight
                }
            }
            onStopped: {
                root.pinFlightVisible = false
                root.pinFlightOpacity = 0
                root.pinFlightScale = 1
                root.pinBeaconScale = 0
                root.pinBeaconOpacity = 0
            }
        }
    }

    Component.onCompleted: _rebuildModel()
}
