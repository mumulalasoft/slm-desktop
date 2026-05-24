import QtQuick
import QtQuick.Controls
import Slm_Desktop
import SlmStyle as DSStyle

// GlobalMenuDropdown — custom popup for global menu items.
//
// Unlike QtQuick Menu, this is a fully custom Popup that slides down + fades in
// for a modern feel. Supports keyboard navigation, item search, and nested
// submenus.
//
// menuItems schema (per row, all fields optional except id+label):
//   { id, label, enabled, separator, iconName, iconSource,
//     shortcutText, checkable, checked, submenu: [ ...same schema... ] }
//
// When `submenu` is a non-empty array, the item renders a trailing ›
// chevron and opens a nested GlobalMenuDropdown to its right on hover or tap.
// Submenu item activation is propagated to the root popup via itemActivated,
// using the SAME menuId — so providers can use a flat itemId namespace.

Popup {
    id: root

    property int menuId: -1
    property var menuItems: []
    property var menuService: null

    // True when this instance is itself a submenu of another dropdown.
    // Used to skip the "first-open auto-focus" behavior so the focus does not
    // jump to the child while the user is still scanning parent items.
    property bool isSubmenu: false

    signal itemActivated(int menuId, int itemId)

    // ── geometry ─────────────────────────────────────────────────────────────
    readonly property int minPopupWidth: 168
    readonly property int popupHPadding: Theme.metric("spacingXs") * 2
    readonly property int popupVPadding: Theme.metric("spacingXs") * 2
    implicitWidth: Math.max(minPopupWidth, Math.round(Number(contentColumn.width || 0) + popupHPadding))
    width: implicitWidth
    implicitHeight: Math.max(1, Math.round(Number(contentColumn.implicitHeight || 0) + popupVPadding))
    height: implicitHeight

    padding: 0
    modal: false
    focus: true
    dim: false
    closePolicy: Popup.CloseOnEscape
                 | Popup.CloseOnPressOutside
                 | Popup.CloseOnPressOutsideParent

    // ── enter / exit transitions ──────────────────────────────────────────────
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
            NumberAnimation { property: "y"; from: root.y - Theme.metric("spacingSm"); to: root.y; duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationSm; easing.type: Theme.easingAccelerate }
        }
    }

    // ── background ────────────────────────────────────────────────────────────
    background: DSStyle.PopupSurface {
        implicitWidth: root.implicitWidth
        implicitHeight: root.implicitHeight
        popupRadius: Theme.radiusWindowAlt
        popupColor: Theme.color("menuBg")
        popupBorderColor: Theme.color("menuBorder")
        popupOpacity: Theme.popupSurfaceOpacityStrong
        elevation: "high"
    }

    // ── submenu coordination ──────────────────────────────────────────────────
    // The index of the row whose submenu is currently open, or -1.
    property int _openSubmenuIndex: -1

    function _itemHasSubmenu(modelData) {
        if (!modelData) return false
        var sm = modelData.submenu
        if (!sm) return false
        if (typeof sm.length !== "number") return false
        return sm.length > 0
    }

    function _closeSubmenu() {
        root._openSubmenuIndex = -1
    }

    // Called by child submenu when one of its items is activated.
    // We propagate to our parent (or to subscribers of this root) and close.
    function _onChildActivated(mId, itemId) {
        root.itemActivated(mId, itemId)
        root.close()
    }

    onClosed: {
        _closeSubmenu()
    }

    // ── keyboard navigation ───────────────────────────────────────────────────
    property int _focusedIndex: -1

    function _moveFocus(delta) {
        var count = root.menuItems.length
        if (count === 0) return
        var next = root._focusedIndex + delta
        var iterations = 0
        while (iterations < count) {
            if (next < 0) next = count - 1
            if (next >= count) next = 0
            var it = root.menuItems[next]
            if (it && !it.separator && it.enabled !== false) break
            next += delta
            ++iterations
        }
        root._focusedIndex = next
    }

    function _activateFocused() {
        if (root._focusedIndex < 0 || root._focusedIndex >= root.menuItems.length) return
        var it = root.menuItems[root._focusedIndex]
        if (!it || it.separator) return
        if (_itemHasSubmenu(it)) {
            // Open the submenu instead of activating the parent row.
            root._openSubmenuIndex = root._focusedIndex
            return
        }
        root.close()
        root.itemActivated(root.menuId, Number(it.id || 0))
    }

    onOpened: {
        if (keyScope && !isSubmenu) {
            keyScope.forceActiveFocus()
        }
    }

    // ── content ───────────────────────────────────────────────────────────────
    contentItem: Item {
        id: keyScope
        focus: true

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Down) {
                _moveFocus(1)
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                _moveFocus(-1)
                event.accepted = true
            } else if (event.key === Qt.Key_Right) {
                // Open submenu if focused item has one.
                if (root._focusedIndex >= 0
                        && root._focusedIndex < root.menuItems.length
                        && _itemHasSubmenu(root.menuItems[root._focusedIndex])) {
                    root._openSubmenuIndex = root._focusedIndex
                    event.accepted = true
                }
            } else if (event.key === Qt.Key_Left) {
                if (root._openSubmenuIndex >= 0) {
                    root._closeSubmenu()
                    event.accepted = true
                }
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                _activateFocused()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                if (root._openSubmenuIndex >= 0) {
                    root._closeSubmenu()
                } else {
                    close()
                }
                event.accepted = true
            }
        }

        implicitWidth: contentColumn.width
        implicitHeight: contentColumn.implicitHeight

        Column {
            id: contentColumn
            width: Math.max(160, Number(contentColumn.implicitWidth || 0))
            topPadding: Theme.metric("spacingXs")
            bottomPadding: Theme.metric("spacingXs")

            Repeater {
                model: root.menuItems

                delegate: Loader {
                    required property var modelData
                    required property int index
                    sourceComponent: (modelData && modelData.separator) ? sepComp : itemComp

                    Component {
                        id: sepComp
                        Rectangle {
                            width: Math.max(160, Number(contentColumn.width || 0))
                            height: 1
                            color: Theme.color("menuSeparator")
                            opacity: Theme.opacitySeparator
                        }
                    }

                    Component {
                        id: itemComp
                        Item {
                            id: itemRow
                            width: Math.max(160, Number(contentColumn.width || 0))
                            implicitHeight: Theme.metric("controlHeightCompact")
                            implicitWidth: {
                                var lp = Theme.metric("spacingMd") + 14 + Theme.metric("spacingXs")
                                var rp = Theme.metric("spacingMd")
                                var iw = itemIcon.visible ? (itemIcon.width + Theme.metric("spacingXs")) : 0
                                var sw = shortcutLabel.text.length > 0
                                         ? (Theme.metric("spacingMd") + shortcutLabel.implicitWidth) : 0
                                var cw = chevron.visible ? (Theme.metric("spacingSm") + chevron.implicitWidth) : 0
                                return Math.max(160, lp + iw + labelText.implicitWidth + sw + cw + rp)
                            }

                            readonly property bool isFocused: root._focusedIndex === index
                            readonly property bool isEnabled: modelData ? modelData.enabled !== false : false
                            readonly property bool hasSubmenu: root._itemHasSubmenu(modelData)
                            readonly property bool submenuOpen: root._openSubmenuIndex === index

                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: 0
                                anchors.rightMargin: 0
                                radius: Theme.radiusSm
                                color: ((itemRow.isFocused || hov.hovered || itemRow.submenuOpen)
                                        && itemRow.isEnabled)
                                       ? Theme.color("menuHover") : "transparent"
                                Behavior on color {
                                    ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
                                }
                            }

                            // Checkmark
                            Text {
                                id: checkMark
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.metric("spacingMd")
                                anchors.verticalCenter: parent.verticalCenter
                                width: 14
                                text: (modelData && modelData.checked) ? "✓" : ""
                                color: itemRow.isEnabled
                                       ? ((itemRow.isFocused || hov.hovered)
                                          ? Theme.color("selectedItemText")
                                          : Theme.color("textSecondary"))
                                       : Theme.color("textDisabled")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("menu")
                                font.weight: Theme.fontWeight("bold")
                            }

                            // Icon
                            Image {
                                id: itemIcon
                                anchors.left: checkMark.right
                                anchors.leftMargin: Theme.metric("spacingXs")
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16; height: 16
                                source: (modelData && modelData.iconSource) ? modelData.iconSource : ""
                                visible: source.toString().length > 0
                                fillMode: Image.PreserveAspectFit
                            }

                            // Label
                            Text {
                                id: labelText
                                anchors.left: itemIcon.visible ? itemIcon.right : checkMark.right
                                anchors.leftMargin: Theme.metric("spacingXs")
                                anchors.right: itemRow.hasSubmenu
                                               ? chevron.left
                                               : (shortcutBadge.visible ? shortcutBadge.left : parent.right)
                                anchors.rightMargin: Theme.metric("spacingSm")
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData ? (modelData.label || "") : ""
                                color: itemRow.isEnabled
                                       ? ((itemRow.isFocused || hov.hovered)
                                          ? Theme.color("selectedItemText")
                                          : Theme.color("textPrimary"))
                                       : Theme.color("textDisabled")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("menu")
                                font.weight: (itemRow.isFocused || hov.hovered)
                                             ? Theme.fontWeight("medium")
                                             : Theme.fontWeight("normal")
                                elide: Text.ElideRight
                            }

                            // Chevron — visible only when item has a submenu
                            Text {
                                id: chevron
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.metric("spacingMd")
                                anchors.verticalCenter: parent.verticalCenter
                                visible: itemRow.hasSubmenu
                                text: "›"  // ›
                                color: itemRow.isEnabled
                                       ? ((itemRow.isFocused || hov.hovered || itemRow.submenuOpen)
                                          ? Theme.color("selectedItemText")
                                          : Theme.color("textSecondary"))
                                       : Theme.color("textDisabled")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("menu")
                                font.weight: Theme.fontWeight("medium")
                            }

                            // Shortcut hint (hidden when item has a submenu — chevron takes the slot)
                            Rectangle {
                                id: shortcutBadge
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.metric("spacingMd")
                                anchors.verticalCenter: parent.verticalCenter
                                visible: !itemRow.hasSubmenu && shortcutLabel.text.length > 0
                                implicitWidth: shortcutLabel.implicitWidth + 10
                                implicitHeight: Math.max(16, shortcutLabel.implicitHeight + 2)
                                radius: Theme.radiusSm
                                color: (itemRow.isFocused || hov.hovered)
                                       ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.22 : 0.28)
                                       : Theme.color("controlBg")
                                border.width: Theme.borderWidthThin
                                border.color: (itemRow.isFocused || hov.hovered)
                                              ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.45 : 0.38)
                                              : Theme.color("panelBorder")

                                Text {
                                    id: shortcutLabel
                                    anchors.centerIn: parent
                                    text: (modelData && modelData.shortcutText) ? modelData.shortcutText : ""
                                    color: itemRow.isEnabled
                                           ? ((itemRow.isFocused || hov.hovered)
                                              ? Theme.color("selectedItemText")
                                              : Theme.color("textSecondary"))
                                           : Theme.color("textDisabled")
                                    font.family: Theme.fontFamilyUi
                                    font.pixelSize: Theme.fontSize("tiny")
                                }
                            }

                            HoverHandler { id: hov }

                            // Submenu open-on-hover with a short dwell.
                            // Closes other submenus when the user moves onto a
                            // different item.
                            Timer {
                                id: submenuOpenTimer
                                interval: 140
                                repeat: false
                                onTriggered: {
                                    if (hov.hovered && itemRow.hasSubmenu && itemRow.isEnabled) {
                                        root._openSubmenuIndex = index
                                    }
                                }
                            }

                            Connections {
                                target: hov
                                function onHoveredChanged() {
                                    if (hov.hovered) {
                                        root._focusedIndex = index
                                        if (itemRow.hasSubmenu && itemRow.isEnabled) {
                                            submenuOpenTimer.restart()
                                        } else if (root._openSubmenuIndex >= 0
                                                   && root._openSubmenuIndex !== index) {
                                            // Moving to a non-submenu sibling: close the open submenu.
                                            root._closeSubmenu()
                                        }
                                    } else {
                                        submenuOpenTimer.stop()
                                    }
                                }
                            }

                            TapHandler {
                                enabled: itemRow.isEnabled
                                onTapped: {
                                    if (itemRow.hasSubmenu) {
                                        // Toggle submenu on tap.
                                        if (itemRow.submenuOpen) {
                                            root._closeSubmenu()
                                        } else {
                                            root._openSubmenuIndex = index
                                        }
                                        return
                                    }
                                    root.close()
                                    if (modelData) {
                                        root.itemActivated(root.menuId, Number(modelData.id || 0))
                                    }
                                }
                            }

                            // Nested submenu popup — anchored to right edge of this item.
                            // Loaded by URL (not by inline Component) to break QML's
                            // static recursive-instantiation check; the engine refuses
                            // to compile a Component that references its enclosing type
                            // by name, but resolving via source URL is allowed.
                            Loader {
                                id: submenuLoader
                                active: itemRow.hasSubmenu && itemRow.submenuOpen
                                source: "GlobalMenuDropdown.qml"
                                onLoaded: {
                                    if (!item) return
                                    item.parent = itemRow
                                    item.isSubmenu = true
                                    item.menuId = root.menuId
                                    item.menuItems = (modelData && modelData.submenu) ? modelData.submenu : []
                                    item.x = itemRow.width - Theme.metric("spacingXs")
                                    item.y = -Theme.metric("spacingXs")
                                    item.itemActivated.connect(function(mId, itemId) {
                                        root._onChildActivated(mId, itemId)
                                    })
                                    item.open()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
