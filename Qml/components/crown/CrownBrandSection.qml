import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "../globalmenu" as GlobalMenuComp

Row {
    id: root
    spacing: 4

    readonly property int _focusRevealHitWidth: Math.max(20, Theme.metric("spacingXl") * 2)
    width: _menuVisible ? implicitWidth : _focusRevealHitWidth
    height: (implicitHeight > 0) ? implicitHeight : (parent ? parent.height : Theme.metric("topBarHeight"))

    property var fileManagerContent: null
    property var desktopMenuProvider: null
    property bool _focusReveal: false

    // -1 = no category menu open; ≥0 = menuId of the open category dropdown.
    property int menuBarOpenId: -1
    readonly property bool anyPopupOpen: menuBarOpenId >= 0

    readonly property string _adaptiveMode: (typeof GlobalMenuAdaptiveController !== "undefined"
                                             && GlobalMenuAdaptiveController
                                             && GlobalMenuAdaptiveController.mode)
                                            ? String(GlobalMenuAdaptiveController.mode) : "full"
    readonly property string _activeAppId: (typeof GlobalMenuManager !== "undefined"
                                            && GlobalMenuManager
                                            && GlobalMenuManager.activeAppId)
                                           ? String(GlobalMenuManager.activeAppId) : ""
    readonly property bool _hasActiveApp: _activeAppId.length > 0
    readonly property string _effectiveAdaptiveMode: (_adaptiveMode === "focus" && !_hasActiveApp)
                                                     ? "full" : _adaptiveMode
    readonly property bool _resumePending: (typeof GlobalMenuSuspendBridge !== "undefined"
                                            && GlobalMenuSuspendBridge
                                            && GlobalMenuSuspendBridge.resumePending)
                                           ? true : false

    // ── fallback menu definitions ─────────────────────────────────────────────

    function _desktopFallbackMenus() {
        return [
            { "id": 9201, "label": "File",      "enabled": true, "source": "fallback-desktop" },
            { "id": 9202, "label": "Edit",      "enabled": true, "source": "fallback-desktop" },
            { "id": 9203, "label": "Go",        "enabled": true, "source": "fallback-desktop" },
            { "id": 9204, "label": "Workspace", "enabled": true, "source": "fallback-desktop" },
            { "id": 9205, "label": "Tools",     "enabled": true, "source": "fallback-desktop" },
            { "id": 9206, "label": "Help",      "enabled": true, "source": "fallback-desktop" }
        ]
    }

    function _appFallbackMenus() {
        return [
            { "id": 9101, "label": "File",      "enabled": true, "source": "fallback-app" },
            { "id": 9102, "label": "Edit",      "enabled": true, "source": "fallback-app" },
            { "id": 9103, "label": "View",      "enabled": true, "source": "fallback-app" },
            { "id": 9104, "label": "Tools",     "enabled": true, "source": "fallback-app" },
            { "id": 9105, "label": "Workspace", "enabled": true, "source": "fallback-app" },
            { "id": 9106, "label": "Help",      "enabled": true, "source": "fallback-app" }
        ]
    }

    function _sanitizeTopLevelMenus(rows) {
        var out = []
        if (!rows || rows.length === undefined) {
            return out
        }
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i]
            if (!row) {
                continue
            }
            var label = String(row.label || "").trim()
            var menuId = Number(row.id || -1)
            if (label.length <= 0 || menuId <= 0) {
                continue
            }
            out.push({
                "id": menuId,
                "label": label,
                "enabled": row.enabled !== false,
                "source": String(row.source || "")
            })
        }
        return out
    }

    function _rawMenus() {
        if (!_hasActiveApp && desktopMenuProvider && desktopMenuProvider.enabled
                && desktopMenuProvider.topLevelMenus) {
            var providerMenus = _sanitizeTopLevelMenus(desktopMenuProvider.topLevelMenus())
            if (providerMenus.length > 0) {
                return providerMenus
            }
        }
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.topLevelMenus) {
            var sanitized = _sanitizeTopLevelMenus(GlobalMenuManager.topLevelMenus)
            if (sanitized.length > 0) {
                return sanitized
            }
        }
        return _hasActiveApp ? _appFallbackMenus() : _desktopFallbackMenus()
    }

    function _moreCompactRows(menus) {
        var rows = []
        for (var i = 0; i < menus.length; ++i) {
            var row = menus[i] || ({})
            var label = String(row.label || "").toLowerCase()
            if (label === "file" || label === "edit" || label === "view") continue
            rows.push({
                "id": 7000 + i, "label": String(row.label || ""),
                "enabled": row.enabled !== false,
                "targetMenuId": Number(row.id || -1),
                "source": "compact-more"
            })
        }
        return rows
    }

    function _effectiveMenus() {
        var menus = _rawMenus()
        if (_effectiveAdaptiveMode === "compact") {
            var keep = []
            var haveFile = false, haveEdit = false, haveView = false
            var haveGo = false, haveWorkspace = false
            for (var i = 0; i < menus.length; ++i) {
                var row = menus[i] || ({})
                var label = String(row.label || "").toLowerCase()
                if (!haveFile  && label === "file")  { keep.push(row); haveFile  = true; continue }
                if (!haveEdit  && label === "edit")  { keep.push(row); haveEdit  = true; continue }
                if (!haveView  && label === "view")  { keep.push(row); haveView  = true; continue }
                if (!haveGo && label === "go") { keep.push(row); haveGo = true; continue }
                if (!haveWorkspace && label === "workspace") { keep.push(row); haveWorkspace = true; continue }
            }
            var more = _moreCompactRows(menus)
            if (more.length > 0) {
                keep.push({ "id": 9999, "label": "More", "enabled": true,
                            "source": "compact", "moreItems": more })
            }
            return keep
        }
        return menus
    }

    // ── fallback item lists ───────────────────────────────────────────────────

    function _fallbackMenuItems(menuId) {
        var id = Number(menuId || -1)
        if (id === 9001 || id === 9201) {
            return [
                { "id": 1, "label": "Open…", "enabled": true },
                { "id": -1, "separator": true },
                { "id": 2, "label": "New Window", "enabled": true }
            ]
        }
        if (id === 9002 || id === 9202) {
            return [
                { "id": 1, "label": "Copy", "enabled": true },
                { "id": 2, "label": "Paste", "enabled": true },
                { "id": -1, "separator": true },
                { "id": 3, "label": "Select All", "enabled": true }
            ]
        }
        if (id === 9003 || id === 9203) {
            return [
                { "id": 1, "label": "Home", "enabled": true },
                { "id": 2, "label": "Documents", "enabled": true }
            ]
        }
        if (id === 9204) {
            return [
                { "id": 1, "label": "Workspace Overview", "enabled": true },
                { "id": 2, "label": "Move to New Workspace", "enabled": true }
            ]
        }
        if (id === 9205) {
            return [
                { "id": 1, "label": "Open Settings", "enabled": true }
            ]
        }
        if (id === 9206) {
            return [
                { "id": 1, "label": "Help Center", "enabled": true }
            ]
        }
        if (id === 9101) {
            return [
                { "id": 1, "label": "Open…",       "enabled": true },
                { "id": 2, "label": "New Window",  "enabled": true }
            ]
        }
        if (id === 9102) {
            return [
                { "id": 1, "label": "Copy",       "enabled": true },
                { "id": 2, "label": "Paste",      "enabled": true },
                { "id": -1, "separator": true },
                { "id": 3, "label": "Select All", "enabled": true }
            ]
        }
        if (id === 9103) {
            return [
                { "id": 1, "label": "Workspace Overview", "enabled": true }
            ]
        }
        if (id === 9104) {
            return []   // placeholder — populated by real app via D-Bus
        }
        if (id === 9105) {
            return [
                { "id": 1, "label": "Workspace Overview",     "enabled": true },
                { "id": 2, "label": "Move to New Workspace",  "enabled": true }
            ]
        }
        if (id === 9106) {
            return [
                { "id": 1, "label": "Help Center", "enabled": true }
            ]
        }
        return []
    }

    function _menuItemsFor(menuRow) {
        if (!menuRow) return []
        var menuId = Number(menuRow.id || -1)
        if (menuId === 9999 && menuRow.moreItems) return menuRow.moreItems
        var source = String(menuRow.source || "")
        if (desktopMenuProvider && desktopMenuProvider.menuItemsFor
                && source === "desktop-provider") {
            return desktopMenuProvider.menuItemsFor(menuId)
        }
        if (source.indexOf("fallback-") === 0) return _fallbackMenuItems(menuId)
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.menuItemsFor) {
            return GlobalMenuManager.menuItemsFor(menuId)
        }
        return []
    }

    function _rowsCount(rows) {
        if (!rows) {
            return 0
        }
        if (rows.length !== undefined) {
            return Math.max(0, Number(rows.length || 0))
        }
        if (rows.count !== undefined) {
            return Math.max(0, Number(rows.count || 0))
        }
        return 0
    }

    // ── fallback activation ───────────────────────────────────────────────────

    function _activateFallback(menuId, itemId) {
        var m = Number(menuId || -1)
        var i = Number(itemId || -1)
        var router = (typeof AppCommandRouter !== "undefined") ? AppCommandRouter : null
        var session = (typeof SessionStateClient !== "undefined") ? SessionStateClient : null
        var shell   = (typeof ShellStateController !== "undefined") ? ShellStateController : null

        if (m === 9001) {
            if (i === 1 && router) { router.route("workspace.toggle", {}, "global-menu-fallback"); return }
            if (i === 2 && session) { session.lock(); return }
        }
        if (m === 9002) {
            if (i === 1 && shell)  { shell.setAppDeckVisible(true); return }
            if (i === 2 && router) { router.route("filemanager.open", { "target": "~" }, "global-menu-fallback"); return }
            if (i === 3 && router) { router.route("app.desktopid", { "desktopId": "slm-settings.desktop" }, "global-menu-fallback"); return }
        }
        if (m === 9003 && router)  { router.route("app.desktopid", { "desktopId": "slm-settings.desktop" }, "global-menu-fallback"); return }
        if (m === 9101) {
            if (i === 1 && shell)  { shell.setPulseVisible(true); return }
            if (i === 2 && router) { router.route("filemanager.open", { "target": "~" }, "global-menu-fallback"); return }
        }
        if ((m === 9103 || m === 9105) && i === 1 && router) {
            router.route("workspace.toggle", {}, "global-menu-fallback")
        }
        if (m === 9201) {
            if (i === 1 && shell)  { shell.setPulseVisible(true); return }
            if (i === 2 && router) { router.route("filemanager.open", { "target": "~" }, "global-menu-fallback"); return }
        }
        if (m === 9203 && i === 1 && router) {
            router.route("filemanager.open", { "target": "~" }, "global-menu-fallback")
            return
        }
        if (m === 9204 && i === 1 && router) {
            router.route("workspace.toggle", {}, "global-menu-fallback")
            return
        }
        if (m === 9205 && i === 1 && router) {
            router.route("app.desktopid", { "desktopId": "slm-settings.desktop" }, "global-menu-fallback")
            return
        }
        if (m === 9206 && i === 1 && router) {
            router.route("app.desktopid", { "desktopId": "slm-settings.desktop" }, "global-menu-fallback")
            return
        }
    }

    function _onCategoryActivated(menuRow, itemId) {
        if (!menuRow) return
        var menuId = Number(menuRow.id || -1)
        if (menuId === 9999) return
        if (menuRow.source === "compact-more") {
            var targetId = Number(menuRow.targetMenuId || -1)
            if (targetId > 0 && typeof GlobalMenuManager !== "undefined"
                    && GlobalMenuManager && GlobalMenuManager.activateMenu) {
                GlobalMenuManager.activateMenu(targetId)
            }
            return
        }
        if (desktopMenuProvider && desktopMenuProvider.activateMenuItem
                && desktopMenuProvider.ownsMenu
                && desktopMenuProvider.ownsMenu(menuId)) {
            desktopMenuProvider.activateMenuItem(menuId, itemId)
            return
        }
        if (String(menuRow.source || "").indexOf("fallback-") === 0) {
            _activateFallback(menuId, itemId)
            return
        }
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.activateMenuItem) {
            GlobalMenuManager.activateMenuItem(menuId, itemId)
        }
    }

    function _openSettings() {
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopid", { "desktopId": "slm-settings.desktop" }, "global-menu")
        }
    }

    function _positionCategoryDropdown(dropdownObj, anchorObj) {
        if (!dropdownObj || !anchorObj) {
            return
        }
        if (dropdownObj.popupType === Popup.Window) {
            if (!anchorObj.mapToGlobal) {
                return
            }
            var g = anchorObj.mapToGlobal(0, anchorObj.height + Theme.metric("spacingSm"))
            dropdownObj.x = Math.round(Number(g.x || 0))
            dropdownObj.y = Math.round(Number(g.y || 0))
            return
        }
        dropdownObj.x = 0
        dropdownObj.y = Math.round(anchorObj.height + Theme.metric("spacingSm"))
    }

    // ── focus-mode reveal ─────────────────────────────────────────────────────

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
            if (root._effectiveAdaptiveMode !== "focus") return
            root._focusReveal = !root._focusReveal
            if (!root._focusReveal) focusHideTimer.stop()
            else focusHideTimer.restart()
        }
    }

    HoverHandler {
        id: revealHover
        onHoveredChanged: {
            if (root._effectiveAdaptiveMode !== "focus") return
            root._focusReveal = hovered
            focusHideTimer.restart()
        }
    }

    // Keep category menu bar deterministic and always visible.
    // Focus/compact modes may alter content density, but not visibility.
    readonly property bool _menuVisible: true

    Connections {
        target: (typeof GlobalMenuSuspendBridge !== "undefined") ? GlobalMenuSuspendBridge : null
        ignoreUnknownSignals: true
        function onResumePendingChanged() {
            if (root._effectiveAdaptiveMode === "focus" && root._resumePending) {
                root._focusReveal = true
                focusHideTimer.restart()
            }
        }
    }

    // ── category menu bar ─────────────────────────────────────────────────────

    Row {
        visible: root._menuVisible
        height: root.height
        spacing: 2

        Repeater {
            model: root._effectiveMenus()

            delegate: Item {
                id: categoryItem
                required property var modelData
                required property int index

                readonly property int _myMenuId: Number(modelData.id || -1)
                property bool popupOpen: dropdown.visible

                width: categoryButton.implicitWidth
                height: root.height

                // ── button ────────────────────────────────────────────────────
                GlobalMenuComp.GlobalMenuCategoryButton {
                    id: categoryButton
                    anchors.fill: parent
                    label: String(modelData.label || "")
                    menuEnabled: modelData.enabled !== false
                    active: categoryItem.popupOpen

                    onClicked: {
                        if (root.menuBarOpenId === categoryItem._myMenuId) {
                            root.menuBarOpenId = -1
                            openRetryTimer.stop()
                        } else {
                            var rows = root._menuItemsFor(modelData)
                            if (root._rowsCount(rows) <= 0) {
                                if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                                        && GlobalMenuManager.activateMenu) {
                                    GlobalMenuManager.activateMenu(categoryItem._myMenuId)
                                }
                                dropdown.menuItems = [
                                    { "id": -999001, "label": "Loading menu...", "enabled": false }
                                ]
                                root._positionCategoryDropdown(dropdown, categoryItem)
                                root.menuBarOpenId = categoryItem._myMenuId
                                openRetryAttempts = 0
                                openRetryTimer.restart()
                                return
                            }
                            openRetryTimer.stop()
                            dropdown.menuItems = rows
                            root._positionCategoryDropdown(dropdown, categoryItem)
                            root.menuBarOpenId = categoryItem._myMenuId
                        }
                    }
                }

                // ── hover-switch: move cursor → switch open menu instantly ────
                HoverHandler {
                    onHoveredChanged: {
                        if (!hovered || root.menuBarOpenId < 0
                                || root.menuBarOpenId === categoryItem._myMenuId) {
                            return
                        }
                        var rows = root._menuItemsFor(modelData)
                        if (root._rowsCount(rows) > 0) {
                            openRetryTimer.stop()
                            dropdown.menuItems = rows
                            root._positionCategoryDropdown(dropdown, categoryItem)
                            root.menuBarOpenId = categoryItem._myMenuId
                        }
                    }
                }

                property int openRetryAttempts: 0
                Timer {
                    id: openRetryTimer
                    interval: 80
                    repeat: true
                    running: false
                    onTriggered: {
                        if (root.menuBarOpenId >= 0 && root.menuBarOpenId !== categoryItem._myMenuId) {
                            stop()
                            return
                        }
                        var lateRows = root._menuItemsFor(modelData)
                        if (root._rowsCount(lateRows) > 0) {
                            dropdown.menuItems = lateRows
                            root._positionCategoryDropdown(dropdown, categoryItem)
                            root.menuBarOpenId = categoryItem._myMenuId
                            stop()
                            return
                        }
                        openRetryAttempts += 1
                        if (openRetryAttempts >= 8) {
                            stop()
                        }
                    }
                }

                // ── dropdown ──────────────────────────────────────────────────
                GlobalMenuComp.GlobalMenuDropdown {
                    id: dropdown
                    parent: categoryItem
                    popupType: Popup.Item
                    menuId: categoryItem._myMenuId
                    menuItems: []

                    onClosed: {
                        if (root.menuBarOpenId === categoryItem._myMenuId) {
                            root.menuBarOpenId = -1
                        }
                    }
                    onItemActivated: function(mId, itemId) {
                        root.menuBarOpenId = -1
                        root._onCategoryActivated(modelData, itemId)
                    }
                }

                // ── react to menuBarOpenId: open or close this dropdown ───────
                Connections {
                    target: root
                    function onMenuBarOpenIdChanged() {
                        if (root.menuBarOpenId === categoryItem._myMenuId) {
                            if (!dropdown.visible && root._rowsCount(dropdown.menuItems) > 0) {
                                Qt.callLater(function() {
                                    if (root.menuBarOpenId === categoryItem._myMenuId
                                            && !dropdown.visible
                                            && root._rowsCount(dropdown.menuItems) > 0) {
                                        root._positionCategoryDropdown(dropdown, categoryItem)
                                        dropdown.open()
                                    }
                                })
                            }
                        } else if (dropdown.visible) {
                            openRetryTimer.stop()
                            dropdown.close()
                        } else {
                            openRetryTimer.stop()
                        }
                    }
                }
            }
        }
    }
}
