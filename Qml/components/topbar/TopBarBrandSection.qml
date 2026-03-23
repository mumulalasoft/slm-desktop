import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import Style as DSStyle

Row {
    id: root
    spacing: 6
    property var fileManagerContent: null

    function printMenuEnabled() {
        var content = root.fileManagerContent
        if (!content || !content.canPrintSelection) {
            return false
        }
        return !!content.canPrintSelection()
    }

    function patchSectionItems(menuId, items) {
        var src = items || []
        var out = []
        var canPrint = root.printMenuEnabled()
        for (var i = 0; i < src.length; ++i) {
            var row = src[i] || ({})
            var next = {}
            var keys = Object.keys(row)
            for (var k = 0; k < keys.length; ++k) {
                var key = keys[k]
                next[key] = row[key]
            }
            if (Number(menuId || -1) === 2001 && Number(next.id || -1) === 6) {
                next.enabled = canPrint
            }
            out.push(next)
        }
        return out
    }

    Label {
        text: "SLM"
        color: Theme.color("textOnGlass")
        font.pixelSize: Theme.fontSize("bodyLarge")
        font.weight: Theme.fontWeight("bold")
    }

    Row {
        spacing: 6
        visible: typeof GlobalMenuManager !== "undefined" &&
                 GlobalMenuManager &&
                 GlobalMenuManager.available &&
                 GlobalMenuManager.topLevelMenus &&
                 GlobalMenuManager.topLevelMenus.length > 0

        Repeater {
            model: (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager)
                   ? GlobalMenuManager.topLevelMenus : []
            delegate: ToolButton {
                required property var modelData
                property int topMenuId: Number(modelData && modelData.id !== undefined ? modelData.id : -1)
                property var sectionItems: []
                text: modelData && modelData.label ? modelData.label : ""
                enabled: modelData && modelData.enabled !== false
                font.pixelSize: Theme.fontSize("bodyLarge")
                onClicked: {
                    if (!(typeof GlobalMenuManager !== "undefined" &&
                          GlobalMenuManager &&
                          modelData && modelData.id !== undefined)) {
                        return
                    }
                    sectionItems = root.patchSectionItems(topMenuId,
                                                          GlobalMenuManager.menuItemsFor(
                                                              topMenuId))
                    if (sectionItems && sectionItems.length > 0) {
                        sectionMenu.x = Math.round(parent.mapToGlobal(0, 0).x)
                        sectionMenu.y = Math.round(parent.mapToGlobal(0, parent.height + Theme.metric("spacingSm")).y)
                        sectionMenu.open()
                        return
                    }
                    GlobalMenuManager.activateMenu(modelData.id)
                }
                background: Rectangle {
                    radius: Theme.radiusMd
                    color: parent.hovered ? Theme.color("accentSoft") : "transparent"
                }

                Menu {
                    id: sectionMenu
                    modal: false
                    focus: false
                    dim: false
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                    background: DSStyle.PopupSurface {
                        implicitWidth: Theme.metric("popupWidthS")
                        implicitHeight: 40
                        popupOpacity: Theme.popupSurfaceOpacityStrong
                    }

                    Instantiator {
                        model: parent.sectionItems || []
                        delegate: Loader {
                            required property var modelData
                            active: true
                            sourceComponent: (modelData && modelData.separator)
                                             ? separatorComp : itemComp
                            onLoaded: {
                                if (!item) {
                                    return
                                }
                                if (item.hasOwnProperty("menuId")) {
                                        item.menuId = topMenuId
                                }
                                if (item.hasOwnProperty("itemId")) {
                                    item.itemId = Number(parent.modelData.id || -1)
                                }
                                if (item.hasOwnProperty("text")) {
                                    item.text = String(parent.modelData.label || "")
                                }
                                if (item.hasOwnProperty("enabled")) {
                                    item.enabled = parent.modelData.enabled !== false
                                }
                                if (item.hasOwnProperty("checkable")) {
                                    item.checkable = parent.modelData.checkable === true
                                }
                                if (item.hasOwnProperty("checked")) {
                                    item.checked = parent.modelData.checked === true
                                }
                                if (item.hasOwnProperty("secondaryText")) {
                                    item.secondaryText = String(parent.modelData.shortcutText || "")
                                }
                                if (item.hasOwnProperty("icon") && item.icon) {
                                    item.icon.name = String(parent.modelData.iconName || "")
                                }
                                if (item.hasOwnProperty("iconSource")) {
                                    item.iconSource = String(parent.modelData.iconSource || "")
                                }
                            }
                        }

                        Component {
                            id: separatorComp
                            MenuSeparator {}
                        }

                        Component {
                            id: itemComp
                            MenuItem {
                                property int menuId: -1
                                property int itemId: -1
                                onTriggered: {
                                    if (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager) {
                                        GlobalMenuManager.activateMenuItem(menuId, itemId)
                                    }
                                }
                            }
                        }

                        onObjectAdded: function(index, object) {
                            sectionMenu.insertItem(index, object)
                        }
                        onObjectRemoved: function(index, object) {
                            sectionMenu.removeItem(object)
                        }
                    }
                }
            }
        }
    }
}
