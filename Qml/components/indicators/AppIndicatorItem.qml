import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root

    property var modelData: null
    property var host: null

    readonly property string itemId: String((modelData && modelData.id) ? modelData.id : "")
    readonly property string iconName: String((modelData && modelData.iconName) ? modelData.iconName : "application-x-executable-symbolic")
    readonly property string iconSource: String((modelData && modelData.iconSource) ? modelData.iconSource : "")
    readonly property string itemTitle: String((modelData && modelData.title) ? modelData.title : "")
    readonly property bool passive: String((modelData && modelData.status) ? modelData.status : "").toLowerCase() === "passive"
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)
    readonly property string itemService: String((modelData && modelData.service) ? modelData.service : "")
    readonly property string itemMenuPath: String((modelData && modelData.menuPath) ? modelData.menuPath : "")
    property var trayMenuRows: []
    property int trayMenuCloseGuardMs: 0

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) return false
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) return true
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    function canUseDbusMenu() {
        return (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager &&
                root.itemService.length > 0 && root.itemMenuPath.length > 0)
    }

    function queryMenuChildren(menuId, service, path) {
        if (!canUseDbusMenu() || menuId <= 0 || !GlobalMenuManager.queryDbusMenuItems) {
            return []
        }
        var svc = String(service || root.itemService)
        var obj = String(path || root.itemMenuPath)
        return GlobalMenuManager.queryDbusMenuItems(svc, obj, menuId)
    }

    function refreshTopMenuRows() {
        if (!root.canUseDbusMenu() || !GlobalMenuManager.queryDbusMenuTopLevel) {
            return
        }
        var rows = GlobalMenuManager.queryDbusMenuTopLevel(root.itemService, root.itemMenuPath)
        if (rows && rows.length >= 0) {
            root.trayMenuRows = rows
        }
    }

    function createSubMenu(menuId, service, path, parentObject) {
        if (menuId <= 0) {
            return null
        }
        var submenu = submenuComponent.createObject(parentObject || root, {
            "sourceService": String(service || root.itemService),
            "sourcePath": String(path || root.itemMenuPath),
            "parentMenuId": menuId
        })
        return submenu
    }

    function applyMenuRow(item, row, service, path) {
        if (!item || !row) {
            return
        }
        if (item.hasOwnProperty("text")) {
            item.text = String(row.label || "")
        }
        if (item.hasOwnProperty("enabled")) {
            item.enabled = row.enabled !== false
        }
        if (item.hasOwnProperty("checkable")) {
            item.checkable = row.checkable === true
        }
        if (item.hasOwnProperty("checked")) {
            item.checked = row.checked === true
        }
        if (item.hasOwnProperty("secondaryText")) {
            item.secondaryText = String(row.shortcutText || "")
        }
        if (item.hasOwnProperty("icon") && item.icon) {
            item.icon.name = String(row.iconName || "")
        }
        if (item.hasOwnProperty("iconSource")) {
            item.iconSource = String(row.iconSource || "")
        }
        if (item.hasOwnProperty("menuId")) {
            item.menuId = Number(row.id || -1)
        }
        if (item.hasOwnProperty("sourceService")) {
            item.sourceService = String(service || root.itemService)
        }
        if (item.hasOwnProperty("sourcePath")) {
            item.sourcePath = String(path || root.itemMenuPath)
        }
        if (row.hasSubmenu === true && item.hasOwnProperty("subMenu")) {
            var menuId = Number(row.id || -1)
            if (menuId > 0) {
                item.subMenu = createSubMenu(menuId,
                                             String(service || root.itemService),
                                             String(path || root.itemMenuPath),
                                             item)
            }
        }
    }

    visible: !passive && itemId.length > 0
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    ToolTip.visible: trayMouse.containsMouse && itemTitle.length > 0
    ToolTip.text: itemTitle
    ToolTip.delay: 500

    Connections {
        target: (typeof GlobalMenuManager !== "undefined") ? GlobalMenuManager : null
        function onDbusMenuChanged(service, path) {
            if (!root.canUseDbusMenu()) {
                return
            }
            if (String(service) !== root.itemService || String(path) !== root.itemMenuPath) {
                return
            }
            if (trayMenuPopup.opened || trayMenuPopup.visible) {
                root.refreshTopMenuRows()
            }
        }
    }

    Rectangle {
        id: button
        implicitWidth: Theme.metric("controlHeightRegular")
        implicitHeight: Theme.metric("controlHeightRegular")
        anchors.centerIn: parent
        radius: Theme.radiusControl
        color: trayMouse.pressed ? Theme.color("controlBgPressed") : (trayMouse.containsMouse ? Theme.color("controlBgHover") : "transparent")
        border.width: Theme.borderWidthThin
        border.color: trayMouse.containsMouse ? Theme.color("panelBorder") : "transparent"
        Behavior on color { enabled: root.microAnimationAllowed(); ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }
        Behavior on border.color { enabled: root.microAnimationAllowed(); ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate } }

        Image {
            id: icon
            anchors.centerIn: parent
            width: 18
            height: 18
            fillMode: Image.PreserveAspectFit
            source: root.iconSource.length > 0
                    ? root.iconSource
                    : ("image://themeicon/" + root.iconName + "?v=" + root.iconRevision)
        }

        MouseArea {
            id: trayMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            cursorShape: Qt.PointingHandCursor

            onClicked: function(mouse) {
                if (!root.host || root.itemId.length === 0) {
                    return
                }
                var gx = Math.round(button.mapToGlobal(mouse.x, mouse.y).x)
                var gy = Math.round(button.mapToGlobal(mouse.x, mouse.y).y)
                if (mouse.button === Qt.RightButton) {
                    if (trayMenuPopup.opened || trayMenuPopup.visible) {
                        root.trayMenuCloseGuardMs = Date.now()
                        trayMenuPopup.close()
                        return
                    }
                    if ((Date.now() - root.trayMenuCloseGuardMs) < 180) {
                        return
                    }
                    var rows = []
                    if (root.canUseDbusMenu() && GlobalMenuManager.queryDbusMenuTopLevel) {
                        rows = GlobalMenuManager.queryDbusMenuTopLevel(root.itemService, root.itemMenuPath)
                    }
                    if (rows && rows.length > 0) {
                        root.trayMenuRows = rows
                        trayMenuPopup.x = Math.round(button.mapToGlobal(0, 0).x)
                        trayMenuPopup.y = Math.round(button.mapToGlobal(0, button.height + Theme.metric("spacingXs")).y)
                        trayMenuPopup.open()
                    } else {
                        root.host.contextMenu(root.itemId, gx, gy)
                    }
                } else if (mouse.button === Qt.MiddleButton) {
                    root.host.secondaryActivate(root.itemId, gx, gy)
                } else {
                    root.host.activate(root.itemId, gx, gy)
                }
            }
        }

        WheelHandler {
            id: trayWheel
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function(event) {
                if (!root.host || root.itemId.length === 0) {
                    return
                }
                var ax = Math.abs(event.angleDelta.x)
                var ay = Math.abs(event.angleDelta.y)
                var vertical = ay >= ax
                var delta = vertical ? event.angleDelta.y : event.angleDelta.x
                if (delta === 0) {
                    return
                }
                root.host.scroll(root.itemId, Math.round(delta / 120), vertical ? "vertical" : "horizontal")
                event.accepted = true
            }
        }
    }

    Menu {
        id: trayMenuPopup
        property var quickRows: []
        modal: false
        focus: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: DSStyle.PopupSurface {
            implicitWidth: Theme.metric("popupWidthS")
            implicitHeight: 40
            popupOpacity: Theme.popupSurfaceOpacityStrong
        }
        onAboutToShow: {
            if (typeof AppModel !== "undefined" && AppModel
                    && AppModel.slmQuickActionsForEntry && root.itemId.length > 0) {
                quickRows = AppModel.slmQuickActionsForEntry("tray", {
                    "desktopId": root.itemId
                }) || []
            }
        }
        onClosed: root.trayMenuCloseGuardMs = Date.now()

        Instantiator {
            model: trayMenuPopup.quickRows
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
                        "scope": "tray",
                        "source_app": "org.slm.tray",
                        "desktop_id": root.itemId
                    })
                }
            }
            onObjectAdded: function(index, object) {
                trayMenuPopup.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                trayMenuPopup.removeItem(object)
            }
        }

        Instantiator {
            model: (trayMenuPopup.quickRows && trayMenuPopup.quickRows.length > 0) ? [1] : []
            delegate: MenuSeparator {}
            onObjectAdded: function(index, object) {
                trayMenuPopup.insertItem(trayMenuPopup.quickRows.length, object)
            }
            onObjectRemoved: function(index, object) {
                trayMenuPopup.removeItem(object)
            }
        }

        Instantiator {
            model: root.trayMenuRows || []
            delegate: Loader {
                required property var modelData
                active: true
                sourceComponent: (modelData && modelData.separator) ? sepComp : itemComp
                onLoaded: {
                    root.applyMenuRow(item, modelData, root.itemService, root.itemMenuPath)
                }
            }

            Component {
                id: sepComp
                MenuSeparator {}
            }

            Component {
                id: itemComp
                MenuItem {
                    property int menuId: -1
                    property string sourceService: ""
                    property string sourcePath: ""
                    onTriggered: {
                        if (menuId <= 0) {
                            return
                        }
                        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager &&
                                sourceService.length > 0 && sourcePath.length > 0 &&
                                GlobalMenuManager.activateDbusMenuItem) {
                            GlobalMenuManager.activateDbusMenuItem(sourceService, sourcePath, menuId)
                            Qt.callLater(root.refreshTopMenuRows)
                        }
                    }
                }
            }

            onObjectAdded: function(index, object) {
                var offset = trayMenuPopup.quickRows.length + (trayMenuPopup.quickRows.length > 0 ? 1 : 0)
                trayMenuPopup.insertItem(offset + index, object)
            }
            onObjectRemoved: function(index, object) {
                trayMenuPopup.removeItem(object)
            }
        }
    }

    Component {
        id: submenuComponent
        Menu {
            property string sourceService: ""
            property string sourcePath: ""
            property int parentMenuId: -1
            property var rows: []

            modal: false
            focus: false
            dim: false
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            background: DSStyle.PopupSurface {
                implicitWidth: Theme.metric("popupWidthS")
                implicitHeight: 40
                popupOpacity: Theme.popupSurfaceOpacityStrong
            }

            onAboutToShow: {
                rows = root.queryMenuChildren(parentMenuId, sourceService, sourcePath)
            }

            Instantiator {
                model: parent.rows || []
                delegate: Loader {
                    required property var modelData
                    active: true
                    sourceComponent: (modelData && modelData.separator) ? sepComp : subItemComp
                    onLoaded: {
                        root.applyMenuRow(item, modelData, parent.sourceService, parent.sourcePath)
                    }
                }

                Component {
                    id: subItemComp
                    MenuItem {
                        property int menuId: -1
                        property string sourceService: ""
                        property string sourcePath: ""
                        onTriggered: {
                            if (menuId <= 0) {
                                return
                            }
                            if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager &&
                                    sourceService.length > 0 && sourcePath.length > 0 &&
                                    GlobalMenuManager.activateDbusMenuItem) {
                                GlobalMenuManager.activateDbusMenuItem(sourceService, sourcePath, menuId)
                                if (parent && parent.parentMenuId > 0) {
                                    parent.rows = root.queryMenuChildren(parent.parentMenuId, sourceService, sourcePath)
                                }
                                Qt.callLater(root.refreshTopMenuRows)
                            }
                        }
                    }
                }

                onObjectAdded: function(index, object) {
                    parent.insertItem(index, object)
                }
                onObjectRemoved: function(index, object) {
                    parent.removeItem(object)
                }
            }
        }
    }
}
