import QtQuick 2.15
import QtQuick.Controls 2.15
import Desktop_Shell

Menu {
    id: root

    required property var hostRoot

    function openAt(px, py) {
        x = px
        y = py
        open()
    }

    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent

    MenuItem {
        text: "Open"
        enabled: hostRoot.sidebarContextCanOpenPath()
        onTriggered: hostRoot.openSidebarContextPath(hostRoot.sidebarContextPath)
    }

    MenuItem {
        text: "Open in New Tab"
        enabled: hostRoot.sidebarContextCanOpenPath()
        onTriggered: hostRoot.openSidebarContextPathInNewTab(hostRoot.sidebarContextPath)
    }

    MenuItem {
        text: "Open in New Window"
        enabled: hostRoot.sidebarContextCanOpenInNewWindow()
        onTriggered: hostRoot.openSidebarContextPathInNewWindow(hostRoot.sidebarContextPath)
    }

    MenuSeparator {
        visible: hostRoot.sidebarContextCanMount() || hostRoot.sidebarContextCanUnmount()
    }

    MenuItem {
        text: "Mount"
        visible: hostRoot.sidebarContextCanMount()
        enabled: visible
        onTriggered: hostRoot.sidebarContextMount()
    }

    MenuItem {
        text: "Unmount"
        visible: hostRoot.sidebarContextCanUnmount()
        enabled: visible
        onTriggered: hostRoot.sidebarContextUnmount()
    }

    MenuSeparator {
        visible: String(hostRoot.sidebarContextPath || "").length > 0
    }

    MenuItem {
        text: "Reveal in File Manager"
        enabled: hostRoot.sidebarContextCanOpenPath()
                 && String(hostRoot.sidebarContextPath || "").indexOf("__mount__:") !== 0
        onTriggered: hostRoot.sidebarContextRevealInFileManager()
    }

    MenuItem {
        text: "Copy Path"
        enabled: String(hostRoot.sidebarContextPath || "").length > 0
                 && String(hostRoot.sidebarContextPath || "").indexOf("__mount__:") !== 0
        onTriggered: hostRoot.sidebarContextCopyPath()
    }

    MenuSeparator {}

    MenuItem {
        text: "Refresh Sidebar"
        onTriggered: hostRoot.rebuildSidebarItems()
    }
}
