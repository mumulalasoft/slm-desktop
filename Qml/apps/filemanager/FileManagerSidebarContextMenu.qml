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
    property string contextDeviceSnapshot: ""
    property bool contextBookmarkRemovableSnapshot: false
    property bool canOpenSnapshot: false
    property bool canOpenInNewWindowSnapshot: false
    property bool canMountSnapshot: false
    property bool canUnmountSnapshot: false
    property bool canRemoveBookmarkSnapshot: false
    property bool canRevealSnapshot: false
    property bool canCopyPathSnapshot: false
    readonly property bool showMountSection: (canMountSnapshot && !canOpenSnapshot)
                                             || canUnmountSnapshot
    readonly property bool showPathActionsSection: canRevealSnapshot
                                                   || canCopyPathSnapshot
                                                   || canRemoveBookmarkSnapshot
    readonly property bool showPrimaryOpenGroup: canOpenSnapshot
                                                 || canOpenInNewWindowSnapshot

    function openAt(px, py) {
        contextPathSnapshot = String(hostRoot.sidebarContextPath || "")
        contextLabelSnapshot = String(hostRoot.sidebarContextLabel || "")
        contextRowTypeSnapshot = String(hostRoot.sidebarContextRowType || "")
        contextMountedSnapshot = !!hostRoot.sidebarContextMounted
        contextDeviceSnapshot = String(hostRoot.sidebarContextDevice || "")
        contextBookmarkRemovableSnapshot = !!hostRoot.sidebarContextBookmarkRemovable
        canOpenSnapshot = !!hostRoot.sidebarContextCanOpenPath()
        canOpenInNewWindowSnapshot = !!hostRoot.sidebarContextCanOpenInNewWindow()
        canMountSnapshot = !!hostRoot.sidebarContextCanMount()
        canUnmountSnapshot = !!hostRoot.sidebarContextCanUnmount()
        canRevealSnapshot = canOpenSnapshot
                           && contextPathSnapshot.indexOf("__mount__:") !== 0
        canCopyPathSnapshot = contextPathSnapshot.length > 0
                           && contextPathSnapshot.indexOf("__mount__:") !== 0
        canRemoveBookmarkSnapshot = contextRowTypeSnapshot === "item"
                                 && contextBookmarkRemovableSnapshot
                                 && contextPathSnapshot.length > 0
                                 && contextPathSnapshot !== "__recent__"
                                 && contextPathSnapshot !== "__network__"
                                 && contextPathSnapshot.indexOf("__mount__:") !== 0
        var xPos = Number(px || 0)
        var yPos = Number(py || 0)
        if (root.popupType === Popup.Window && hostRoot && hostRoot.mapToGlobal) {
            var g = hostRoot.mapToGlobal(xPos, yPos)
            xPos = Number(g.x || 0)
            yPos = Number(g.y || 0)
        }
        x = Math.round(xPos)
        y = Math.round(yPos)
        open()
    }

    popupType: Popup.Window
    modal: true
    closePolicy: Popup.CloseOnEscape
                 | Popup.CloseOnPressOutside
                 | Popup.CloseOnPressOutsideParent
                 | Popup.CloseOnReleaseOutside
                 | Popup.CloseOnReleaseOutsideParent

    DSStyle.MenuItem {
        text: contextRowTypeSnapshot === "storage"
              ? "Open Drive"
              : "Open"
        visible: canOpenSnapshot
        enabled: canOpenSnapshot
        height: visible ? implicitHeight : 0
        onTriggered: hostRoot.openPath(root.contextPathSnapshot)
    }

    DSStyle.MenuItem {
        text: "Open in New Tab"
        visible: canOpenSnapshot
        enabled: canOpenSnapshot
        height: visible ? implicitHeight : 0
        onTriggered: {
            hostRoot.openPathInNewTab(root.contextPathSnapshot)
        }
    }

    DSStyle.MenuItem {
        text: "Open in New Window"
        visible: canOpenInNewWindowSnapshot
        enabled: canOpenInNewWindowSnapshot
        height: visible ? implicitHeight : 0
        onTriggered: hostRoot.openInNewWindowRequested(root.contextPathSnapshot)
    }

    MenuSeparator {
        visible: showMountSection
        height: visible ? implicitHeight : 0
    }

    DSStyle.MenuItem {
        text: "Open Drive"
        // Only show when the first "Open" item can't already handle mounting
        // (i.e., path has no __mount__: prefix but device is known).
        visible: canMountSnapshot && !canOpenSnapshot
        enabled: visible
        height: visible ? implicitHeight : 0
        onTriggered: hostRoot.sidebarContextMountDevice(root.contextDeviceSnapshot)
    }

    DSStyle.MenuItem {
        text: "Eject"
        visible: canUnmountSnapshot
        enabled: visible
        height: visible ? implicitHeight : 0
        onTriggered: hostRoot.sidebarContextUnmount()
    }

    MenuSeparator {
        visible: showPathActionsSection
        height: visible ? implicitHeight : 0
    }

    DSStyle.MenuItem {
        text: "Reveal in File Manager"
        visible: canRevealSnapshot
        enabled: canRevealSnapshot
        height: visible ? implicitHeight : 0
        onTriggered: hostRoot.sidebarContextRevealInFileManager()
    }

    DSStyle.MenuItem {
        text: "Copy Path"
        visible: canCopyPathSnapshot
        enabled: canCopyPathSnapshot
        height: visible ? implicitHeight : 0
        onTriggered: hostRoot.sidebarContextCopyPath()
    }

    DSStyle.MenuItem {
        text: "Remove from Bookmarks"
        visible: canRemoveBookmarkSnapshot
        enabled: canRemoveBookmarkSnapshot
        height: visible ? implicitHeight : 0
        onTriggered: hostRoot.sidebarContextRemoveBookmark()
    }

    MenuSeparator {
        visible: showPrimaryOpenGroup || showMountSection || showPathActionsSection
        height: visible ? implicitHeight : 0
    }

    DSStyle.MenuItem {
        text: "Refresh Sidebar"
        onTriggered: hostRoot.rebuildSidebarItems()
    }
}
