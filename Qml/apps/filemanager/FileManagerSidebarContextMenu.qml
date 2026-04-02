import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

DSStyle.Menu {
    id: root

    required property var hostRoot
    property string contextPathSnapshot: ""
    property string contextLabelSnapshot: ""
    property string contextRowTypeSnapshot: ""
    property bool contextMountedSnapshot: false

    function openAt(px, py) {
        contextPathSnapshot = String(hostRoot.sidebarContextPath || "")
        contextLabelSnapshot = String(hostRoot.sidebarContextLabel || "")
        contextRowTypeSnapshot = String(hostRoot.sidebarContextRowType || "")
        contextMountedSnapshot = !!hostRoot.sidebarContextMounted
        console.log("[fm-sidebar-menu] openAt path=", contextPathSnapshot,
                    "label=", contextLabelSnapshot,
                    "rowType=", contextRowTypeSnapshot)
        x = px
        y = py
        open()
    }

    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent

    DSStyle.MenuItem {
        text: "Open"
        enabled: hostRoot.sidebarContextCanOpenPath()
        onTriggered: hostRoot.openPath(root.contextPathSnapshot)
    }

    DSStyle.MenuItem {
        text: "Open in New Tab"
        enabled: hostRoot.sidebarContextCanOpenPath()
        onTriggered: {
            console.log("[fm-sidebar-menu] open-new-tab path=", root.contextPathSnapshot,
                        "label=", root.contextLabelSnapshot)
            hostRoot.openPathInNewTab(root.contextPathSnapshot)
        }
    }

    DSStyle.MenuItem {
        text: "Open in New Window"
        enabled: hostRoot.sidebarContextCanOpenInNewWindow()
        onTriggered: hostRoot.openInNewWindowRequested(root.contextPathSnapshot)
    }

    MenuSeparator {
        visible: hostRoot.sidebarContextCanMount() || hostRoot.sidebarContextCanUnmount()
    }

    DSStyle.MenuItem {
        text: "Mount"
        visible: hostRoot.sidebarContextCanMount()
        enabled: visible
        onTriggered: hostRoot.sidebarContextMount()
    }

    DSStyle.MenuItem {
        text: "Unmount"
        visible: hostRoot.sidebarContextCanUnmount()
        enabled: visible
        onTriggered: hostRoot.sidebarContextUnmount()
    }

    MenuSeparator {
        visible: String(hostRoot.sidebarContextPath || "").length > 0
    }

    DSStyle.MenuItem {
        text: "Reveal in File Manager"
        enabled: hostRoot.sidebarContextCanOpenPath()
                 && String(hostRoot.sidebarContextPath || "").indexOf("__mount__:") !== 0
        onTriggered: hostRoot.sidebarContextRevealInFileManager()
    }

    DSStyle.MenuItem {
        text: "Copy Path"
        enabled: String(hostRoot.sidebarContextPath || "").length > 0
                 && String(hostRoot.sidebarContextPath || "").indexOf("__mount__:") !== 0
        onTriggered: hostRoot.sidebarContextCopyPath()
    }

    MenuSeparator {}

    DSStyle.MenuItem {
        text: "Refresh Sidebar"
        onTriggered: hostRoot.rebuildSidebarItems()
    }
}
