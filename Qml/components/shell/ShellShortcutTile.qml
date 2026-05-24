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

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    width: cellWidth
    height: cellHeight

    function _isLocalFilePath(value) {
        var text = String(value || "").trim()
        return text.length > 0 && text.charAt(0) === "/"
    }

    function _providerLocalSource(value) {
        var raw = String(value || "").trim()
        if (raw.length <= 0) {
            return ""
        }
        if (raw.indexOf("image://themeicon/") === 0) {
            return raw
        }
        if (raw.indexOf("file://") === 0) {
            raw = raw.slice("file://".length)
        }
        if (_isLocalFilePath(raw)) {
            return "image://themeicon/" + encodeURIComponent(raw) + "?v="
                   + ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                      ? ThemeIconController.revision : 0)
        }
        return raw
    }

    function _themeIconSource(name) {
        var n = String(name || "").trim()
        var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                   ? ThemeIconController.revision : 0)
        if (n.length <= 0) {
            n = "text-x-generic-symbolic"
        }
        if (n.indexOf("image://themeicon/") === 0) {
            return n + (n.indexOf("?") >= 0 ? "&v=" : "?v=") + rev
        }
        return "image://themeicon/" + n + "?v=" + rev
    }

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
                enabled: root.microAnimationAllowed()
                NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
            Behavior on color {
                enabled: root.microAnimationAllowed()
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
                    id: shortcutIcon
                    anchors.centerIn: parent
                    width: 38
                    height: 38
                    source: {
                        var iconSource = String(root.entryMap && root.entryMap.iconSource ? root.entryMap.iconSource : "").trim()
                        if (iconSource.length > 0) {
                            if (iconSource.indexOf("image://themeicon/") === 0) {
                                return iconSource
                            }
                            if (iconSource.indexOf("file://") === 0 || iconSource.charAt(0) === "/") {
                                return root._providerLocalSource(iconSource)
                            }
                            if (iconSource.indexOf("image://") === 0 || iconSource.indexOf("qrc:/") === 0) {
                                return iconSource
                            }
                            return root._themeIconSource(iconSource)
                        }
                        var iconName = String(root.entryMap && root.entryMap.iconName ? root.entryMap.iconName : "").trim()
                        if (iconName.length > 0) {
                            if (iconName.indexOf("image://themeicon/") === 0) {
                                return iconName
                            }
                            if (iconName.indexOf("file://") === 0 || iconName.charAt(0) === "/") {
                                return root._providerLocalSource(iconName)
                            }
                            if (iconName.indexOf("image://") === 0 || iconName.indexOf("qrc:/") === 0) {
                                return iconName
                            }
                            return root._themeIconSource(iconName)
                        }
                        return "qrc:/icons/logo.svg"
                    }
                    fillMode: Image.PreserveAspectFit
                    onStatusChanged: {
                        if (status === Image.Error || status === Image.Null) {
                            console.warn("[shell-shortcut-icon] slot=" + String(root.slotIndex)
                                         + " modelIndex=" + String(root.modelIndex)
                                         + " name=" + String(root.entryMap && root.entryMap.name ? root.entryMap.name : "")
                                         + " iconName=" + String(root.entryMap && root.entryMap.iconName ? root.entryMap.iconName : "")
                                         + " iconSource=" + String(root.entryMap && root.entryMap.iconSource ? root.entryMap.iconSource : "")
                                         + " source=" + String(source || "")
                                         + " status=" + String(status))
                        }
                    }
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
            parent: Overlay.overlay
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
                text: "Add to AppDeck"
                enabled: root.hasEntry
                         && root.entryMap
                         && root.entryMap.type === "desktop"
                         && root.entryMap.desktopFile
                         && root.entryMap.desktopFile.length > 0
                onTriggered: {
                    if (root.entryMap.desktopFile && root.entryMap.desktopFile.length > 0) {
                        AppDeckModel.addDesktopEntry(root.entryMap.desktopFile)
                        root.shellRoot.addToDockRequested(root.entryMap)
                    }
                }
            }
        }
    }
}
