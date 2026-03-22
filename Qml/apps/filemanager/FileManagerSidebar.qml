import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Desktop_Shell

Rectangle {
    id: root
    property var favoritesModel: null
    property var userBookmarksModel: null
    property var locationsModel: null
    property var recentModel: null
    property var favoriteModel: null
    property bool bookmarkDropHover: false
    property string externalDropPath: ""
    property string externalDropLabel: ""
    property string selectedSidebarPath: "~"
    property string libraryFilter: "all"

    signal openPathRequested(string path)
    signal favoriteContextRequested(int index, string label, string path)
    signal storageEntryActivated(string path, string device, bool mounted, bool browsable)
    signal storageUnmountRequested(string device)
    signal storageContextRequested(string path, string device, bool mounted, bool browsable, bool isRoot)
    signal openLibraryPathRequested(string path)
    signal libraryFilterRequested(string filterMode)

    color: Theme.color("fileManagerSidebarBg")
    border.width: Theme.borderWidthThin
    border.color: bookmarkDropHover
                  ? Theme.color("accent")
                  : Theme.color("fileManagerSidebarBorder")

    function iconRevision() {
        return ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                ? ThemeIconController.revision : 0)
    }

    function menuFontPx() {
        return Math.max(12, Number(Theme.fontSize("menu")))
    }

    function entryIconPx() {
        return Math.max(18, Math.round(menuFontPx() * 1.30))
    }

    function ejectIconPx() {
        return Math.max(13, Math.round(menuFontPx() * 0.95))
    }

    function normalizedBookmarkPath(pathValue) {
        var p = String(pathValue || "").trim()
        if (p.length === 0) {
            return ""
        }
        if (p.indexOf("file://") === 0) {
            var s = p.substring(7)
            if (s.length > 0 && s.charAt(0) !== "/") {
                s = "/" + s
            }
            p = decodeURIComponent(s)
        }
        p = p.replace(/\/+/g, "/")
        if (p.length > 1 && p.charAt(p.length - 1) === "/") {
            p = p.substring(0, p.length - 1)
        }
        return p.toLowerCase()
    }

    function bookmarkDedupeKeys(pathValue) {
        var out = []
        var raw = String(pathValue || "").trim()
        var base = normalizedBookmarkPath(raw)
        if (base.length > 0) {
            out.push(base)
        }
        var knownLeaf = ({
                             "desktop": true,
                             "documents": true,
                             "music": true,
                             "pictures": true,
                             "videos": true,
                             "downloads": true
                         })
        var leaf = ""
        if (raw.indexOf("~/") === 0) {
            var rel = raw.substring(2)
            if (rel.indexOf("/") < 0 && rel.length > 0) {
                leaf = rel.toLowerCase()
            }
        } else if (base.indexOf("/") >= 0) {
            var parts = base.split("/")
            leaf = String(parts.length > 0 ? parts[parts.length - 1] : "").toLowerCase()
        }
        if (leaf.length > 0 && knownLeaf[leaf]) {
            out.push("xdg-home-leaf:" + leaf)
        }
        return out
    }

    function validDropPathForRow(row) {
        if (!row) {
            return ""
        }
        var rowType = String(row.rowType || "")
        if (rowType !== "bookmark" && rowType !== "storage") {
            return ""
        }
        if (rowType === "storage" && !row.browsable) {
            return ""
        }
        var p = String(row.path || "")
        if (rowType === "bookmark") {
            if (p === "__recent__" || p === "__favorites__") {
                return ""
            }
            if (p === "__trash__") {
                return "~/.local/share/Trash/files"
            }
            if (p.startsWith("__")) {
                return ""
            }
        }
        return p.length > 0 ? p : ""
    }

    function resolveDropInfoAt(localX, localY) {
        if (localX < 0 || localY < 0 || localX > width || localY > height) {
            return { "path": "", "label": "" }
        }
        if (!listView || !flatRows) {
            return { "path": "", "label": "" }
        }
        var idx = listView.indexAt(localX, localY)
        if (idx < 0 || idx >= flatRows.count) {
            return { "path": "", "label": "" }
        }
        var row = flatRows.get(idx)
        return {
            "path": validDropPathForRow(row),
            "label": String((row && row.label) ? row.label : "")
        }
    }

    function updateExternalDrop(localX, localY, active) {
        if (!active) {
            externalDropPath = ""
            externalDropLabel = ""
            return { "path": "", "label": "" }
        }
        var info = resolveDropInfoAt(localX, localY)
        externalDropPath = String(info.path || "")
        externalDropLabel = String(info.label || "")
        return info
    }

    function clearExternalDrop() {
        externalDropPath = ""
        externalDropLabel = ""
    }

    function rebuildRows() {
        flatRows.clear()
        flatRows.append({ "rowType": "section", "label": "Bookmarks" })
        var hasTrashEntry = false
        var seenBookmarkPaths = ({})

        if (favoritesModel) {
            for (var i = 0; i < favoritesModel.count; ++i) {
                var fav = favoritesModel.get(i)
                var favPath = String(fav.path || "")
                var favKeys = bookmarkDedupeKeys(favPath)
                for (var fk = 0; fk < favKeys.length; ++fk) {
                    seenBookmarkPaths[favKeys[fk]] = true
                }
                if (favPath === "__trash__") {
                    hasTrashEntry = true
                }
                flatRows.append({
                                    "rowType": "bookmark",
                                    "favIndex": i,
                                    "label": String(fav.label || ""),
                                    "path": favPath,
                                    "icon": String(fav.icon || "folder")
                                })
            }
        }

        if (userBookmarksModel) {
            for (var j = 0; j < userBookmarksModel.count; ++j) {
                var ub = userBookmarksModel.get(j)
                var ubPath = String(ub.path || "")
                var ubKeys = bookmarkDedupeKeys(ubPath)
                var duplicated = false
                for (var uk = 0; uk < ubKeys.length; ++uk) {
                    if (seenBookmarkPaths[ubKeys[uk]]) {
                        duplicated = true
                        break
                    }
                }
                if (duplicated) {
                    continue
                }
                for (var uk2 = 0; uk2 < ubKeys.length; ++uk2) {
                    seenBookmarkPaths[ubKeys[uk2]] = true
                }
                if (ubPath === "__trash__") {
                    hasTrashEntry = true
                }
                flatRows.append({
                                    "rowType": "bookmark",
                                    "favIndex": -1,
                                    "label": String(ub.label || ""),
                                    "path": ubPath,
                                    "icon": String(ub.icon || "folder")
                                })
            }
        }

        if (!hasTrashEntry) {
            flatRows.append({
                                "rowType": "bookmark",
                                "favIndex": -1,
                                "label": "Trash",
                                "path": "__trash__",
                                "icon": "user-trash-symbolic"
                            })
        }

        if (locationsModel && locationsModel.count > 0) {
            flatRows.append({ "rowType": "section", "label": "Storage" })
            for (var k = 0; k < locationsModel.count; ++k) {
                var loc = locationsModel.get(k)
                flatRows.append({
                                    "rowType": "storage",
                                    "label": String(loc.label || "Volume"),
                                    "path": String(loc.path || ""),
                                    "device": String(loc.device || ""),
                                    "icon": String(loc.icon || "drive-harddisk-symbolic"),
                                    "bytesTotal": Number(loc.bytesTotal || 0),
                                    "bytesAvailable": Number(loc.bytesAvailable || -1),
                                    "mounted": !!loc.mounted,
                                    "browsable": !!loc.browsable,
                                    "isRoot": !!loc.isRoot
                                })
            }
        }

        flatRows.append({ "rowType": "section", "label": "Network" })
        flatRows.append({
                            "rowType": "network",
                            "label": "Entire Network",
                            "icon": "network-workgroup-symbolic"
                        })
    }

    Timer {
        id: rebuildDebounce
        interval: 40
        repeat: false
        running: false
        onTriggered: root.rebuildRows()
    }

    ListModel {
        id: flatRows
    }

    Connections {
        target: favoritesModel
        ignoreUnknownSignals: true
        function onCountChanged() { rebuildDebounce.restart() }
    }

    Connections {
        target: userBookmarksModel
        ignoreUnknownSignals: true
        function onCountChanged() { rebuildDebounce.restart() }
    }

    Connections {
        target: locationsModel
        ignoreUnknownSignals: true
        function onCountChanged() { rebuildDebounce.restart() }
    }

    Component.onCompleted: rebuildRows()

    ListView {
        id: listView
        anchors.fill: parent
        anchors.margins: 6
        model: flatRows
        clip: true
        spacing: 0
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 5000
        flickDeceleration: 3500
        reuseItems: true
        cacheBuffer: 640
        interactive: contentHeight > height

        delegate: Item {
            readonly property var row: flatRows.get(index) || ({})
            readonly property bool isSection: String(row.rowType || "") === "section"
            readonly property bool isStorage: String(row.rowType || "") === "storage"
            readonly property bool isNetwork: String(row.rowType || "") === "network"
            readonly property bool active: String(row.path || "").length > 0
                                         && root.selectedSidebarPath === String(row.path || "")
            readonly property bool dropTarget: !isSection
                                              && String(row.path || "").length > 0
                                              && root.externalDropPath === String(row.path || "")
            readonly property real usedRatio: {
                var bt = Number(row.bytesTotal || 0)
                var ba = Number(row.bytesAvailable || -1)
                if (bt <= 0 || ba < 0) {
                    return 0
                }
                var used = bt - ba
                if (used < 0) {
                    used = 0
                }
                return Math.max(0, Math.min(1, used / bt))
            }
            width: listView.width
            height: isSection
                    ? Math.max(26, Math.round(root.menuFontPx() * 1.75))
                    : (isStorage
                       ? Math.max(37, Math.round(root.menuFontPx() * 2.20))
                       : Math.max(30, Math.round(root.menuFontPx() * 1.95)))

            Text {
                visible: isSection
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                text: String(row.label || "")
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("bold")
            }

            Rectangle {
                visible: !isSection
                anchors.fill: parent
                radius: Theme.radiusMd
                color: active
                       ? "transparent"
                       : (dropTarget
                          ? Theme.color("accentSoft")
                          : (entryMouse.containsMouse ? Theme.color("hoverItem") : "transparent"))
                border.width: dropTarget ? 1 : 0
                border.color: dropTarget ? Theme.color("accent") : "transparent"

                Row {
                    anchors.top: parent.top
                    anchors.topMargin: isStorage ? 2 : 6
                    x: 7
                    width: parent.width - 14
                    spacing: 5

                    Image {
                        id: rowIcon
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 1
                        width: root.entryIconPx()
                        height: root.entryIconPx()
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: true
                        smooth: true
                        source: "image://themeicon/" + String(row.icon || (isStorage ? "drive-harddisk-symbolic" : "folder"))
                                + "?v=" + root.iconRevision()
                    }

                    Text {
                        visible: rowIcon.status !== Image.Ready
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\u2022"
                        color: active ? Theme.color("selectedItem") : Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("menu")
                    }

                    Text {
                        text: String(row.label || "")
                        anchors.verticalCenter: parent.verticalCenter
                        color: active ? Theme.color("selectedItem") : Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("menu")
                        width: parent.width - (unmountButton.visible ? 46 : 22)
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    id: unmountButton
                    visible: isStorage && !!row.mounted && !row.isRoot
                    width: 18
                    height: 18
                    radius: Theme.radiusSmPlus
                    anchors.right: parent.right
                    anchors.rightMargin: 7
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    color: unmountMouse.containsMouse
                           ? Theme.color("fileManagerUnmountHover")
                           : "transparent"
                    z: 3

                    Image {
                        id: unmountIcon
                        anchors.centerIn: parent
                        width: root.ejectIconPx()
                        height: root.ejectIconPx()
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: true
                        source: "image://themeicon/media-eject-symbolic?v=" + root.iconRevision()
                    }

                    Text {
                        visible: unmountIcon.status !== Image.Ready
                        anchors.centerIn: parent
                        text: "\u23cf"
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("xs")
                    }

                    MouseArea {
                        id: unmountMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        preventStealing: true
                        onClicked: function(mouse) {
                            mouse.accepted = true
                            root.storageUnmountRequested(String(row.device || ""))
                        }
                    }
                }

                Rectangle {
                    visible: isStorage
                             && Number(row.bytesTotal || 0) > 0
                             && Number(row.bytesAvailable || -1) >= 0
                    x: 26
                    y: parent.height - 7
                    width: parent.width - 32
                    height: 2
                    radius: Theme.radiusTiny
                    color: Theme.color("fileManagerStorageTrack")

                    Rectangle {
                        width: Math.max(2, parent.width * usedRatio)
                        height: parent.height
                        radius: parent.radius
                        color: Theme.color("fileManagerStorageFill")
                    }
                }

                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: function(mouse) {
                        if (isNetwork) {
                            return
                        }
                        if (isStorage) {
                            if (mouse.button === Qt.RightButton) {
                                root.storageContextRequested(String(row.path || ""),
                                                            String(row.device || ""),
                                                            !!row.mounted,
                                                            !!row.browsable,
                                                            !!row.isRoot)
                                return
                            }
                            root.storageEntryActivated(String(row.path || ""),
                                                      String(row.device || ""),
                                                      !!row.mounted,
                                                      !!row.browsable)
                            return
                        }
                        if (mouse.button === Qt.RightButton) {
                            var favIndex = (row.favIndex === undefined || row.favIndex === null)
                                    ? -1 : Number(row.favIndex)
                            root.favoriteContextRequested(favIndex,
                                                          String(row.label || ""),
                                                          String(row.path || ""))
                            return
                        }
                        root.openPathRequested(String(row.path || ""))
                    }
                }
            }
        }
    }
}
