import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "." as AppDeckComp

Item {
    id: root

    property var appModel: null
    property string filterText: ""
    property var filteredApps: []
    property int selectedIndex: -1
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
    readonly property int minCellWidth: 104
    readonly property int maxCellWidth: 146
    readonly property int gridSpacing: 12
    readonly property int columnCount: Math.max(
                                           1,
                                           Math.floor((grid.width + gridSpacing)
                                                      / (Math.max(1, minCellWidth) + gridSpacing))
                                       )

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
            grid.currentIndex = -1
            return
        }
        if (selectedIndex < 0 || selectedIndex >= filteredApps.length) {
            selectedIndex = 0
        }
        grid.currentIndex = selectedIndex
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
        grid.currentIndex = indexValue
        appActivated(filteredApps[indexValue])
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

    GridView {
        id: grid
        anchors.fill: parent
        clip: true
        model: root.filteredApps
        cellWidth: Math.max(
                       root.minCellWidth,
                       Math.min(root.maxCellWidth,
                                Math.floor((width - Math.max(0, root.columnCount - 1) * root.gridSpacing)
                                           / Math.max(1, root.columnCount)))
                   )
        cellHeight: 130
        interactive: contentHeight > height
        currentIndex: root.selectedIndex
        focus: true
        keyNavigationWraps: false
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight

            AppDeckComp.AppDeckItemDelegate {
                id: delegateItem
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(96, grid.cellWidth - 6)
                height: grid.cellHeight - 6
                appData: modelData
                title: String((modelData && modelData.display) || "")
                iconSource: String((modelData && modelData.icon) || "")
                running: !!(modelData && modelData.running)
                selected: GridView.isCurrentItem
                onHovered: {
                    root.selectedIndex = index
                    grid.currentIndex = index
                }
                onActivated: function(appData) { root.activateIndex(index) }
                onContextMenuRequested: function(appData) {
                    menu.actionAppData = appData || ({})
                    console.log("[apphub] context menu requested app=", JSON.stringify(menu.actionAppData))
                    menu.popup()
                }
            }

            Menu {
                id: menu
                property var actionAppData: ({})
                MenuItem {
                    text: "Pin to Appdeck"
                    onTriggered: {
                        var payload = menu.actionAppData || modelData || ({})
                        console.log("[apphub] pin triggered payload=", JSON.stringify(payload))
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
                root.activateIndex(grid.currentIndex)
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Escape) {
                root.collapseRequested()
                event.accepted = true
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
                mipmap: true
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
