import QtQuick
import QtQuick.Controls
import Slm_Desktop

Item {
    id: root

    property string desktopPath: ""
    property bool handleRightClick: false
    property alias menuVisible: desktopMenu.visible
    property double _lastPopupAtMs: 0
    property real _lastPopupX: -99999
    property real _lastPopupY: -99999

    signal newFolder()
    signal newTextFile()
    signal paste()
    signal arrangeIcons()
    signal snapToGrid()
    signal cleanUpDesktop()
    signal arrangeIconsBy(string sortKey, bool descending)
    signal changeWallpaper()
    signal openAppearance()
    signal openSettings()
    signal openTerminal()

    function openFolderMenu(x, y) {
        var nx = Number(x || 0)
        var ny = Number(y || 0)
        var nowMs = Date.now()
        if ((nowMs - Number(_lastPopupAtMs || 0)) < 140
                && Math.abs(nx - Number(_lastPopupX || 0)) <= 2
                && Math.abs(ny - Number(_lastPopupY || 0)) <= 2) {
            return
        }
        _lastPopupAtMs = nowMs
        _lastPopupX = nx
        _lastPopupY = ny
        desktopMenu.popup(nx, ny)
    }

    function closeMenu() {
        if (desktopMenu.visible) {
            desktopMenu.close()
        }
    }

    TapHandler {
        enabled: root.handleRightClick
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint) {
            root.openFolderMenu(eventPoint.position.x, eventPoint.position.y)
        }
    }

    Menu {
        id: desktopMenu
        parent: Overlay.overlay
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent

        Menu {
            title: "New"
            MenuItem {
                text: "Folder"
                onTriggered: root.newFolder()
            }
            MenuItem {
                text: "Text File"
                onTriggered: root.newTextFile()
            }
        }

        MenuItem {
            text: "Paste"
            enabled: (typeof ClipboardServiceClient !== "undefined"
                      && ClipboardServiceClient
                      && ClipboardServiceClient.hasContent === true)
            onTriggered: root.paste()
        }

        Menu {
            title: "Arrange Icons By"
            MenuItem {
                text: "Name"
                onTriggered: root.arrangeIconsBy("name", false)
            }
            MenuItem {
                text: "Date Modified"
                onTriggered: root.arrangeIconsBy("dateModified", true)
            }
            MenuItem {
                text: "Size"
                onTriggered: root.arrangeIconsBy("size", true)
            }
            MenuSeparator {}
            MenuItem {
                text: "Snap to Grid"
                onTriggered: root.snapToGrid()
            }
            MenuItem {
                text: "Clean Up Desktop"
                onTriggered: root.cleanUpDesktop()
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Change Wallpaper..."
            onTriggered: root.changeWallpaper()
        }
        MenuItem {
            text: "Appearance..."
            onTriggered: root.openAppearance()
        }

        MenuSeparator {}

        MenuItem {
            text: "Settings"
            onTriggered: root.openSettings()
        }
        MenuItem {
            text: "Open Terminal Here"
            onTriggered: root.openTerminal()
        }
    }
}
