import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as AppDeckComp
import "./v2" as AppDeckV2

FocusScope {
    id: root

    required property var appsModel
    required property var desktopScene

    property int panelHeight: 0
    property int bottomSafeInset: 160
    property real preferredPanelX: -1
    property real preferredPanelY: -1
    property real preferredPanelWidth: -1
    property real preferredPanelHeight: -1
    property string appdeckSearchSeed: ""
    property string filterText: ""
    property real revealProgress: 1.0
    property int lazyRefreshCooldownMs: 15000
    property double lastLazyRefreshAtMs: 0
    // docs/APPDECK.md §7 — forwarded to inner AppDeckGridView.
    property bool iconsRenderedExternally: false
    // docs/APPDECK.md §8 — morph progress 0..1 (dock→grid). When set, the
    // header / search row stagger their reveal via GridContentLayer's curve
    // (visible >0.05, opacity 0→1 across 0.45→0.80). Defaults to 1.0 so the
    // legacy revealProgress path stays in effect when this isn't wired.
    property real morphProgress: 1.0

    readonly property var allAppsModel: (typeof AppModel !== "undefined" && AppModel)
                                         ? AppModel : appsModel
    readonly property int contentTopInset: preferredPanelY >= 0 ? preferredPanelY : Math.max(18, root.panelHeight + 14)
    readonly property int contentBottomInset: Math.max(24, root.bottomSafeInset)
    readonly property int totalAppCount: allAppsModel && typeof allAppsModel.count !== "undefined"
                                         ? Number(allAppsModel.count || 0) : 0
    // Recent apps — sorted by lastLaunch (ISO timestamp) descending. We
    // pull a generous window from topApps() then re-sort client-side,
    // because topApps() blends launch frequency with recency in its score;
    // for a "Recent" row we want strict last-used order.
    readonly property var favoriteApps: {
        if (!allAppsModel || !allAppsModel.topApps) {
            return []
        }
        var rows = allAppsModel.topApps(48) || []
        var withTs = []
        for (var i = 0; i < rows.length; ++i) {
            var r = rows[i] || {}
            var ts = String(r.lastLaunch || "")
            if (ts.length === 0) {
                continue
            }
            withTs.push(r)
        }
        // ISO 8601 strings compare chronologically.
        withTs.sort(function(a, b) {
            var aTs = String(a.lastLaunch || "")
            var bTs = String(b.lastLaunch || "")
            if (aTs === bTs) return 0
            return aTs < bTs ? 1 : -1
        })
        var picked = withTs.slice(0, 8)
        var mapped = []
        for (var j = 0; j < picked.length; ++j) {
            var row = picked[j] || {}
            var iconName = String(row.iconName || "")
            var iconValue = String(row.icon || row.iconSource || "")
            if (iconName.length > 0) {
                var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                           ? ThemeIconController.revision : 0)
                iconValue = "image://themeicon/" + iconName + "?v=" + rev
            }
            mapped.push({
                "appId": String(row.appId || row.desktopId || row.desktopFile || row.executable || row.name || ""),
                "display": String(row.display || row.name || ""),
                "icon": iconValue,
                "running": !!row.running,
                "desktopId": String(row.desktopId || ""),
                "desktopFile": String(row.desktopFile || ""),
                "executable": String(row.executable || ""),
                "name": String(row.name || "")
            })
        }
        return mapped
    }

    signal dismissRequested()
    signal pointerDismissRequested()
    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)
    // docs/APPDECK.md §5 — re-emit grid cell-layout settle from inner GridView
    // so AppDeckGeometry can recompute grid anchors.
    signal gridLayoutSettled()

    // Forward to the grid view's own helper (returns Qt.point in
    // AppDeckGridView coordinates; caller mapToItem-s into surface space).
    function gridIconCenterFor(globalIndex) {
        if (!appsGrid) {
            return Qt.point(-1, -1)
        }
        var local = appsGrid.gridIconCenterFor(globalIndex)
        if (local.x < 0) {
            return local
        }
        return appsGrid.mapToItem(root, local.x, local.y)
    }

    anchors.fill: parent
    focus: visible

    function _insideContentArea(px, py) {
        return px >= contentFrame.x
                && px <= (contentFrame.x + contentFrame.width)
                && py >= contentFrame.y
                && py <= (contentFrame.y + contentFrame.height)
    }

    function maybeRefreshAllAppsLazy(forceRefresh) {
        var model = root.allAppsModel
        if (!model) {
            return
        }
        if (!model.refreshAsync && !model.refresh) {
            return
        }
        var now = Date.now()
        var shouldRefresh = !!forceRefresh
        if (!shouldRefresh) {
            var ageMs = now - Number(root.lastLazyRefreshAtMs || 0)
            var staleEnough = ageMs >= Math.max(1000, Number(root.lazyRefreshCooldownMs || 15000))
            var hasNoApps = Number(root.totalAppCount || 0) <= 0
            shouldRefresh = staleEnough || hasNoApps
        }
        if (!shouldRefresh) {
            return
        }
        root.lastLazyRefreshAtMs = now
        if (model.refreshAsync) {
            model.refreshAsync()
        } else {
            model.refresh()
        }
    }

    onAppdeckSearchSeedChanged: {
        if (String(appdeckSearchSeed || "").trim().length > 0) {
            filterText = String(appdeckSearchSeed || "")
        }
    }

    onVisibleChanged: {
        if (visible) {
            maybeRefreshAllAppsLazy(false)
            if (String(appdeckSearchSeed || "").trim().length > 0) {
                filterText = String(appdeckSearchSeed || "")
            }
        } else {
            filterText = ""
        }
    }

    onAllAppsModelChanged: {
        if (visible) {
            maybeRefreshAllAppsLazy(true)
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.dismissRequested()
            event.accepted = true
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: false
        acceptedButtons: Qt.LeftButton
        // P0 (§12 of fix prompt) — gate the outside-click dismiss on the
        // grid being fully settled. Without this, clicks landing during the
        // morph (when this surface is animating into place) trip
        // `pointerDismissRequested` and the downstream
        // `canDismissGridByPointer` flag may briefly contradict itself. Once
        // morphProgress is past 0.95 the morph is visually complete and the
        // MouseArea behaves normally.
        enabled: root.morphProgress >= 0.95
        onClicked: function(mouse) {
            if (!root._insideContentArea(mouse.x, mouse.y)) {
                root.pointerDismissRequested()
            }
        }
    }

    Item {
        id: contentFrame
        width: preferredPanelWidth > 0
               ? preferredPanelWidth
               : Math.min(1180, Math.max(320, parent.width - (Math.max(28, parent.width * 0.07) * 2)))
        height: preferredPanelHeight > 0
                ? preferredPanelHeight
                : Math.min(760, Math.max(360, parent.height - root.contentTopInset - root.contentBottomInset))
        x: preferredPanelX >= 0 ? preferredPanelX : Math.round((parent.width - width) * 0.5)
        y: root.contentTopInset
        opacity: Math.max(0.0, Math.min(1.0, root.revealProgress))
        transform: Translate { y: (1.0 - root.revealProgress) * 18 }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            // docs/APPDECK.md §8 — header staggers in over morphProgress
            // 0.45→0.80 via GridContentLayer's reveal curve. Defaults
            // (morphProgress=1.0) keep the header fully visible when the
            // outer panel is up, which matches legacy behavior.
            AppDeckV2.GridContentLayer {
                id: headerHitLayer
                Layout.fillWidth: true
                Layout.preferredHeight: header.implicitHeight
                morphProgress: root.morphProgress

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: root.pointerDismissRequested()
                }

                AppDeckComp.AppDeckGridHeader {
                    id: header
                    anchors.fill: parent
                    title: "AppDeck"
                    searchText: root.filterText
                    totalCount: root.totalAppCount
                    filteredCount: appsGrid.filteredCount
                    onQueryChanged: function(text) {
                        root.filterText = text
                    }
                    onCollapseRequested: root.dismissRequested()
                    onFocusGridRequested: appsGrid.forceActiveFocus()
                }
            }

            // docs/APPDECK.md §8 — favorites row joins the staged reveal.
            // gateVisible carries the existing logic (hide when filtering or
            // no favorites); morphProgress drives the stagger curve on top.
            AppDeckV2.GridContentLayer {
                id: favoritesHitLayer
                Layout.fillWidth: true
                Layout.preferredHeight: gateVisible ? favoritesRow.implicitHeight : 0
                morphProgress: root.morphProgress
                gateVisible: root.filterText.trim().length === 0 && favoriteApps.length > 0

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: root.pointerDismissRequested()
                }

                AppDeckComp.AppDeckFavoritesRow {
                    id: favoritesRow
                    anchors.fill: parent
                    favoritesModel: root.favoriteApps
                    onAppActivated: function(appData) {
                        root.appChosen(appData)
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: root.pointerDismissRequested()
                }

                AppDeckComp.AppDeckGridView {
                    id: appsGrid
                    anchors.fill: parent
                    appModel: root.allAppsModel
                    filterText: root.filterText
                    iconsRenderedExternally: root.iconsRenderedExternally
                    onAppActivated: function(appData) {
                        root.appChosen(appData)
                    }
                    onCollapseRequested: root.dismissRequested()
                    onAddToDockRequested: function(appData) {
                        root.addToDockRequested(appData)
                    }
                    onAddToDesktopRequested: function(appData) {
                        root.addToDesktopRequested(appData)
                    }
                    onGridLayoutSettled: root.gridLayoutSettled()
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 6
                    visible: appsGrid.noResultState || appsGrid.emptyState

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: appsGrid.noResultState ? "Tidak ada aplikasi yang cocok" : "Belum ada aplikasi"
                        font.pixelSize: Theme.fontSize("title")
                        color: Theme.color("textPrimary")
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: appsGrid.noResultState ? "Coba kata kunci lain" : "Aplikasi akan tampil di sini"
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textSecondary")
                        opacity: Theme.opacityMuted
                    }
                }
            }
        }
    }
}
