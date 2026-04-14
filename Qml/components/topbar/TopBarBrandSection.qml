import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "../globalmenu" as GlobalMenuComp

Row {
    id: root
    spacing: 4
    property var fileManagerContent: null
    property bool _focusReveal: false
    readonly property string _adaptiveMode: (typeof GlobalMenuAdaptiveController !== "undefined"
                                             && GlobalMenuAdaptiveController
                                             && GlobalMenuAdaptiveController.mode)
                                            ? String(GlobalMenuAdaptiveController.mode) : "full"
    readonly property string _activeAppId: (typeof GlobalMenuManager !== "undefined"
                                            && GlobalMenuManager
                                            && GlobalMenuManager.activeAppId)
                                           ? String(GlobalMenuManager.activeAppId) : ""
    readonly property bool _hasActiveApp: _activeAppId.length > 0
    readonly property bool _resumePending: (typeof GlobalMenuSuspendBridge !== "undefined"
                                            && GlobalMenuSuspendBridge
                                            && GlobalMenuSuspendBridge.resumePending)
                                           ? true : false

    function _systemFallbackMenus() {
        return [
                    { "id": 9001, "label": "System", "enabled": true, "source": "fallback-system" },
                    { "id": 9002, "label": "Applications", "enabled": true, "source": "fallback-system" },
                    { "id": 9003, "label": "Settings", "enabled": true, "source": "fallback-system" }
               ]
    }

    function _appFallbackMenus() {
        return [
                    { "id": 9101, "label": "File", "enabled": true, "source": "fallback-app" },
                    { "id": 9102, "label": "Edit", "enabled": true, "source": "fallback-app" },
                    { "id": 9103, "label": "View", "enabled": true, "source": "fallback-app" },
                    { "id": 9104, "label": "Help", "enabled": true, "source": "fallback-app" }
               ]
    }

    function _rawMenus() {
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.available
                && GlobalMenuManager.topLevelMenus
                && GlobalMenuManager.topLevelMenus.length > 0) {
            return GlobalMenuManager.topLevelMenus
        }
        return _hasActiveApp ? _appFallbackMenus() : _systemFallbackMenus()
    }

    function _moreCompactRows(menus) {
        var rows = []
        for (var i = 0; i < menus.length; ++i) {
            var row = menus[i] || ({})
            var id = Number(row.id || -1)
            var label = String(row.label || "")
            var lower = label.toLowerCase()
            if (lower === "file" || lower === "edit" || lower === "view") {
                continue
            }
            rows.push({
                         "id": 7000 + i,
                         "label": label,
                         "enabled": row.enabled !== false,
                         "targetMenuId": id,
                         "source": "compact-more"
                     })
        }
        return rows
    }

    function _effectiveMenus() {
        var menus = _rawMenus()
        if (_adaptiveMode === "compact") {
            var keep = []
            var haveFile = false
            var haveEdit = false
            var haveView = false
            for (var i = 0; i < menus.length; ++i) {
                var row = menus[i] || ({})
                var label = String(row.label || "").toLowerCase()
                if (!haveFile && label === "file") {
                    keep.push(row)
                    haveFile = true
                    continue
                }
                if (!haveEdit && label === "edit") {
                    keep.push(row)
                    haveEdit = true
                    continue
                }
                if (!haveView && label === "view") {
                    keep.push(row)
                    haveView = true
                    continue
                }
            }
            var moreRows = _moreCompactRows(menus)
            if (moreRows.length > 0) {
                keep.push({
                              "id": 9999,
                              "label": "More",
                              "enabled": true,
                              "source": "compact",
                              "moreItems": moreRows
                          })
            }
            return keep
        }
        return menus
    }

    function _fallbackMenuItems(menuId) {
        var id = Number(menuId || -1)
        if (id === 9001) {
            return [
                        { "id": 1, "label": "Overview", "enabled": true },
                        { "id": 2, "label": "Lock Screen", "enabled": true }
                   ]
        }
        if (id === 9002) {
            return [
                        { "id": 1, "label": "Open Launchpad", "enabled": true },
                        { "id": 2, "label": "Open Files", "enabled": true }
                   ]
        }
        if (id === 9003) {
            return [
                        { "id": 1, "label": "Open Settings", "enabled": true },
                        { "id": 2, "label": "Firewall Settings", "enabled": true }
                   ]
        }
        if (id === 9101) {
            return [
                        { "id": 1, "label": "Open...", "enabled": true },
                        { "id": 2, "label": "New Window", "enabled": true }
                   ]
        }
        if (id === 9102) {
            return [
                        { "id": 1, "label": "Copy", "enabled": true },
                        { "id": 2, "label": "Paste", "enabled": true }
                   ]
        }
        if (id === 9103) {
            return [
                        { "id": 1, "label": "Workspace Overview", "enabled": true }
                   ]
        }
        if (id === 9104) {
            return [
                        { "id": 1, "label": "Help Center", "enabled": true }
                   ]
        }
        return []
    }

    function _menuItemsFor(menuRow) {
        if (!menuRow) {
            return []
        }
        var menuId = Number(menuRow.id || -1)
        if (menuId === 9999 && menuRow.moreItems) {
            return menuRow.moreItems
        }
        var source = String(menuRow.source || "")
        if (source.indexOf("fallback-") === 0) {
            return _fallbackMenuItems(menuId)
        }
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.menuItemsFor) {
            return GlobalMenuManager.menuItemsFor(menuId)
        }
        return []
    }

    function _activateFallback(menuId, itemId) {
        var m = Number(menuId || -1)
        var i = Number(itemId || -1)
        if (m === 9001 && i === 1 && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("workspace.toggle", {}, "global-menu-fallback")
            return
        }
        if (m === 9001 && i === 2 && SessionStateClient && SessionStateClient.lock) {
            SessionStateClient.lock()
            return
        }
        if (m === 9002 && i === 1 && ShellStateController && ShellStateController.setLaunchpadVisible) {
            ShellStateController.setLaunchpadVisible(true)
            return
        }
        if (m === 9002 && i === 2 && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("filemanager.open", { "target": "~" }, "global-menu-fallback")
            return
        }
        if (m === 9003 && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopid",
                                   { "desktopId": "slm-settings.desktop" },
                                   "global-menu-fallback")
            return
        }
        if (m === 9101 && i === 1 && ShellStateController && ShellStateController.setToTheSpotVisible) {
            ShellStateController.setToTheSpotVisible(true)
            return
        }
        if (m === 9101 && i === 2 && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("filemanager.open", { "target": "~" }, "global-menu-fallback")
            return
        }
        if (m === 9103 && i === 1 && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("workspace.toggle", {}, "global-menu-fallback")
            return
        }
    }

    function _onCategoryActivated(menuRow, itemId) {
        if (!menuRow) {
            return
        }
        var menuId = Number(menuRow.id || -1)
        if (menuId === 9999) {
            return
        }
        if (menuRow.source === "compact" && itemId > 0) {
            return
        }
        if (menuRow.source === "compact-more") {
            var targetId = Number(menuRow.targetMenuId || -1)
            if (targetId > 0 && GlobalMenuManager && GlobalMenuManager.activateMenu) {
                GlobalMenuManager.activateMenu(targetId)
            }
            return
        }
        var source = String(menuRow.source || "")
        if (source.indexOf("fallback-") === 0) {
            _activateFallback(menuId, itemId)
            return
        }
        if (GlobalMenuManager && GlobalMenuManager.activateMenuItem) {
            GlobalMenuManager.activateMenuItem(menuId, itemId)
        }
    }

    function _openSettings() {
        if (AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopid",
                                   { "desktopId": "slm-settings.desktop" },
                                   "global-menu")
        }
    }

    Timer {
        id: focusHideTimer
        interval: 2200
        repeat: false
        onTriggered: root._focusReveal = false
    }

    Shortcut {
        sequence: "Meta+Alt+M"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root._adaptiveMode !== "focus") {
                return
            }
            root._focusReveal = !root._focusReveal
            if (!root._focusReveal) {
                focusHideTimer.stop()
            } else {
                focusHideTimer.restart()
            }
        }
    }

    HoverHandler {
        id: revealHover
        onHoveredChanged: {
            if (root._adaptiveMode !== "focus") {
                return
            }
            root._focusReveal = hovered
            if (hovered) {
                focusHideTimer.restart()
            } else {
                focusHideTimer.restart()
            }
        }
    }

    readonly property bool _menuVisible: _adaptiveMode !== "focus" || _focusReveal || _resumePending

    Connections {
        target: (typeof GlobalMenuSuspendBridge !== "undefined") ? GlobalMenuSuspendBridge : null
        ignoreUnknownSignals: true
        function onResumePendingChanged() {
            if (root._adaptiveMode === "focus" && root._resumePending) {
                root._focusReveal = true
                focusHideTimer.restart()
            }
        }
    }

    GlobalMenuComp.GlobalMenuAppIdentity {
        visible: root._menuVisible
        onRequestAbout: root._activateFallback(9104, 1)
        onRequestSettings: root._openSettings()
        onRequestShortcuts: {
            if (ShellStateController && ShellStateController.setToTheSpotVisible) {
                ShellStateController.setToTheSpotVisible(true)
            }
        }
        onRequestPermissions: root._openSettings()
        onRequestRestartApp: {
            if (AppCommandRouter && AppCommandRouter.route && root._activeAppId.length > 0) {
                AppCommandRouter.route("app.desktopid", { "desktopId": root._activeAppId }, "global-menu")
            }
        }
        onRequestQuitApp: {
            if (WindowingBackend && WindowingBackend.sendCommand) {
                WindowingBackend.sendCommand("close-view")
            }
        }
    }

    Row {
        visible: root._menuVisible
        spacing: 2

        Repeater {
            model: root._effectiveMenus()
            delegate: Item {
                id: categoryItem
                required property var modelData
                property bool popupOpen: dropdown.visible
                width: categoryButton.implicitWidth
                height: parent ? parent.height : 28

                GlobalMenuComp.GlobalMenuCategoryButton {
                    id: categoryButton
                    anchors.fill: parent
                    label: String(modelData.label || "")
                    menuEnabled: modelData.enabled !== false
                    active: categoryItem.popupOpen
                    onClicked: {
                        var rows = root._menuItemsFor(modelData)
                        if (!rows || rows.length <= 0) {
                            if (GlobalMenuManager && GlobalMenuManager.activateMenu) {
                                GlobalMenuManager.activateMenu(Number(modelData.id || -1))
                            }
                            return
                        }
                        dropdown.menuItems = rows
                        dropdown.open()
                    }
                }

                GlobalMenuComp.GlobalMenuDropdown {
                    id: dropdown
                    x: 0
                    y: categoryItem.height + Theme.metric("spacingXs")
                    menuId: Number(modelData.id || -1)
                    menuItems: []
                    onItemActivated: function(menuId, itemId) {
                        root._onCategoryActivated(modelData, itemId)
                    }
                }
            }
        }
    }
}
