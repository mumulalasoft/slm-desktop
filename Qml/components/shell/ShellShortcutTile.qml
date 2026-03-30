import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var shellRoot
    property int slotIndex: -1
    property int modelIndex: -1
    property bool inputEnabled: true
    property bool contextMenuOnly: false
    property var entryMap: null
    property bool hasEntry: false
    property bool draggingThis: false
    property bool selected: false
    property bool dragActive: false
    property int dragToSlot: -1
    property int cellWidth: 104
    property int cellHeight: 122
    property int tileWidth: 96
    property int tileHeight: 108

    width: cellWidth
    height: cellHeight

    Item {
        anchors.centerIn: parent
        width: root.tileWidth
        height: root.tileHeight

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusCard
            color: (root.selected || (mouseArea.containsMouse && root.hasEntry)) ? Theme.color("accentSoft") : "transparent"
            border.width: (root.selected || (mouseArea.containsMouse && root.hasEntry) ||
                           (root.dragActive && root.dragToSlot === root.slotIndex))
                          ? Theme.borderWidthThin : Theme.borderWidthNone
            border.color: Theme.color("dragGhostBorder")
            opacity: root.draggingThis ? 0.15 : (root.hasEntry ? 1.0 : 0.0)

            Behavior on opacity {
                NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
            Behavior on color {
                ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8
            visible: root.hasEntry

            Rectangle {
                width: 58
                height: 58
                radius: Theme.radiusWindow
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.color("shellIconPlateBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("windowCardBorder")

                Image {
                    anchors.centerIn: parent
                    width: 38
                    height: 38
                    source: (root.entryMap && root.entryMap.iconName && root.entryMap.iconName.length > 0)
                            ? ("image://themeicon/" + root.entryMap.iconName + "?v=" +
                               ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                ? ThemeIconController.revision : 0))
                            : ((root.entryMap && root.entryMap.iconSource && root.entryMap.iconSource.length > 0)
                               ? root.entryMap.iconSource : "qrc:/icons/logo.svg")
                    fillMode: Image.PreserveAspectFit
                }
            }

            Label {
                width: parent.width
                text: (root.entryMap && root.entryMap.name) ? root.entryMap.name : ""
                color: Theme.color("selectedItemText")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: Theme.lineHeight("tight")
                maximumLineCount: 2
                elide: Text.ElideRight
                font.pixelSize: Theme.fontSize("small")
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            enabled: root.hasEntry && root.inputEnabled
            property bool leftPressed: false
            property bool dragArmed: false
            property bool suppressClick: false
            property real pressRootX: 0
            property real pressRootY: 0

            onPressed: function(mouse) {
                if (root.contextMenuOnly) {
                    leftPressed = false
                    dragArmed = false
                    suppressClick = false
                    return
                }
                leftPressed = (mouse.button === Qt.LeftButton)
                dragArmed = false
                suppressClick = false
                var px = (mouse.sceneX !== undefined) ? mouse.sceneX : mouseArea.mapToItem(root.shellRoot, mouse.x, mouse.y).x
                var py = (mouse.sceneY !== undefined) ? mouse.sceneY : mouseArea.mapToItem(root.shellRoot, mouse.x, mouse.y).y
                pressRootX = px
                pressRootY = py
                if (leftPressed) {
                    root.shellRoot.dragInProgress = true
                }
            }

            onPositionChanged: function(mouse) {
                if (!leftPressed || !mouseArea.pressed) {
                    return
                }
                var px = (mouse.sceneX !== undefined) ? mouse.sceneX : mouseArea.mapToItem(root.shellRoot, mouse.x, mouse.y).x
                var py = (mouse.sceneY !== undefined) ? mouse.sceneY : mouseArea.mapToItem(root.shellRoot, mouse.x, mouse.y).y
                var dx = px - pressRootX
                var dy = py - pressRootY
                if (!dragArmed && (dx * dx + dy * dy) >= 36) {
                    dragArmed = true
                    suppressClick = true
                    root.shellRoot.beginDrag(root.slotIndex, root.entryMap, px, py)
                }
                if (dragArmed) {
                    root.shellRoot.updateDrag(px, py)
                    var hoverDock = root.shellRoot.dockTopY > 0
                                    && py >= root.shellRoot.dockTopY
                                    && root.shellRoot.dragEntryData
                                    && root.shellRoot.dragEntryData.type === "desktop"
                                    && root.shellRoot.dragEntryData.desktopFile
                                    && root.shellRoot.dragEntryData.desktopFile.length > 0
                    root.shellRoot.dockDropHover(hoverDock, px,
                                                 root.shellRoot.dragEntryIconName &&
                                                 root.shellRoot.dragEntryIconName.length > 0
                                                 ? ("image://themeicon/" + root.shellRoot.dragEntryIconName + "?v=" +
                                                    ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                                     ? ThemeIconController.revision : 0))
                                                 : (root.shellRoot.dragEntryIconSource || ""))
                }
            }

            onReleased: function(mouse) {
                if (dragArmed) {
                    var px = (mouse.sceneX !== undefined) ? mouse.sceneX : mouseArea.mapToItem(root.shellRoot, mouse.x, mouse.y).x
                    var py = (mouse.sceneY !== undefined) ? mouse.sceneY : mouseArea.mapToItem(root.shellRoot, mouse.x, mouse.y).y
                    root.shellRoot.updateDrag(px, py)
                    var droppedInDockArea = root.shellRoot.dockTopY > 0 && py >= root.shellRoot.dockTopY
                    if (droppedInDockArea &&
                        root.shellRoot.dragEntryData &&
                        root.shellRoot.dragEntryData.type === "desktop" &&
                        root.shellRoot.dragEntryData.desktopFile &&
                        root.shellRoot.dragEntryData.desktopFile.length > 0) {
                        var previewIconPath = (root.shellRoot.dragEntryIconName &&
                                               root.shellRoot.dragEntryIconName.length > 0)
                                              ? ("image://themeicon/" + root.shellRoot.dragEntryIconName + "?v=" +
                                                 ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                                  ? ThemeIconController.revision : 0))
                                              : (root.shellRoot.dragEntryIconSource || "")
                        root.shellRoot.dockDropCommit(root.shellRoot.dragEntryData.desktopFile, px, previewIconPath)
                    } else {
                        root.shellRoot.dragToSlot = root.shellRoot.nearestSlotIndex(px, py)
                        root.shellRoot.moveEntryToSlot(root.shellRoot.dragFromSlot, root.shellRoot.dragToSlot)
                    }
                    root.shellRoot.dockDropClear()
                    root.shellRoot.clearDragState()
                    suppressClickTimer.restart()
                }
                root.shellRoot.dragInProgress = false
                leftPressed = false
                dragArmed = false
            }

            onCanceled: {
                root.shellRoot.dockDropClear()
                root.shellRoot.clearDragState()
                leftPressed = false
                dragArmed = false
                suppressClickTimer.restart()
            }

            onClicked: function(mouse) {
                if (suppressClick || !root.hasEntry) {
                    return
                }
                if (root.contextMenuOnly) {
                    if (mouse.button === Qt.RightButton) {
                        if (!root.shellRoot.isSelected(root.modelIndex)) {
                            root.shellRoot.selectSingle(root.modelIndex)
                        }
                        shortcutMenu.popup()
                    }
                    return
                }
                if (mouse.button === Qt.LeftButton) {
                    var additive = (mouse.modifiers & Qt.ControlModifier) !== 0
                                   || (mouse.modifiers & Qt.MetaModifier) !== 0
                    var rangeSelect = (mouse.modifiers & Qt.ShiftModifier) !== 0
                    if (rangeSelect) {
                        root.shellRoot.selectRangeTo(root.modelIndex, additive)
                    } else if (additive) {
                        root.shellRoot.toggleSelection(root.modelIndex)
                    } else {
                        root.shellRoot.selectSingle(root.modelIndex)
                    }
                } else if (mouse.button === Qt.RightButton) {
                    if (!root.shellRoot.isSelected(root.modelIndex)) {
                        root.shellRoot.selectSingle(root.modelIndex)
                    }
                    shortcutMenu.popup()
                }
            }

            onDoubleClicked: function(mouse) {
                if (root.contextMenuOnly) {
                    return
                }
                if (mouse.button !== Qt.LeftButton || !root.hasEntry) {
                    return
                }
                root.shellRoot.selectSingle(root.modelIndex)
                root.shellRoot.launchEntry(root.modelIndex, root.entryMap, "double-click")
            }
        }

        Timer {
            id: suppressClickTimer
            interval: 140
            repeat: false
            onTriggered: mouseArea.suppressClick = false
        }

        Menu {
            id: shortcutMenu
            MenuItem {
                text: "Open"
                onTriggered: {
                    if (root.hasEntry) {
                        root.shellRoot.selectSingle(root.modelIndex)
                        root.shellRoot.launchEntry(root.modelIndex, root.entryMap, "menu")
                    }
                }
            }
            MenuItem {
                text: "Add to Dock"
                enabled: root.hasEntry
                         && root.entryMap
                         && root.entryMap.type === "desktop"
                         && root.entryMap.desktopFile
                         && root.entryMap.desktopFile.length > 0
                onTriggered: {
                    if (root.entryMap.desktopFile && root.entryMap.desktopFile.length > 0) {
                        DockModel.addDesktopEntry(root.entryMap.desktopFile)
                        root.shellRoot.addToDockRequested(root.entryMap)
                    }
                }
            }
        }
    }
}
