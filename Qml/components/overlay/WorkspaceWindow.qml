import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../workspace" as WorkspaceComp

Window {
    id: root

    required property var rootWindow
    required property var desktopScene

    visible: !!rootWindow && !!desktopScene && rootWindow.visible && ShellState.workspaceOverviewVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint
    transientParent: rootWindow
    title: "Desktop Workspace"
    x: rootWindow ? rootWindow.x : 0
    y: rootWindow ? rootWindow.y : 0
    width: rootWindow ? rootWindow.width : 0
    height: rootWindow ? rootWindow.height : 0

    WorkspaceComp.WorkspaceOverlay {
        anchors.fill: parent
        active: root.visible
        onDismissed: {
            if (desktopScene) {
                desktopScene.workspaceVisible = false
            }
        }
    }
}
