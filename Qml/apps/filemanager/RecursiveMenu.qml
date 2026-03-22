import QtQuick 2.15
import QtQuick.Controls 2.15

Menu {
    id: recursiveMenu

    property var menuController: null
    property string nodeId: "root"
    property var _dynamicEntries: []

    function menuRowsForNode(idValue) {
        if (!menuController) {
            return []
        }
        var id = String(idValue || "root")
        if (id === "root" && menuController.menuRootItems) {
            return menuController.menuRootItems() || []
        }
        if (id === "root" && menuController.rootItems) {
            return menuController.rootItems() || []
        }
        if (menuController.menuChildren) {
            return menuController.menuChildren(id) || []
        }
        if (menuController.children) {
            return menuController.children(id) || []
        }
        return []
    }

    function clearDynamicEntries() {
        for (var i = _dynamicEntries.length - 1; i >= 0; --i) {
            var entry = _dynamicEntries[i]
            var parentMenu = entry.parent
            var object = entry.object
            var kind = String((entry && entry.kind) ? entry.kind : "")
            if (!object) {
                continue
            }
            if (parentMenu) {
                if (kind === "menu" && parentMenu.removeMenu) {
                    parentMenu.removeMenu(object)
                } else if (kind === "action" && parentMenu.removeAction) {
                    parentMenu.removeAction(object)
                } else if (parentMenu.removeItem) {
                    parentMenu.removeItem(object)
                }
            }
            object.destroy()
        }
        _dynamicEntries = []
    }

    function trackDynamic(parentMenu, object, kind) {
        var next = _dynamicEntries.slice(0)
        next.push({
                      "parent": parentMenu,
                      "object": object,
                      "kind": String(kind || "")
                  })
        _dynamicEntries = next
    }

    function populateInto(menuObject, idValue) {
        var rows = menuRowsForNode(idValue)
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            var type = String((row && row.type) ? row.type : "")
            if (type === "submenu") {
                var submenu = submenuComp.createObject(recursiveMenu, {
                                                          "title": String((row && row.label) ? row.label : "Group"),
                                                          "nodeIdInternal": String((row && row.id) ? row.id : "")
                                                      })
                if (!submenu) {
                    continue
                }
                if (menuObject.addMenu) {
                    menuObject.addMenu(submenu)
                    trackDynamic(menuObject, submenu, "menu")
                    populateInto(submenu, submenu.nodeIdInternal)
                } else {
                    submenu.destroy()
                }
                continue
            }

            var actionId = String((row && row.actionId) ? row.actionId : "")
            if (actionId.length <= 0) {
                continue
            }
            var item = actionComp.createObject(recursiveMenu, {
                                                  "textLabel": String((row && row.label) ? row.label : actionId),
                                                  "iconName": String((row && row.icon) ? row.icon : ""),
                                                  "actionId": actionId
                                              })
            if (!item) {
                continue
            }
            if (menuObject.addAction) {
                menuObject.addAction(item)
                trackDynamic(menuObject, item, "action")
            } else {
                item.destroy()
            }
        }
    }

    function rebuildMenu() {
        clearDynamicEntries()
        populateInto(recursiveMenu, nodeId)
    }

    onAboutToShow: rebuildMenu()
    Component.onDestruction: clearDynamicEntries()

    Component {
        id: submenuComp
        Menu {
            property string nodeIdInternal: "root"
            modal: false
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent
        }
    }

    Component {
        id: actionComp
        Action {
            property string textLabel: ""
            property string iconName: ""
            property string actionId: ""
            text: textLabel
            icon.name: iconName
            enabled: actionId.length > 0
            onTriggered: {
                if (actionId.length > 0 && recursiveMenu.menuController && recursiveMenu.menuController.invoke) {
                    recursiveMenu.menuController.invoke(actionId)
                }
            }
        }
    }
}
