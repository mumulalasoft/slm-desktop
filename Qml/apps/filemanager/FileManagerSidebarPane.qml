import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Slm_Desktop
import SlmStyle
import "../../components/style" as StyleComp

Rectangle {
    id: root

    required property var hostRoot
    required property var sidebarModel
    property var sidebarContextMenuRef: null
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)

    function dropTargetAt(sceneX, sceneY) {
        if (!sidebarList || !sidebarList.contentItem) {
            return ({ "ok": false })
        }
        var pos = hostRoot.mapToItem(sidebarList.contentItem, sceneX, sceneY)
        var idx = sidebarList.indexAt(pos.x, pos.y)
        if (idx < 0 || !sidebarModel || !sidebarModel.get) {
            return ({ "ok": false })
        }
        var row = sidebarModel.get(idx)
        if (!row) {
            return ({ "ok": false })
        }
        var rowType = String(row.rowType || "")
        var rowPath = String(row.path || "")
        var mounted = row.mounted === undefined ? true : !!row.mounted
        if (rowType === "section" || !mounted || rowPath.length <= 0) {
            return ({ "ok": false })
        }
        if (rowPath === "__recent__" || rowPath === "__network__"
                || rowPath.indexOf("__mount__:") === 0) {
            return ({ "ok": false })
        }
        return ({ "ok": true, "path": rowPath })
    }

    color: Theme.color("fileManagerSidebarBg")
    radius: Theme.radiusWindow
    border.width: Theme.borderWidthThin
    border.color: Theme.color("fileManagerSidebarBorder")
    antialiasing: true
    clip: true

    Rectangle {
        id: sidebarTopChrome
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: visible ? 52 : 0
        visible: !hostRoot.useNativeWindowDecoration
        color: "transparent"

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            StyleComp.WindowControlsCapsule {
                spacing: 0
                iconProvider: function(kind, hovered, pressed) {
                    return hostRoot.titleButtonIcon(kind, hovered, pressed)
                }
                onCloseRequested: hostRoot.closeRequested()
                onMinimizeRequested: hostRoot.minimizeWindow()
                onMaximizeRequested: hostRoot.toggleMaximizeWindow()
            }
        }
    }

    ListView {
        id: sidebarList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: sidebarTopChrome.bottom
        anchors.bottom: sidebarBottomBar.top
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 6
        anchors.bottomMargin: 8
        clip: true
        model: sidebarModel
        spacing: 1

        delegate: Item {
            required property string rowType
            required property string label
            required property string path
            required property string iconName
            required property string device
            required property bool mounted
            required property bool browsable
            required property real bytesTotal
            required property real bytesAvailable
            readonly property real bytesTotalValue: Number(
                                                        bytesTotal !== undefined ? bytesTotal : -1)
            readonly property real bytesAvailableValue: Number(
                                                            bytesAvailable !== undefined ? bytesAvailable : -1)
            readonly property real usageRatio: hostRoot.storageUsageRatio(
                                                   bytesAvailableValue,
                                                   bytesTotalValue)
            width: sidebarList.width
            height: rowType === "section" ? Math.max(
                                                24, Math.round(
                                                    hostRoot.sidebarMenuFontPx
                                                    * 1.50)) : Math.max(
                                                30, Math.round(
                                                    hostRoot.sidebarMenuFontPx * 1.75))

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMdPlus
                color: (rowType !== "section" && hostRoot.dndActive
                        && hostRoot.dndSidebarHoverPath
                        === path) ? Theme.color(
                                        "fileManagerTabActive") : ((rowType !== "section" && hostRoot.selectedSidebarPath === path) ? Theme.color("selectedItem") : ((rowType !== "section" && sidebarMouse.containsMouse) ? Theme.color("hoverItem") : "transparent"))

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    spacing: 6
                    visible: rowType !== "section"
                    opacity: 1.0

                    Image {
                        width: hostRoot.sidebarEntryIconPx
                        height: hostRoot.sidebarEntryIconPx
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: true
                        source: "image://themeicon/" + (iconName && iconName.length > 0 ? iconName : "folder-symbolic")
                                + "?v=" + root.iconRevision
                    }

                    Text {
                        width: sidebarList.width - 66
                        text: label
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("menu")
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: rowType === "section"
                    text: label
                    color: Theme.color("textSecondary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("caption")
                    font.weight: Theme.fontWeight("medium")
                    verticalAlignment: Text.AlignVCenter
                }

                Rectangle {
                    visible: rowType === "storage" && mounted
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 24
                    anchors.rightMargin: 52
                    height: 2
                    radius: Theme.radiusTiny
                    color: hostRoot.storageTrackColor(mounted)

                    Rectangle {
                        visible: mounted && usageRatio >= 0
                        width: Math.max(
                                   3, Math.round(
                                       parent.width * usageRatio))
                        height: parent.height
                        radius: Theme.radiusTiny
                        color: hostRoot.storageUsageColor(
                                   usageRatio)
                    }
                }

                Rectangle {
                    visible: rowType === "storage"
                    width: 22
                    height: 22
                    radius: Theme.radiusLg
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: mountActionMouse.containsMouse ? Theme.color("fileManagerTabCloseHover") : Theme.color("fileManagerControlBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("fileManagerControlBorder")

                    Image {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: true
                        source: "image://themeicon/" + (mounted ? "media-eject-symbolic" : "go-up-symbolic")
                                + "?v=" + root.iconRevision
                    }

                    MouseArea {
                        id: mountActionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: function(mouse) {
                            mouse.accepted = true
                            hostRoot.toggleStorageMount(path, device, mounted)
                        }
                    }
                }
            }

            MouseArea {
                id: sidebarMouse
                anchors.fill: parent
                enabled: rowType !== "section"
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        hostRoot.closeAllContextMenus()
                        hostRoot.sidebarContextPath = String(path || "")
                        hostRoot.sidebarContextLabel = String(label || "")
                        hostRoot.sidebarContextRowType = String(rowType || "")
                        hostRoot.sidebarContextDevice = String(device || "")
                        hostRoot.sidebarContextMounted = !!mounted
                        hostRoot.sidebarContextBrowsable = !!browsable
                        var p = sidebarMouse.mapToItem(hostRoot, mouse.x, mouse.y)
                        if (sidebarContextMenuRef && sidebarContextMenuRef.openAt) {
                            sidebarContextMenuRef.openAt(p.x, p.y)
                        }
                        return
                    }
                    hostRoot.openPath(path)
                }
            }
        }
    }

    Rectangle {
        id: sidebarBottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        anchors.bottomMargin: 1
        height: Math.max(40, Math.round(hostRoot.sidebarMenuFontPx * 2.20))
        color: "transparent"

        Rectangle {
            anchors.fill: parent
            color: Theme.color("fileManagerSidebarBg")
            radius: Theme.radiusWindow
            antialiasing: true
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.color("fileManagerSidebarBorder")
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMdPlus
            color: connectServerMouse.containsMouse ? Theme.color(
                                                          "hoverItem") : "transparent"
            border.width: Theme.borderWidthNone
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Image {
                width: hostRoot.sidebarEntryIconPx
                height: hostRoot.sidebarEntryIconPx
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                source: "image://themeicon/network-workgroup-symbolic?v=" + root.iconRevision
            }

            Text {
                text: "Connect Server..."
                color: Theme.color("textPrimary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("menu")
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: connectServerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                hostRoot.openConnectServerDialog()
            }
        }
    }
}
