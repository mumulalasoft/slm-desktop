import QtQuick
import QtQuick.Controls
import Slm_Desktop
import SlmStyle as DSStyle

// GlobalMenuDropdown — custom popup for global menu items.
//
// Unlike QtQuick Menu, this is a fully custom Popup that slides down + fades in
// for a modern feel. Supports keyboard navigation and item search.

Popup {
    id: root

    property int menuId: -1
    property var menuItems: []     // [{id, label, enabled, separator, iconName, shortcutText, checkable, checked}]
    property var menuService: null // GlobalMenuManager reference

    signal itemActivated(int menuId, int itemId)

    // ── geometry ─────────────────────────────────────────────────────────────
    width: Math.max(200, contentColumn.implicitWidth + Theme.metric("spacingMd") * 2)
    height: contentColumn.implicitHeight + Theme.metric("spacingSm") * 2

    padding: 0
    modal: false
    focus: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
        implicitWidth: root.width
        implicitHeight: root.height
        popupOpacity: Theme.popupSurfaceOpacityStrong
    }

    // ── keyboard navigation ───────────────────────────────────────────────────
    property int _focusedIndex: -1

    function _moveFocus(delta) {
        var count = root.menuItems.length
        if (count === 0) return
        var next = root._focusedIndex + delta
        // Skip separators
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
        root.close()
        root.itemActivated(root.menuId, Number(it.id || 0))
    }

    onOpened: {
        if (keyScope) {
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
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                _activateFocused()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                close()
                event.accepted = true
            }
        }

        implicitWidth: contentColumn.implicitWidth
        implicitHeight: contentColumn.implicitHeight

        Column {
            id: contentColumn
            // Use parent.width (contentItem), not root.width, to avoid a direct
            // cross-component binding that triggers a QQuickItem::polish() loop.
            width: parent.width
            topPadding: Theme.metric("spacingSm")
            bottomPadding: Theme.metric("spacingSm")

            Repeater {
                model: root.menuItems

                delegate: Loader {
                    required property var modelData
                    required property int index
                    width: parent.width

                    sourceComponent: (modelData && modelData.separator) ? sepComp : itemComp

                    Component {
                        id: sepComp
                        Rectangle {
                            anchors.left: parent ? parent.left : undefined
                            anchors.right: parent ? parent.right : undefined
                            anchors.leftMargin: Theme.metric("spacingMd")
                            anchors.rightMargin: Theme.metric("spacingMd")
                            height: 1
                            color: Theme.color("divider")
                            opacity: Theme.opacitySeparator
                        }
                    }

                    Component {
                        id: itemComp
                        Item {
                            id: itemRow
                            width: parent ? parent.width : 0
                            implicitHeight: Theme.metric("controlHeightCompact")
                            implicitWidth: {
                                var lp = Theme.metric("spacingMd") + 14 + Theme.metric("spacingXs")
                                var rp = Theme.metric("spacingMd")
                                var iw = itemIcon.visible ? (itemIcon.width + Theme.metric("spacingXs")) : 0
                                var sw = shortcutLabel.text.length > 0
                                         ? (Theme.metric("spacingMd") + shortcutLabel.implicitWidth) : 0
                                return Math.max(160, lp + iw + labelText.implicitWidth + sw + rp)
                            }

                        readonly property bool isFocused: root._focusedIndex === index
                        readonly property bool isEnabled: modelData ? modelData.enabled !== false : false

                        // Focused/hovered highlight — soft fill, not macOS-style
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.metric("spacingXs")
                            anchors.rightMargin: Theme.metric("spacingXs")
                            radius: Theme.radiusSm
                            color: Theme.color("accent")
                            opacity: (itemRow.isFocused || hov.hovered) && itemRow.isEnabled ? 0.12 : 0
                            Behavior on opacity {
                                NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
                            }
                        }

                        // Checkmark
                        Text {
                            id: checkMark
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.metric("spacingMd")
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            text: (modelData && modelData.checked) ? "\u2713" : ""
                            color: Theme.color("accent")
                            font.pixelSize: Theme.fontSize("bodySmall")
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
                            anchors.right: shortcutLabel.left
                            anchors.rightMargin: Theme.metric("spacingSm")
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData ? (modelData.label || "") : ""
                            color: itemRow.isEnabled
                                   ? Theme.color("textPrimary")
                                   : Theme.color("textDisabled")
                            font.pixelSize: Theme.fontSize("bodyLarge")
                            elide: Text.ElideRight
                        }

                        // Shortcut hint
                        Text {
                            id: shortcutLabel
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.metric("spacingMd")
                            anchors.verticalCenter: parent.verticalCenter
                            text: (modelData && modelData.shortcutText) ? modelData.shortcutText : ""
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("bodySmall")
                            opacity: Theme.opacityMuted
                        }

                        HoverHandler { id: hov }

                        TapHandler {
                            enabled: itemRow.isEnabled
                            onTapped: {
                                root.close()
                                if (modelData) {
                                    root.itemActivated(root.menuId, Number(modelData.id || 0))
                                }
                            }
                        }

                        // Update keyboard focus on hover
                            Connections {
                                target: hov
                                function onHoveredChanged() {
                                    if (hov.hovered) root._focusedIndex = index
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
