import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as AppDeckComp

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

    readonly property var allAppsModel: (typeof AppModel !== "undefined" && AppModel)
                                         ? AppModel : appsModel
    readonly property int contentTopInset: preferredPanelY >= 0 ? preferredPanelY : Math.max(18, root.panelHeight + 14)
    readonly property int contentBottomInset: Math.max(24, root.bottomSafeInset)
    readonly property int totalAppCount: allAppsModel && typeof allAppsModel.count !== "undefined"
                                         ? Number(allAppsModel.count || 0) : 0
    readonly property var favoriteApps: {
        if (!allAppsModel || !allAppsModel.topApps) {
            return []
        }
        var rows = allAppsModel.topApps(8) || []
        var mapped = []
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || {}
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
    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    anchors.fill: parent
    focus: visible

    function _insideContentArea(px, py) {
        return px >= contentFrame.x
                && px <= (contentFrame.x + contentFrame.width)
                && py >= contentFrame.y
                && py <= (contentFrame.y + contentFrame.height)
    }

    onAppdeckSearchSeedChanged: {
        if (String(appdeckSearchSeed || "").trim().length > 0) {
            filterText = String(appdeckSearchSeed || "")
        }
    }

    onVisibleChanged: {
        if (visible) {
            revealProgress = 0.0
            if (String(appdeckSearchSeed || "").trim().length > 0) {
                filterText = String(appdeckSearchSeed || "")
            }
            revealAnim.restart()
        } else {
            revealAnim.stop()
            revealProgress = 0.0
            filterText = ""
        }
    }

    NumberAnimation {
        id: revealAnim
        target: root
        property: "revealProgress"
        from: 0.0
        to: 1.0
        duration: Theme.durationNormal
        easing.type: Theme.easingDefault
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
        onClicked: function(mouse) {
            if (!root._insideContentArea(mouse.x, mouse.y)) {
                root.dismissRequested()
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

            Item {
                id: headerHitLayer
                Layout.fillWidth: true
                Layout.preferredHeight: header.implicitHeight

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: root.dismissRequested()
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

            Item {
                id: favoritesHitLayer
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? favoritesRow.implicitHeight : 0
                visible: root.filterText.trim().length === 0 && favoriteApps.length > 0
                opacity: visible ? 1.0 : 0.0

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: root.dismissRequested()
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
                    onClicked: root.dismissRequested()
                }

                AppDeckComp.AppDeckGridView {
                    id: appsGrid
                    anchors.fill: parent
                    appModel: root.allAppsModel
                    filterText: root.filterText
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
