import QtQuick 2.15
import QtQuick.Controls 2.15
import SlmStyle
import "."

Item {
    id: root

    required property var hostRoot
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)
    property var fileMenuSlmRows: []
    property var folderMenuSlmRows: []
    property var multiMenuSlmRows: []

    function menuColor(key, fallback) {
        if (typeof Theme !== "undefined" && Theme && Theme.color) {
            return Theme.color(String(key || ""))
        }
        return String(fallback || "transparent")
    }

    function menuFontPx(role, fallback) {
        if (typeof Theme !== "undefined" && Theme && Theme.fontSize) {
            return Number(Theme.fontSize(String(role || "body")))
        }
        return Number(fallback || 14)
    }

    function menuRadius(fallback) {
        if (typeof Theme !== "undefined" && Theme && Theme.radiusControl !== undefined) {
            return Number(Theme.radiusControl)
        }
        return Number(fallback || 7)
    }

    function slmShareRows() {
        var source = root.hostRoot.contextSlmShareActions || []
        var out = []
        var seen = ({})
        for (var i = 0; i < source.length; ++i) {
            var row = source[i] || ({})
            var actionId = String((row && row.id) ? row.id : "")
            var actionName = String((row && row.name) ? row.name : "").trim()
            if (actionId.length <= 0 || actionName.length <= 0) {
                continue
            }
            if (seen[actionId] === true) {
                continue
            }
            seen[actionId] = true
            out.push({
                         "actionId": actionId,
                         "name": actionName,
                         "iconName": String((row && row.iconName) ? row.iconName
                                                                    : ((row && row.icon) ? row.icon : "")),
                         "iconSource": String((row && row.iconSource) ? row.iconSource : "")
                     })
        }
        return out
    }

    function slmMenuController() {
        if (!root.hostRoot || !root.hostRoot.fileManagerApiRef
                || !root.hostRoot.fileManagerApiRef.slmContextMenuController) {
            return null
        }
        return root.hostRoot.fileManagerApiRef.slmContextMenuController
    }

    function slmMenuRootItems() {
        var ctl = root.slmMenuController()
        if (!ctl) {
            return []
        }
        if (ctl.menuRootItems) {
            return ctl.menuRootItems() || []
        }
        if (ctl.rootItems) {
            return ctl.rootItems() || []
        }
        return []
    }

    function slmActionFlatRows() {
        var source = root.hostRoot.contextSlmActions || []
        var byId = ({})
        for (var i = 0; i < source.length; ++i) {
            var item = source[i] || ({})
            var id = String((item && item.id) ? item.id : "")
            if (id.length > 0) {
                byId[id] = item
            }
        }

        var out = []
        var seen = ({})
        function walk(nodeId) {
            var children = root.hostRoot.slmContextMenuChildren(nodeId) || []
            for (var c = 0; c < children.length; ++c) {
                var node = children[c] || ({})
                var type = String((node && node.type) ? node.type : "")
                var actionId = String((node && node.actionId) ? node.actionId : "")
                if (type === "submenu") {
                    var childId = String((node && node.id) ? node.id : "")
                    if (childId.length > 0) {
                        walk(childId)
                    }
                    continue
                }
                if (type !== "action" || actionId.length <= 0 || seen[actionId] === true) {
                    continue
                }
                var actionObj = byId[actionId]
                if (!actionObj) {
                    continue
                }
                var actionName = String((actionObj && actionObj.name) ? actionObj.name : "").trim()
                if (actionName.length <= 0) {
                    continue
                }
                seen[actionId] = true
                out.push(actionObj)
            }
        }

        walk("root")
        if (out.length > 0) {
            return out
        }

        // Fallback for old/non-tree providers.
        var rows = []
        for (var j = 0; j < source.length; ++j) {
            var action = source[j] || ({})
            var rid = String((action && action.id) ? action.id : "")
            var rname = String((action && action.name) ? action.name : "")
            if (rid.length > 0 && rname.trim().length > 0) {
                rows.push(action)
            }
        }
        return rows
    }

    function splitSlmMenuRows(actions) {
        var src = actions || []
        var out = []
        var seen = ({})
        for (var i = 0; i < src.length; ++i) {
            var action = src[i] || ({})
            var actionId = String((action && action.id) ? action.id : "")
            var actionName = String((action && action.name) ? action.name : "").trim()
            var actionGroup = String((action && action.group) ? action.group : "").trim()
            if (actionId.length <= 0 || actionName.length <= 0) {
                continue
            }
            var dedupKey = actionGroup.toLowerCase() + "|" + actionName.toLowerCase()
            if (seen[dedupKey] === true) {
                continue
            }
            seen[dedupKey] = true
            out.push({
                         "kind": "action",
                         "label": actionName,
                         "actionId": actionId,
                         "iconName": String((action && action.iconName) ? action.iconName : ""),
                         "nodeId": ""
                     })
        }
        return out
    }

    function slmCollectLinearRows(nodeId, level, outRows) {
        var children = root.hostRoot.slmContextMenuChildren(String(nodeId || "root")) || []
        for (var i = 0; i < children.length; ++i) {
            var child = children[i] || ({})
            var type = String((child && child.type) ? child.type : "")
            if (type === "submenu") {
                outRows.push({
                                 "kind": "groupHeader",
                                 "label": String((child && child.label) ? child.label : "Group"),
                                 "level": Number(level || 0),
                                 "actionId": "",
                                 "iconName": ""
                             })
                var childNodeId = String((child && child.id) ? child.id : "")
                if (childNodeId.length > 0) {
                    root.slmCollectLinearRows(childNodeId, Number(level || 0) + 1, outRows)
                }
                continue
            }
            if (type !== "action") {
                continue
            }
            var actionId = String((child && child.actionId) ? child.actionId : "")
            if (actionId.length <= 0) {
                continue
            }
            outRows.push({
                             "kind": "action",
                             "label": String((child && child.label) ? child.label : actionId),
                             "actionId": actionId,
                             "iconName": String((child && child.icon) ? child.icon : ""),
                             "level": Number(level || 0)
                         })
        }
    }

    function slmRowsLinearFromTree() {
        var rootChildren = root.hostRoot.slmContextMenuChildren("root") || []
        var rows = []
        var ungrouped = []
        var grouped = []
        for (var i = 0; i < rootChildren.length; ++i) {
            var child = rootChildren[i] || ({})
            if (String((child && child.type) ? child.type : "") === "submenu") {
                grouped.push(child)
            } else if (String((child && child.type) ? child.type : "") === "action") {
                ungrouped.push(child)
            }
        }

        for (var u = 0; u < ungrouped.length; ++u) {
            var a = ungrouped[u] || ({})
            var aid = String((a && a.actionId) ? a.actionId : "")
            if (aid.length <= 0) {
                continue
            }
            rows.push({
                         "kind": "action",
                         "label": String((a && a.label) ? a.label : aid),
                         "actionId": aid,
                         "iconName": String((a && a.icon) ? a.icon : ""),
                         "level": 0
                     })
        }

        for (var g = 0; g < grouped.length; ++g) {
            if (rows.length > 0) {
                rows.push({ "kind": "separator" })
            }
            var groupNode = grouped[g] || ({})
            rows.push({
                         "kind": "groupHeader",
                         "label": String((groupNode && groupNode.label) ? groupNode.label : "Group"),
                         "level": 0,
                         "actionId": "",
                         "iconName": ""
                     })
            var nodeId = String((groupNode && groupNode.id) ? groupNode.id : "")
            if (nodeId.length > 0) {
                root.slmCollectLinearRows(nodeId, 1, rows)
            }
        }

        return rows
    }

    function slmRowsForCurrentContext() {
        var rows = root.slmRowsLinearFromTree()
        if (rows && rows.length > 0) {
            return rows
        }
        return root.splitSlmMenuRows(root.slmActionFlatRows())
    }

    function openFileMenu(px, py) {
        if (!fileEntryMenu || fileEntryMenu.open === undefined)
            return
        root.fileMenuSlmRows = root.slmMenuRootItems()
        if (!root.fileMenuSlmRows || root.fileMenuSlmRows.length <= 0) {
            root.fileMenuSlmRows = root.slmRowsForCurrentContext()
        }

        root.slmRebuildInjectedObjects(fileEntryMenu, root.fileMenuSlmRows, fileEntryMenu.count-1)
        fileEntryMenu.x = px
        fileEntryMenu.y = py
        fileEntryMenu.open()
    }

    function openFolderMenu(px, py) {
        if (!folderEntryMenu || folderEntryMenu.open === undefined)
            return
        root.folderMenuSlmRows = root.slmMenuRootItems()
        if (!root.folderMenuSlmRows || root.folderMenuSlmRows.length <= 0) {
            root.folderMenuSlmRows = root.slmRowsForCurrentContext()
        }
        root.slmRebuildInjectedObjects(folderEntryMenu, root.folderMenuSlmRows, folderEntryMenu.count-1)
        folderEntryMenu.x = px
        folderEntryMenu.y = py
        folderEntryMenu.open()
    }

    function openMultiSelectionMenu(px, py) {
        if (!multiSelectionMenu || multiSelectionMenu.open === undefined)
            return
        root.multiMenuSlmRows = root.slmMenuRootItems()
        if (!root.multiMenuSlmRows || root.multiMenuSlmRows.length <= 0) {
            root.multiMenuSlmRows = root.slmRowsForCurrentContext()
        }
        root.slmRebuildInjectedObjects(multiSelectionMenu, root.multiMenuSlmRows, 1)
        multiSelectionMenu.x = px
        multiSelectionMenu.y = py
        multiSelectionMenu.open()
    }

    function openTrashMenu(px, py) {
        if (!trashEntryMenu || trashEntryMenu.open === undefined)
            return
        trashEntryMenu.x = px
        trashEntryMenu.y = py
        trashEntryMenu.open()
    }

    function closeMenu(menuRef) {
        if (!menuRef || menuRef.close === undefined) {
            return
        }
        if (menuRef.opened !== undefined && !menuRef.opened) {
            return
        }
        menuRef.close()
    }

    function closeAllMenus() {
        closeMenu(fileEntryMenu)
        closeMenu(folderEntryMenu)
        closeMenu(multiSelectionMenu)
        closeMenu(trashEntryMenu)
    }

    function slmClearNativeMenuEntries(menuRef) {
        if (!menuRef) {
            return
        }
        var dyn = menuRef._slmDynamicItems || []
        for (var i = dyn.length - 1; i >= 0; --i) {
            var entry = dyn[i] || ({})
            var obj = entry.object
            var kind = String((entry && entry.kind) ? entry.kind : "")
            if (!obj) {
                continue
            }
            if (kind === "menu" && menuRef.removeMenu) {
                menuRef.removeMenu(obj)
            } else if (kind === "action" && menuRef.removeAction) {
                menuRef.removeAction(obj)
            } else if (kind === "item" && menuRef.removeItem) {
                menuRef.removeItem(obj)
            }
            obj.destroy()
        }
        menuRef._slmDynamicItems = []
    }

    function slmTrackNativeMenuEntry(menuRef, kind, object) {
        if (!menuRef || !object) {
            return
        }
        var next = (menuRef._slmDynamicItems || []).slice(0)
        next.push({ "kind": String(kind || ""), "object": object })
        menuRef._slmDynamicItems = next
    }

    function slmPopulateNativeMenu(menuRef, nodeId) {
        if (!menuRef) {
            return
        }
        var children = root.hostRoot.slmContextMenuChildren(String(nodeId || "root")) || []
        for (var i = 0; i < children.length; ++i) {
            var child = children[i] || ({})
            var type = String((child && child.type) ? child.type : "")
            if (type === "submenu") {
                var submenu = slmNativeSubmenuComponent.createObject(menuRef, {
                                                                          "menuRow": child
                                                                      })
                if (submenu && menuRef.addMenu) {
                    menuRef.addMenu(submenu)
                    root.slmTrackNativeMenuEntry(menuRef, "menu", submenu)
                } else if (submenu) {
                    submenu.destroy()
                }
                continue
            }
            var actionId = String((child && child.actionId) ? child.actionId : "")
            if (type !== "action" || actionId.length <= 0) {
                continue
            }
            var actionObj = slmNativeActionComponent.createObject(menuRef, {
                                                                       "menuRow": child
                                                                   })
            if (actionObj && menuRef.addAction) {
                menuRef.addAction(actionObj)
                root.slmTrackNativeMenuEntry(menuRef, "action", actionObj)
            } else if (actionObj) {
                actionObj.destroy()
            }
        }
    }

    function slmInsertObject(parentMenu, index, object) {
        if (!parentMenu || !object) {
            return
        }
        if (object._slmIsNativeMenu && parentMenu.insertMenu) {
            parentMenu.insertMenu(index, object)
            return
        }
        if (object._slmIsNativeAction && parentMenu.insertAction) {
            parentMenu.insertAction(index, object)
            return
        }
        if (parentMenu.insertItem) {
            parentMenu.insertItem(index, object)
        }
    }

    function slmRemoveObject(parentMenu, object) {
        if (!parentMenu || !object) {
            return
        }
        if (object._slmIsNativeMenu && parentMenu.removeMenu) {
            parentMenu.removeMenu(object)
            return
        }
        if (object._slmIsNativeAction && parentMenu.removeAction) {
            parentMenu.removeAction(object)
            return
        }
        if (parentMenu.removeItem) {
            parentMenu.removeItem(object)
        }
    }

    function slmClearInjectedObjects(menuRef) {
        if (!menuRef) {
            return
        }
        var injected = menuRef._slmInjectedObjects || []
        for (var i = injected.length - 1; i >= 0; --i) {
            var obj = injected[i]
            if (!obj) {
                continue
            }
            root.slmRemoveObject(menuRef, obj)
            obj.destroy()
        }
        menuRef._slmInjectedObjects = []
    }

    function slmRebuildInjectedObjects(menuRef, rows, insertAt) {
        if (!menuRef) {
            return
        }
        root.slmClearInjectedObjects(menuRef)
        var list = []
        var src = rows || []
        var offset = Number(insertAt || 0)
        for (var i = 0; i < src.length; ++i) {
            var row = src[i] || ({})
            var type = String((row && row.type) ? row.type : "")
            var obj = null
            if (type === "submenu") {
                obj = slmNativeSubmenuComponent.createObject(menuRef, { "menuRow": row })
            } else if (type === "action") {
                obj = slmNativeActionComponent.createObject(menuRef, { "menuRow": row })
            } else if (row && row.kind === "separator") {
                obj = slmSeparatorComponent.createObject(menuRef)
            }
            if (!obj) {
                continue
            }
            root.slmInsertObject(menuRef, offset + list.length, obj)
            list.push(obj)
        }
        menuRef._slmInjectedObjects = list
    }

    Component {
        id: slmNativeActionComponent
        Action {
            property var menuRow: ({})
            property bool _slmIsNativeAction: true
            readonly property string rowActionId: String((menuRow && menuRow.actionId) ? menuRow.actionId : "")
            text: String((menuRow && menuRow.label) ? menuRow.label : rowActionId)
            icon.name: {
                var iconA = String((menuRow && menuRow.icon) ? menuRow.icon : "")
                if (iconA.length > 0) {
                    return iconA
                }
                return String((menuRow && menuRow.iconName) ? menuRow.iconName : "")
            }
            enabled: rowActionId.length > 0
            onTriggered: {
                if (rowActionId.length > 0) {
                    root.hostRoot.invokeSlmContextAction(rowActionId)
                }
            }
        }
    }

    Component {
        id: slmNativeSubmenuComponent
        Menu {
            property var menuRow: ({})
            property bool _slmIsNativeMenu: true
            property var _slmDynamicItems: []
            title: String((menuRow && menuRow.label) ? menuRow.label : "Group")
            function rebuild() {
                root.slmClearNativeMenuEntries(this)
                var nodeIdValue = String((menuRow && menuRow.id) ? menuRow.id : "")
                if (nodeIdValue.length <= 0) {
                    return
                }
                root.slmPopulateNativeMenu(this, nodeIdValue)
            }
            onMenuRowChanged: rebuild()
            onAboutToShow: {
                if ((_slmDynamicItems || []).length <= 0) {
                    rebuild()
                }
            }
            Component.onCompleted: rebuild()
            Component.onDestruction: root.slmClearNativeMenuEntries(this)
        }
    }

    Component {
        id: slmSeparatorComponent
        MenuSeparator {}
    }

    Component {
        id: slmShareActionItemComponent
        MenuItem {
            id: shareActionItem
            property var menuRow: ({})
            readonly property string actionId: String((menuRow && menuRow.actionId) ? menuRow.actionId : "")
            readonly property string iconNameValue: String((menuRow && menuRow.iconName) ? menuRow.iconName : "")
            readonly property string iconSourceValue: String((menuRow && menuRow.iconSource) ? menuRow.iconSource : "")
            readonly property bool rowActiveHover: (shareActionItem.highlighted || shareActionItem.hovered)
            text: String((menuRow && menuRow.name) ? menuRow.name : actionId)
            hoverEnabled: true
            leftPadding: 10
            rightPadding: 10
            contentItem: Row {
                spacing: 8
                leftPadding: 2

                Image {
                    width: 16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    source: {
                        if (shareActionItem.iconSourceValue.length > 0) {
                            return shareActionItem.iconSourceValue
                        }
                        if (shareActionItem.iconNameValue.length > 0) {
                            return "image://themeicon/" + shareActionItem.iconNameValue + "?v=" + root.iconRevision
                        }
                        return ""
                    }
                    visible: source.toString().length > 0
                }

                Label {
                    text: shareActionItem.text
                    color: shareActionItem.rowActiveHover ? root.menuColor("textOnAccent", "#ffffff")
                                                          : root.menuColor("menuText", "#202124")
                    font.pixelSize: root.menuFontPx("body", 14)
                    verticalAlignment: Text.AlignVCenter
                }
            }
            background: Rectangle {
                radius: root.menuRadius(7)
                color: shareActionItem.rowActiveHover ? root.menuColor("accent", "#2b7cff")
                                                      : "transparent"
                border.width: Theme.borderWidthNone
                Behavior on color {
                    ColorAnimation {
                        duration: 100
                        easing.type: Easing.OutCubic
                    }
                }
            }
            enabled: actionId.length > 0
            onTriggered: {
                if (actionId.length > 0) {
                    root.hostRoot.invokeSlmCapabilityAction("Share", actionId)
                }
            }
        }
    }

    Menu {
        id: fileEntryMenu
        modal: false
        property var _slmInjectedObjects: []
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent
        onClosed: root.slmClearInjectedObjects(fileEntryMenu)

        MenuItem {
            text: "Open"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.openContextEntry()
        }

        Menu {
            title: "Open with"
            enabled: root.hostRoot.contextEntryIndex >= 0
            readonly property var defaultRow: root.hostRoot.contextDefaultOpenWithEntry()
            readonly property string defaultAppId: String((defaultRow && defaultRow.id) ? defaultRow.id : "")
            readonly property var recommendedRows: root.hostRoot.contextRecommendedOpenWithEntries()
            function computedRows() {
                var rows = []
                if (defaultAppId.length > 0) {
                    rows.push({
                        "separator": false,
                        "appId": defaultAppId,
                        "name": "Default: " + String((defaultRow && defaultRow.name) ? defaultRow.name : defaultAppId),
                        "iconName": String((defaultRow && defaultRow.iconName) ? defaultRow.iconName : "")
                    })
                }
                for (var i = 0; i < recommendedRows.length; ++i) {
                    var r = recommendedRows[i] || ({})
                    var rid = String((r && r.id) ? r.id : "")
                    if (rid.length <= 0) {
                        continue
                    }
                    rows.push({
                        "separator": false,
                        "appId": rid,
                        "name": String((r && r.name) ? r.name : rid),
                        "iconName": String((r && r.iconName) ? r.iconName : "")
                    })
                }
                if (rows.length > 0) {
                    rows.push({ "separator": true })
                }
                rows.push({
                    "separator": false,
                    "appId": "__other__",
                    "name": "Other Application...",
                    "iconName": ""
                })
                return rows
            }

            Instantiator {
                model: fileEntryMenuOpenWith.computedRows()
                delegate: Loader {
                    required property var modelData
                    active: true
                    sourceComponent: (modelData && modelData.separator) ? fileEntrySepComp : fileEntryItemComp
                    onLoaded: {
                        if (item && item.hasOwnProperty("menuRow")) {
                            item.menuRow = modelData || ({})
                        }
                    }
                }

                Component {
                    id: fileEntrySepComp
                    MenuSeparator {}
                }

                Component {
                    id: fileEntryItemComp
                    MenuItem {
                        property var menuRow: ({})
                        text: String((menuRow && menuRow.name) ? menuRow.name : "")
                        icon.name: {
                            var n = String((menuRow && menuRow.iconName) ? menuRow.iconName : "")
                            return n.length > 0 ? n : ""
                        }
                        onTriggered: {
                            var appId = String((menuRow && menuRow.appId) ? menuRow.appId : "")
                            if (appId === "__other__") {
                                root.hostRoot.openInOtherApplication()
                                return
                            }
                            if (appId.length > 0) {
                                root.hostRoot.openContextEntryInApp(appId)
                            }
                        }
                    }
                }

                onObjectAdded: function(index, object) {
                    fileEntryMenuOpenWith.insertItem(index, object)
                }
                onObjectRemoved: function(index, object) {
                    fileEntryMenuOpenWith.removeItem(object)
                }
            }
            id: fileEntryMenuOpenWith
        }

        MenuSeparator {}

        MenuItem {
            text: "Cut"
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.copySelected(true)
        }

        MenuItem {
            text: "Copy"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.copySelected(false)
        }

        MenuItem {
            text: "Copy as Link"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.copySelectedAsLinkToClipboard()
        }

        MenuItem {
            text: "Select All"
            onTriggered: root.hostRoot.selectAllVisibleEntries()
        }

        MenuItem {
            text: "Invert Selection"
            onTriggered: root.hostRoot.invertSelection()
        }

        MenuSeparator {
            visible: root.hostRoot.recentView && root.hostRoot.contextEntryIndex < 0
        }

        MenuItem {
            visible: root.hostRoot.recentView && root.hostRoot.contextEntryIndex < 0
            text: "Clear Recents"
            onTriggered: root.hostRoot.requestClearRecentFiles()
        }

        MenuSeparator {}

        MenuItem {
            text: "Paste"
            enabled: root.hostRoot.clipboardPath.length > 0
            onTriggered: {
                fileEntryMenu.close()
                Qt.callLater(function() {
                    root.hostRoot.pasteIntoCurrent()
                })
            }
        }

        MenuItem {
            text: "Copy to..."
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.chooseDestinationForSelection(false)
        }

        MenuItem {
            text: "Move to..."
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.chooseDestinationForSelection(true)
        }

        MenuSeparator {}

        MenuItem {
            text: "Move to Trash"
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.moveContextEntryToTrash()
        }

        MenuItem {
            text: "Rename..."
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.requestRenameContextEntry()
        }

        MenuSeparator {}

        MenuItem {
            text: "Copy Path"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.copySelectedPathToClipboard()
        }

        MenuItem {
            text: "Share..."
            enabled: root.slmShareRows().length > 0
            onTriggered: {
                if (root.hostRoot.shareSheetRef) {
                    root.hostRoot.shareSheetRef.open()
                }
            }
        }

        MenuItem {
            text: "Bagikan Folder..."
            enabled: root.hostRoot.contextEntryIndex >= 0 && !!root.hostRoot.contextEntryIsDir
            onTriggered: root.hostRoot.openFolderShareDialog(root.hostRoot.contextEntryPath)
        }

        MenuItem {
            text: "Print..."
            secondaryText: "Ctrl+P"
            enabled: root.hostRoot.canPrintSelection ? root.hostRoot.canPrintSelection() : false
            onTriggered: {
                root.hostRoot.requestPrintSelection()
                fileEntryMenu.close()
            }
        }



        MenuItem {
            text: "Send by Email"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.sendSelectionByEmail()
        }

        MenuSeparator {}

        MenuItem {
            text: "Compress"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.compressSelection()
        }

        MenuItem {
            text: "Send Files via Bluetooth"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.sendSelectionViaBluetooth()
        }

        MenuItem {
            text: "Properties"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.showPropertiesForSelection()
        }
    }

    Menu {
        id: folderEntryMenu
        modal: false
        property var _slmInjectedObjects: []
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent
        onClosed: root.slmClearInjectedObjects(folderEntryMenu)

        MenuItem {
            text: "Open"
            enabled: String(root.hostRoot.contextEntryPath || "").length > 0
            onTriggered: {
                if (root.hostRoot.contextEntryIndex >= 0) {
                    root.hostRoot.openContextEntry()
                } else if (String(root.hostRoot.contextEntryPath || "").length > 0) {
                    root.hostRoot.openPath(root.hostRoot.contextEntryPath)
                }
            }
        }

        Menu {
            title: "Open in"
            enabled: String(root.hostRoot.contextEntryPath || "").length > 0
            readonly property var defaultRow: root.hostRoot.contextDefaultOpenWithEntry()
            readonly property string defaultAppId: String((defaultRow && defaultRow.id) ? defaultRow.id : "")
            readonly property var recommendedRows: root.hostRoot.contextRecommendedOpenWithEntries()
            function computedRows() {
                var rows = []
                if (defaultAppId.length > 0) {
                    rows.push({
                        "separator": false,
                        "appId": defaultAppId,
                        "name": "Default: " + String((defaultRow && defaultRow.name) ? defaultRow.name : defaultAppId),
                        "iconName": String((defaultRow && defaultRow.iconName) ? defaultRow.iconName : "")
                    })
                }
                for (var i = 0; i < recommendedRows.length; ++i) {
                    var r = recommendedRows[i] || ({})
                    var rid = String((r && r.id) ? r.id : "")
                    if (rid.length <= 0) {
                        continue
                    }
                    rows.push({
                        "separator": false,
                        "appId": rid,
                        "name": String((r && r.name) ? r.name : rid),
                        "iconName": String((r && r.iconName) ? r.iconName : "")
                    })
                }
                if (rows.length > 0) {
                    rows.push({ "separator": true })
                }
                rows.push({
                    "separator": false,
                    "appId": "__other__",
                    "name": "Other Application...",
                    "iconName": ""
                })
                return rows
            }

            Instantiator {
                model: folderEntryMenuOpenIn.computedRows()
                delegate: Loader {
                    required property var modelData
                    active: true
                    sourceComponent: (modelData && modelData.separator) ? folderEntrySepComp : folderEntryItemComp
                    onLoaded: {
                        if (item && item.hasOwnProperty("menuRow")) {
                            item.menuRow = modelData || ({})
                        }
                    }
                }

                Component {
                    id: folderEntrySepComp
                    MenuSeparator {}
                }

                Component {
                    id: folderEntryItemComp
                    MenuItem {
                        property var menuRow: ({})
                        text: String((menuRow && menuRow.name) ? menuRow.name : "")
                        icon.name: {
                            var n = String((menuRow && menuRow.iconName) ? menuRow.iconName : "")
                            return n.length > 0 ? n : ""
                        }
                        onTriggered: {
                            var appId = String((menuRow && menuRow.appId) ? menuRow.appId : "")
                            if (appId === "__other__") {
                                root.hostRoot.openInOtherApplication()
                                return
                            }
                            if (appId.length > 0) {
                                root.hostRoot.openContextEntryInApp(appId)
                            }
                        }
                    }
                }

                onObjectAdded: function(index, object) {
                    folderEntryMenuOpenIn.insertItem(index, object)
                }
                onObjectRemoved: function(index, object) {
                    folderEntryMenuOpenIn.removeItem(object)
                }
            }
            id: folderEntryMenuOpenIn
        }

        MenuSeparator {}

        MenuItem {
            text: "Open in New Tab"
            enabled: String(root.hostRoot.contextEntryPath || "").length > 0
            onTriggered: root.hostRoot.openContextEntryInNewTab()
        }

        MenuItem {
            text: "Open in New Window"
            enabled: String(root.hostRoot.contextEntryPath || "").length > 0
            onTriggered: root.hostRoot.openContextEntryInNewWindow()
        }

        MenuSeparator {}

        MenuItem {
            text: "Cut"
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.copySelected(true)
        }

        MenuItem {
            text: "Copy"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.copySelected(false)
        }

        MenuItem {
            text: "Copy as Link"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.copySelectedAsLinkToClipboard()
        }

        MenuItem {
            text: "Select All"
            onTriggered: root.hostRoot.selectAllVisibleEntries()
        }

        MenuItem {
            text: "Invert Selection"
            onTriggered: root.hostRoot.invertSelection()
        }

        MenuSeparator {}

        MenuItem {
            text: "Paste"
            enabled: root.hostRoot.clipboardPath.length > 0
            onTriggered: {
                folderEntryMenu.close()
                Qt.callLater(function() {
                    root.hostRoot.pasteIntoCurrent()
                })
            }
        }

        MenuItem {
            text: "Copy to..."
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.chooseDestinationForSelection(false)
        }

        MenuItem {
            text: "Move to..."
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.chooseDestinationForSelection(true)
        }

        MenuSeparator {}

        MenuItem {
            text: "Move to Trash"
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.moveContextEntryToTrash()
        }

        MenuItem {
            text: "Rename..."
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.requestRenameContextEntry()
        }

        MenuItem {
            text: "Add to Bookmarks"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.addContextEntryToBookmarks()
        }

        MenuSeparator {}

        MenuItem {
            text: "Copy Path"
            enabled: String(root.hostRoot.contextEntryPath || "").length > 0
            onTriggered: root.hostRoot.copySelectedPathToClipboard()
        }

        MenuItem {
            text: "Share..."
            enabled: root.slmShareRows().length > 0
            onTriggered: {
                if (root.hostRoot.shareSheetRef) {
                    root.hostRoot.shareSheetRef.open()
                }
            }
        }

        MenuItem {
            text: "Bagikan Folder..."
            enabled: String(root.hostRoot.contextEntryPath || "").length > 0
                     && !!root.hostRoot.contextEntryIsDir
            onTriggered: root.hostRoot.openFolderShareDialog(root.hostRoot.contextEntryPath)
        }

        MenuSeparator {}



        MenuItem {
            text: "Send by Email"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.sendSelectionByEmail()
        }

        MenuSeparator {}

        MenuItem {
            text: "Compress"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.compressSelection()
        }

        MenuItem {
            text: "Send Files via Bluetooth"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.sendSelectionViaBluetooth()
        }

        MenuItem {
            text: "Properties"
            enabled: String(root.hostRoot.contextEntryPath || "").length > 0
            onTriggered: root.hostRoot.showPropertiesForSelection()
        }
    }

    Menu {
        id: multiSelectionMenu
        modal: false
        property var _slmInjectedObjects: []
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent
        onClosed: root.slmClearInjectedObjects(multiSelectionMenu)

        MenuItem {
            text: "Open"
            enabled: (root.hostRoot.selectedEntryIndexes && root.hostRoot.selectedEntryIndexes.length > 1)
            onTriggered: root.hostRoot.openSelectedEntries()
        }

        MenuItem {
            text: "Open in New Tab"
            enabled: root.hostRoot.selectedAllDirectories()
            onTriggered: root.hostRoot.openSelectedInNewTabs()
        }

        MenuItem {
            text: "Open in New Window"
            enabled: root.hostRoot.selectedAllDirectories()
            onTriggered: root.hostRoot.openSelectedInNewWindows()
        }

        MenuSeparator {}

        MenuItem {
            text: "Cut"
            enabled: !root.hostRoot.selectedHasProtectedPath()
            onTriggered: root.hostRoot.copySelected(true)
        }

        MenuItem {
            text: "Copy"
            onTriggered: root.hostRoot.copySelected(false)
        }

        MenuItem {
            text: "Copy as Link"
            onTriggered: root.hostRoot.copySelectedAsLinkToClipboard()
        }

        MenuItem {
            text: "Select All"
            onTriggered: root.hostRoot.selectAllVisibleEntries()
        }

        MenuItem {
            text: "Invert Selection"
            onTriggered: root.hostRoot.invertSelection()
        }

        MenuSeparator {}

        MenuItem {
            text: "Paste"
            enabled: root.hostRoot.clipboardPath.length > 0
            onTriggered: root.hostRoot.pasteIntoCurrent()
        }

        MenuItem {
            text: "Copy to..."
            enabled: root.hostRoot.selectedEntryIndexes && root.hostRoot.selectedEntryIndexes.length > 0
            onTriggered: root.hostRoot.chooseDestinationForSelection(false)
        }

        MenuItem {
            text: "Move to..."
            enabled: (root.hostRoot.selectedEntryIndexes && root.hostRoot.selectedEntryIndexes.length > 0)
                     && !root.hostRoot.selectedHasProtectedPath()
            onTriggered: root.hostRoot.chooseDestinationForSelection(true)
        }

        MenuSeparator {}

        MenuItem {
            text: "Move to Trash"
            enabled: !root.hostRoot.selectedHasProtectedPath()
            onTriggered: root.hostRoot.deleteSelected(false)
        }

        MenuItem {
            text: "Copy Paths"
            onTriggered: root.hostRoot.copySelectedPathToClipboard()
        }

        MenuItem {
            text: "Share..."
            enabled: root.slmShareRows().length > 0
            onTriggered: {
                if (root.hostRoot.shareSheetRef) {
                    root.hostRoot.shareSheetRef.open()
                }
            }
        }

        MenuSeparator {}


        MenuItem {
            text: "Send by Email"
            onTriggered: root.hostRoot.sendSelectionByEmail()
        }

        MenuSeparator {}

        MenuItem {
            text: "Compress"
            onTriggered: root.hostRoot.compressSelection()
        }

        MenuItem {
            text: "Send Files via Bluetooth"
            onTriggered: root.hostRoot.sendSelectionViaBluetooth()
        }
        MenuItem {
            text: "Properties"
            onTriggered: root.hostRoot.showPropertiesForSelection()
        }

    }

    Menu {
        id: trashEntryMenu
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent

        MenuItem {
            text: "Restore from Trash"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.restoreSelectedFromTrash()
        }

        MenuItem {
            text: "Delete Permanently"
            enabled: root.hostRoot.contextEntryIndex >= 0 && !root.hostRoot.contextEntryProtected
            onTriggered: root.hostRoot.deleteSelected(true)
        }

        MenuSeparator {}

        MenuItem {
            text: "Copy Path"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.copySelectedPathToClipboard()
        }

        MenuItem {
            text: "Select All"
            onTriggered: root.hostRoot.selectAllVisibleEntries()
        }

        MenuSeparator {}

        MenuItem {
            text: "Properties"
            enabled: root.hostRoot.contextEntryIndex >= 0
            onTriggered: root.hostRoot.showPropertiesForSelection()
        }
    }

}
