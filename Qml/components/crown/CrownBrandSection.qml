import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "../globalmenu" as GlobalMenuComp

Row {
    id: root
    spacing: 4

    readonly property int _focusRevealHitWidth: Math.max(20, Theme.metric("spacingXl") * 2)
    width: _menuVisible ? implicitWidth : _focusRevealHitWidth
    height: Math.max(1, menuBarHeight)

    property var fileManagerContent: null
    property var desktopMenuProvider: null
    property bool _focusReveal: false
    property int menuBarHeight: Theme.metric("controlHeightCompact")

    // -1 = no category menu open; ≥0 = menuId of the open category dropdown.
    property int menuBarOpenId: -1
    readonly property bool anyPopupOpen: menuBarOpenId >= 0

    readonly property string _adaptiveMode: (typeof GlobalMenuAdaptiveController !== "undefined"
                                             && GlobalMenuAdaptiveController
                                             && GlobalMenuAdaptiveController.mode)
                                            ? String(GlobalMenuAdaptiveController.mode) : "full"
    readonly property string _activeAppId: {
        if (typeof AppStateClient !== "undefined" && AppStateClient && AppStateClient.focusedAppId) {
            var focused = String(AppStateClient.focusedAppId || "")
            if (focused.length > 0) {
                return focused
            }
        }
        if (typeof GlobalMenuManager !== "undefined"
                && GlobalMenuManager
                && GlobalMenuManager.activeAppId) {
            return String(GlobalMenuManager.activeAppId || "")
        }
        return ""
    }
    readonly property bool _hasActiveApp: _activeAppId.length > 0
    readonly property bool _hasRunningApps: (typeof AppStateClient !== "undefined"
                                             && AppStateClient
                                             && AppStateClient.runningAppIds)
                                            ? !!(AppStateClient.runningAppIds.length > 0) : false
    readonly property string _effectiveAdaptiveMode: (_adaptiveMode === "focus" && !_hasActiveApp)
                                                     ? "full" : _adaptiveMode
    readonly property bool _resumePending: (typeof GlobalMenuSuspendBridge !== "undefined"
                                            && GlobalMenuSuspendBridge
                                            && GlobalMenuSuspendBridge.resumePending)
                                           ? true : false

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
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.topLevelMenus) {
            var sanitized = _sanitizeTopLevelMenus(GlobalMenuManager.topLevelMenus)
            if (sanitized.length > 0) {
                return sanitized
            }
        }
        if (!_hasActiveApp && !_hasRunningApps
                && desktopMenuProvider && desktopMenuProvider.enabled
                && desktopMenuProvider.topLevelMenus) {
            var providerMenus = _sanitizeTopLevelMenus(desktopMenuProvider.topLevelMenus())
            if (providerMenus.length > 0) {
                return providerMenus
            }
        }
        return []
    }

    // Maximum number of menus to pin in the bar when compact mode is active.
    // Any menus beyond this count overflow into a "More" entry.
    // If ALL menus fit within this limit, no "More" entry is created and the
    // bar effectively behaves identically to full mode.
    readonly property int _compactPinCount: 5

    function _moreCompactRows(menus, keepIds) {
        // keepIds: a plain JS object used as a Set — keys are menu id strings
        // that are already pinned in the bar. Only menus NOT in keepIds go here.
        var rows = []
        for (var i = 0; i < menus.length; ++i) {
            var row = menus[i] || ({})
            var id  = String(row.id || "")
            if (keepIds[id]) continue
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

        // When there are few enough menus to fit comfortably, always show them
        // all regardless of what the adaptive controller reports.  This avoids
        // the "More" overflow pattern entirely for typical desktop-mode menus
        // (Desktop / Edit / Storage / Workspace = 4 items).  The threshold
        // _compactPinCount acts as the ceiling: ≤ pinCount → always full.
        if (_effectiveAdaptiveMode !== "compact" || menus.length <= _compactPinCount) {
            return menus
        }

        // Compact mode with more menus than the pin limit: pin the first
        // _compactPinCount menus and collect the rest into "More".
        // This is label-agnostic — no hardcoded menu names.
        var keep    = []
        var keepIds = {}
        for (var i = 0; i < Math.min(menus.length, _compactPinCount); ++i) {
            var row = menus[i] || ({})
            keep.push(row)
            keepIds[String(row.id || "")] = true
        }

        var more = _moreCompactRows(menus, keepIds)
        if (more.length > 0) {
            keep.push({ "id": 9999, "label": "More", "enabled": true,
                        "source": "compact", "moreItems": more })
        }
        return keep
    }

    function _menuItemsFor(menuRow) {
        if (!menuRow) return []
        var menuId = Number(menuRow.id || -1)
        if (menuId === 9999 && menuRow.moreItems) return menuRow.moreItems
        var source = String(menuRow.source || "")
        // Route to desktop provider when:
        //   (a) the row explicitly carries source="desktop-provider" (QML fallback path), OR
        //   (b) the provider claims ownership of this menuId (override path via C++ where
        //       the source field may not survive the QVariantMap round-trip).
        if (desktopMenuProvider && desktopMenuProvider.menuItemsFor
                && (source === "desktop-provider"
                    || (desktopMenuProvider.ownsMenu && desktopMenuProvider.ownsMenu(menuId)))) {
            return desktopMenuProvider.menuItemsFor(menuId)
        }
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.menuItemsFor) {
            return GlobalMenuManager.menuItemsFor(menuId)
        }
        return []
    }

    function _loadingMenuItems() {
        return [
            { "id": -999001, "label": "Loading menu...", "enabled": false }
        ]
    }

    function _emptyMenuItems(menuRow) {
        var label = menuRow ? String(menuRow.label || "Menu") : "Menu"
        return [
            { "id": -999002, "label": label + " has no menu items", "enabled": false }
        ]
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
        if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager
                && GlobalMenuManager.activateMenuItem) {
            GlobalMenuManager.activateMenuItem(menuId, itemId)
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
        height: root.menuBarHeight
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
                                dropdown.menuItems = root._loadingMenuItems()
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
                        if (openRetryAttempts >= 25) {
                            dropdown.menuItems = root._emptyMenuItems(modelData)
                            root._positionCategoryDropdown(dropdown, categoryItem)
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

                Connections {
                    target: (typeof GlobalMenuManager !== "undefined") ? GlobalMenuManager : null
                    ignoreUnknownSignals: true
                    function onChanged() {
                        if (root.menuBarOpenId !== categoryItem._myMenuId || !dropdown.visible) {
                            return
                        }
                        var refreshedRows = root._menuItemsFor(modelData)
                        if (root._rowsCount(refreshedRows) > 0) {
                            openRetryTimer.stop()
                            dropdown.menuItems = refreshedRows
                            root._positionCategoryDropdown(dropdown, categoryItem)
                        }
                    }
                }
            }
        }
    }
}
