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
    // docs/APPDECK_REDESIGN.md Phase 1 — Recent and Suggestions strips above
    // the paged "All Apps" grid. recentApps = top MRU from AppModel.frequentApps;
    // suggestionApps = next frequent apps not already in Recent. Phase 2 will
    // refine suggestions with time-of-day category bucketing.
    property var recentApps: []
    property var suggestionApps: []
    readonly property int recentCount: 6
    readonly property int suggestionCount: 4
    readonly property int stripCellWidth: 96
    readonly property int stripHeight: 100
    readonly property int stripSpacing: 8

    // docs/APPDECK_REDESIGN.md Phase 2 — Category segmented control.
    // 5 fixed buckets; XDG Categories= tokens map to a bucket via
    // _categoryBucket() with priority order Internet → Graphics & Media →
    // Productivity → Utilities (most-specific-first). Apps with no recognized
    // category appear only under "all".
    property string selectedCategory: "all"
    readonly property var categoryButtons: [
        { "id": "all",            "label": "All" },
        { "id": "productivity",   "label": "Productivity" },
        { "id": "internet",       "label": "Internet" },
        { "id": "graphics-media", "label": "Graphics & Media" },
        { "id": "utilities",      "label": "Utilities" }
    ]
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
    // docs/APPDECK.md §7 — when true, IconMorphLayer renders the grid icons
    // from a sibling surface-space layer; this view keeps cell layout for
    // pagination + keyboard navigation but skips the icon paint inside each
    // cell. Tahap 4 wires the property — visual swap completes when the
    // feature flag (AppDeckTokens.iconMorphEnabled) is enabled.
    property bool iconsRenderedExternally: false

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
    // HIG-style chevron hit-target: 36px circle, borderless. The visual
    // chevron glyph is the system "go-previous/next-symbolic" icon at 16px,
    // the rest is padding so the touch/click area stays comfortable.
    readonly property int pageButtonSize: 36
    readonly property string iconRev: (typeof ThemeIconController !== "undefined"
                                       && ThemeIconController)
                                      ? ("?v=" + ThemeIconController.revision) : ""
    readonly property real pageSwitchOffset: Math.max(28, Math.round(grid.width * 0.16))
    // Lowered threshold (was 14% of width / 36px floor) so the swipe gesture
    // commits with a shorter flick, matching iOS-style page turn responsiveness.
    readonly property real swipeThreshold: Math.max(28, grid.width * 0.10)

    signal appActivated(var appData)
    signal collapseRequested()
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    // docs/APPDECK.md §5 — emitted when the grid's cell layout (columnCount,
    // rowCount, cellWidth, currentPage) is stable enough that AppDeckGeometry
    // can sample positions. Consumers (AppDeckGeometry, IconMorphLayer) should
    // call gridIconCenterFor() after this fires.
    signal gridLayoutSettled()

    // docs/APPDECK.md §7 — return the center of the icon at globalIndex in
    // GridView-root coordinates, or Qt.point(-1, -1) if the icon is not on the
    // current page (only icons on the current page have a stable on-screen
    // position to morph to/from). Caller is responsible for mapToItem() into
    // surface space.
    function gridIconCenterFor(globalIndex) {
        if (globalIndex < 0 || columnCount <= 0 || pageSize <= 0) {
            return Qt.point(-1, -1)
        }
        var pageStart = currentPage * pageSize
        if (globalIndex < pageStart || globalIndex >= pageStart + pageSize) {
            return Qt.point(-1, -1)
        }
        var local = globalIndex - pageStart
        var col = local % columnCount
        var row = Math.floor(local / columnCount)
        var cx = grid.x + grid.leftMargin + col * grid.cellWidth + grid.cellWidth / 2
        var cy = grid.y + row * grid.cellHeight + grid.cellHeight / 2
        return Qt.point(cx, cy)
    }

    onColumnCountChanged: gridLayoutSettled()
    onCurrentPageChanged: gridLayoutSettled()

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
            // docs/APPDECK_REDESIGN.md Phase 2 — XDG Categories list from
            // .desktop file, exposed by DesktopAppModel.page()/frequentApps().
            "categories": (entry && entry.categories) ? entry.categories : [],
            "desktopId": String((entry && entry.desktopId) || ""),
            "desktopFile": String((entry && entry.desktopFile) || ""),
            "executable": String((entry && entry.executable) || ""),
            "name": String((entry && entry.name) || "")
        }
    }

    // docs/APPDECK_REDESIGN.md Phase 2 — Bucket an app into one of 5 fixed
    // categories by inspecting its XDG Categories= tokens. Priority order:
    // Internet (most specific for comms apps) → Graphics & Media →
    // Productivity (broad office/dev) → Utilities (catch-all system tools).
    // Mail app with Office;Network;Email; resolves to Internet because
    // Internet's check runs first.
    function _categoryBucket(categories) {
        var cats = []
        var src = categories || []
        for (var k = 0; k < src.length; ++k) {
            cats.push(String(src[k] || "").trim().toLowerCase())
        }
        if (cats.length === 0) {
            return "uncategorized"
        }
        var rules = [
            { id: "internet",
              tokens: ["network", "webbrowser", "email", "chat", "instantmessaging",
                       "ircclient", "telephony", "videoconference", "filetransfer",
                       "remoteaccess", "p2p", "news", "rss"] },
            { id: "graphics-media",
              tokens: ["graphics", "photography", "scanning", "vectorgraphics",
                       "rastergraphics", "imageprocessing", "3dgraphics",
                       "audiovideo", "audio", "video", "music", "player",
                       "recorder", "tv"] },
            { id: "productivity",
              tokens: ["office", "development", "ide", "texteditor",
                       "projectmanagement", "spreadsheet", "presentation",
                       "wordprocessor", "database", "calendar", "dictionary"] },
            { id: "utilities",
              tokens: ["settings", "system", "utility", "accessibility",
                       "filemanager", "calculator", "terminal", "consoleonly",
                       "hardwaresettings", "monitor", "security",
                       "packagemanager"] }
        ]
        for (var r = 0; r < rules.length; ++r) {
            var rule = rules[r]
            for (var c = 0; c < cats.length; ++c) {
                if (rule.tokens.indexOf(cats[c]) >= 0) {
                    return rule.id
                }
            }
        }
        return "uncategorized"
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
        // Phase 2 — segmented category filter. While the user is typing a
        // local filter we keep "all" so search isn't masked by the active
        // segment; Phase 4 will route search to Pulse instead.
        if (selectedCategory !== "all" && needle.length === 0) {
            next = next.filter(function(entry) {
                return _categoryBucket(entry && entry.categories) === selectedCategory
            })
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

    // Identity key used to dedupe between Recent and Suggestions strips.
    function _identityKey(entry) {
        if (!entry) return ""
        var id = String(entry.desktopId || entry.desktopFile || entry.executable || entry.name || "")
        return id.toLowerCase()
    }

    function _rebuildRecentApps() {
        if (typeof AppModel === "undefined" || !AppModel || !AppModel.frequentApps) {
            recentApps = []
            return
        }
        var raw = AppModel.frequentApps(recentCount) || []
        var next = []
        for (var i = 0; i < raw.length && next.length < recentCount; ++i) {
            var row = raw[i]
            if (!row) continue
            next.push(_normalize(row))
        }
        recentApps = next
    }

    function _rebuildSuggestionApps() {
        if (typeof AppModel === "undefined" || !AppModel || !AppModel.frequentApps) {
            suggestionApps = []
            return
        }
        // Pull a wider window of frequent apps; suggestions are picked as the
        // next-most-frequent apps that are NOT already in the Recent strip.
        // Phase 2 will refine ranking with time-of-day category bucketing.
        var raw = AppModel.frequentApps(recentCount + suggestionCount + 8) || []
        var seen = {}
        for (var i = 0; i < recentApps.length; ++i) {
            seen[_identityKey(recentApps[i])] = true
        }
        var next = []
        for (var j = 0; j < raw.length && next.length < suggestionCount; ++j) {
            var row = raw[j]
            if (!row) continue
            var normalized = _normalize(row)
            var key = _identityKey(normalized)
            if (key.length === 0 || seen[key]) continue
            seen[key] = true
            next.push(normalized)
        }
        suggestionApps = next
    }

    function _refreshStripModels() {
        _rebuildRecentApps()
        _rebuildSuggestionApps()
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
            console.warn("[appdeck-launch] grid activate ignored: index out of range", indexValue,
                         "count", filteredApps.length)
            return
        }
        var appData = filteredApps[indexValue] || {}
        console.log("[appdeck-launch] grid item activated index=" + String(indexValue)
                    + " name=" + String(appData.name || appData.display || "")
                    + " desktopFile=" + String(appData.desktopFile || "")
                    + " executable=" + String(appData.executable || ""))
        selectedIndex = indexValue
        switchToPage(_pageForGlobalIndex(indexValue), false, 0)
        grid.currentIndex = indexValue - (currentPage * Math.max(1, pageSize))
        appActivated(appData)
    }

    onPageSizeChanged: { _syncPaginationToSelection(); gridLayoutSettled() }
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
    onSelectedCategoryChanged: {
        // Reset to first page when the category changes so the user does
        // not stay on a now-empty page index.
        selectedIndex = -1
        currentPage = 0
        _rebuildModel()
    }
    onAppModelChanged: { _rebuildModel(); _refreshStripModels() }
    onVisibleChanged: if (visible) { _rebuildModel(); _refreshStripModels() }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            collapseRequested()
            event.accepted = true
        }
    }

    Connections {
        target: root.appModel ? root.appModel : null
        ignoreUnknownSignals: true
        function onCountChanged() { root._rebuildModel(); root._refreshStripModels() }
        function onDataChanged() { root._rebuildModel(); root._refreshStripModels() }
        function onModelReset() { root._rebuildModel(); root._refreshStripModels() }
        function onRowsInserted() { root._rebuildModel(); root._refreshStripModels() }
        function onRowsRemoved() { root._rebuildModel(); root._refreshStripModels() }
        function onAppScoresChanged() { root._refreshStripModels() }
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

        // docs/APPDECK_REDESIGN.md Phase 1 — Region 1: Recent strip.
        // 6 MRU apps sourced from AppModel.frequentApps; hidden while a
        // local filter is active (Phase 4 routes search to Pulse instead).
        Item {
            id: recentSection
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? (root.stripHeight + recentLabel.implicitHeight + 6) : 0
            visible: root.recentApps.length > 0
                     && String(root.filterText || "").trim().length === 0

            Label {
                id: recentLabel
                text: "Recent"
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("semibold")
                color: Theme.color("textPrimary")
                opacity: Theme.opacityMuted
                anchors.left: parent.left
                anchors.top: parent.top
            }

            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: recentLabel.bottom
                anchors.topMargin: 6
                height: root.stripHeight
                spacing: root.stripSpacing

                Repeater {
                    model: root.recentApps
                    delegate: Item {
                        width: root.stripCellWidth
                        height: root.stripHeight

                        AppDeckComp.AppDeckItemDelegate {
                            anchors.fill: parent
                            appData: modelData
                            title: String((modelData && modelData.display) || "")
                            iconSource: String((modelData && modelData.icon) || "")
                            running: !!(modelData && modelData.running)
                            selected: false
                            onActivated: function(appData) { root.appActivated(modelData) }
                        }
                    }
                }
            }
        }

        // docs/APPDECK_REDESIGN.md Phase 1 — Region 2: Suggestions strip.
        // Next 4 frequent apps not already in Recent. Phase 2 will refine
        // ranking with time-of-day category buckets.
        Item {
            id: suggestionsSection
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? (root.stripHeight + suggestionsLabel.implicitHeight + 6) : 0
            visible: root.suggestionApps.length > 0
                     && String(root.filterText || "").trim().length === 0

            Label {
                id: suggestionsLabel
                text: "Suggestions"
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("semibold")
                color: Theme.color("textPrimary")
                opacity: Theme.opacityMuted
                anchors.left: parent.left
                anchors.top: parent.top
            }

            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: suggestionsLabel.bottom
                anchors.topMargin: 6
                height: root.stripHeight
                spacing: root.stripSpacing

                Repeater {
                    model: root.suggestionApps
                    delegate: Item {
                        width: root.stripCellWidth
                        height: root.stripHeight

                        AppDeckComp.AppDeckItemDelegate {
                            anchors.fill: parent
                            appData: modelData
                            title: String((modelData && modelData.display) || "")
                            iconSource: String((modelData && modelData.icon) || "")
                            running: !!(modelData && modelData.running)
                            selected: false
                            onActivated: function(appData) { root.appActivated(modelData) }
                        }
                    }
                }
            }
        }

        // Region 3 header — category segmented control above the paged grid.
        // 5 fixed buckets; selecting one filters the GridView via
        // _rebuildModel(). Hidden while a local filter is active (search
        // routes through Pulse in Phase 4).
        Item {
            id: categorySegmentRow
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 32 : 0
            visible: String(root.filterText || "").trim().length === 0

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Repeater {
                    model: root.categoryButtons
                    delegate: Rectangle {
                        id: chip
                        readonly property bool active: root.selectedCategory === modelData.id
                        height: 28
                        width: chipLabel.implicitWidth + 24
                        radius: height * 0.5
                        antialiasing: true
                        color: active
                               ? Theme.color("accent")
                               : (chipHover.containsMouse
                                  ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.08 : 0.18)
                                  : "transparent")
                        border.width: active ? 0 : Theme.borderWidthThin
                        border.color: active ? "transparent" : Qt.rgba(1, 1, 1, Theme.darkMode ? 0.14 : 0.22)

                        Behavior on color {
                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                        }

                        Label {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: String(modelData.label || "")
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Theme.fontWeight("semibold")
                            color: chip.active
                                   ? Theme.color("accentText")
                                   : Theme.color("textPrimary")
                            opacity: chip.active ? 1.0 : Theme.opacityMuted
                        }

                        MouseArea {
                            id: chipHover
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedCategory = String(modelData.id || "all")
                        }
                    }
                }
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
                    parent: Overlay.overlay
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

            // Touch + mouse swipe to flip pages. The drag follows the pointer
            // (slightly damped) up to pageSwitchOffset, then commits on
            // release if either (a) translation exceeds swipeThreshold or
            // (b) the gesture is a flick — short duration with non-trivial
            // horizontal velocity. Vertical-dominant gestures are ignored so
            // future vertical scrolling can coexist.
            DragHandler {
                id: pageSwipe
                target: null
                enabled: root.pageCount > 1 && !pageSwitchAnim.running
                acceptedDevices: PointerDevice.AllDevices
                // Aggressive grab so a horizontal drag wins over the icon
                // delegate's MouseArea and the empty-area dismiss MouseArea
                // once the user moves past the drag threshold.
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                                 | PointerHandler.ApprovesTakeOverByAnything
                dragThreshold: 10
                xAxis.enabled: true
                yAxis.enabled: false
                readonly property real flickVelocityThreshold: 280
                readonly property real flickDistanceFloor: 12
                onTranslationChanged: {
                    if (!active) {
                        return
                    }
                    if (Math.abs(translation.y) > Math.abs(translation.x) * 0.85) {
                        root.pageShiftX = 0
                        return
                    }
                    var damped = translation.x * 0.6
                    if ((damped > 0 && !root.hasPreviousPage) || (damped < 0 && !root.hasNextPage)) {
                        damped = damped * 0.35
                    }
                    root.pageShiftX = Math.max(-root.pageSwitchOffset, Math.min(root.pageSwitchOffset, damped))
                }
                onActiveChanged: {
                    if (active) {
                        return
                    }
                    var deltaX = translation.x
                    var vx = (centroid && centroid.velocity) ? centroid.velocity.x : 0
                    root.pageShiftX = 0
                    var isFlick = Math.abs(vx) > flickVelocityThreshold
                                  && Math.abs(deltaX) > flickDistanceFloor
                                  && Math.abs(deltaX) > Math.abs(translation.y)
                    var distOk = Math.abs(deltaX) >= root.swipeThreshold
                    if (!distOk && !isFlick) {
                        return
                    }
                    var direction = (isFlick && Math.abs(vx) > Math.abs(deltaX) * 4)
                                    ? (vx < 0 ? -1 : 1)
                                    : (deltaX < 0 ? -1 : 1)
                    if (direction < 0) {
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
                // No preventStealing — the page-swipe DragHandler must be
                // able to take over the pointer grab after threshold.
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

        // HIG-style pagination strip — chevron icon buttons (borderless,
        // secondary tint, 30% opacity when disabled) flanking an iOS-style
        // expanding-dot indicator. No counter text; the indicator is the
        // single source of truth for "where am I".
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

            // Previous — circular ghost button with system symbolic chevron.
            Item {
                id: previousButton
                width: root.pageButtonSize
                height: root.pageButtonSize
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                opacity: root.hasPreviousPage ? 1.0 : 0.30

                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: root.pageButtonSize / 2
                    color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.10 : 0.22)
                    opacity: (previousHover.containsMouse && root.hasPreviousPage) ? 1.0 : 0.0
                    antialiasing: true
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                    }
                }

                Image {
                    id: previousChevron
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    source: "image://themeicon/go-previous-symbolic" + root.iconRev
                    sourceSize.width: 32
                    sourceSize.height: 32
                    smooth: true
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                    opacity: previousChevronFallback.visible ? 0.0 : 0.85
                }

                // Fallback glyph if symbolic icon missing from theme.
                Label {
                    id: previousChevronFallback
                    anchors.centerIn: parent
                    visible: previousChevron.status === Image.Error
                             || previousChevron.status === Image.Null
                             || String(previousChevron.source).length === 0
                    text: "‹"
                    font.pixelSize: Theme.fontSize("hero")
                    font.weight: Theme.fontWeight("regular")
                    color: Theme.color("textPrimary")
                    opacity: Theme.opacityMuted
                }

                MouseArea {
                    id: previousHover
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: root.hasPreviousPage
                    acceptedButtons: Qt.LeftButton
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.switchToPage(root.currentPage - 1, true, -1)
                }
            }

            // Expanding-dot indicator: each dot is a capsule pill whose width
            // smoothly interpolates between dotInactiveWidth and dotActiveWidth
            // as the page selection changes. Animated via Behavior so swipes
            // and clicks both feel continuous.
            Row {
                id: dotRow
                anchors.centerIn: parent
                spacing: 6

                readonly property int dotHeight: 6
                readonly property int dotInactiveWidth: 6
                readonly property int dotActiveWidth: 18

                Repeater {
                    model: root.pageCount
                    delegate: Item {
                        width: dotRow.dotActiveWidth
                        height: root.footerHeight

                        Rectangle {
                            id: dot
                            anchors.centerIn: parent
                            height: dotRow.dotHeight
                            width: index === root.currentPage
                                   ? dotRow.dotActiveWidth
                                   : dotRow.dotInactiveWidth
                            radius: height * 0.5
                            color: Theme.color("textPrimary")
                            opacity: index === root.currentPage ? 0.88 : 0.30
                            antialiasing: true

                            Behavior on width {
                                NumberAnimation {
                                    duration: Theme.durationNormal
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
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (index !== root.currentPage) {
                                    root.switchToPage(index, true, index > root.currentPage ? 1 : -1)
                                }
                            }
                        }
                    }
                }
            }

            // Next — mirror of previous.
            Item {
                id: nextButton
                width: root.pageButtonSize
                height: root.pageButtonSize
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                opacity: root.hasNextPage ? 1.0 : 0.30

                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: root.pageButtonSize / 2
                    color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.10 : 0.22)
                    opacity: (nextHover.containsMouse && root.hasNextPage) ? 1.0 : 0.0
                    antialiasing: true
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                    }
                }

                Image {
                    id: nextChevron
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    source: "image://themeicon/go-next-symbolic" + root.iconRev
                    sourceSize.width: 32
                    sourceSize.height: 32
                    smooth: true
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                    opacity: nextChevronFallback.visible ? 0.0 : 0.85
                }

                Label {
                    id: nextChevronFallback
                    anchors.centerIn: parent
                    visible: nextChevron.status === Image.Error
                             || nextChevron.status === Image.Null
                             || String(nextChevron.source).length === 0
                    text: "›"
                    font.pixelSize: Theme.fontSize("hero")
                    font.weight: Theme.fontWeight("regular")
                    color: Theme.color("textPrimary")
                    opacity: Theme.opacityMuted
                }

                MouseArea {
                    id: nextHover
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: root.hasNextPage
                    acceptedButtons: Qt.LeftButton
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.switchToPage(root.currentPage + 1, true, 1)
                }
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

    Component.onCompleted: { _rebuildModel(); _refreshStripModels(); gridLayoutSettled() }
}
