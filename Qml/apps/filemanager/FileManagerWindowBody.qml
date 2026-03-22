import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property var hostRoot
    property var sidebarModel
    property var tabModel
    property var sidebarContextMenuRef
    property var fileManagerDialogsRef

    property alias sidebarPaneRef: sidebarPane
    property alias headerBarRef: headerBar
    property alias mainPaneRef: mainPane

    anchors.fill: parent
    anchors.margins: 0
    color: "transparent"
    radius: hostRoot ? hostRoot.radius : 0
    antialiasing: true
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            FileManagerSidebarPane {
                id: sidebarPane
                Layout.preferredWidth: 214
                Layout.fillHeight: true
                Layout.leftMargin: 16
                Layout.topMargin: 16
                Layout.bottomMargin: 16
                Layout.rightMargin: 16
                hostRoot: root.hostRoot
                sidebarModel: root.sidebarModel
                sidebarContextMenuRef: root.sidebarContextMenuRef
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                FileManagerHeaderBar {
                    id: headerBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 4
                    hostRoot: root.hostRoot
                    contentViewRef: mainPane ? mainPane.contentViewRef : null
                }

                FileManagerMainPane {
                    id: mainPane
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    hostRoot: root.hostRoot
                    tabModel: root.tabModel
                    fileManagerDialogsRef: root.fileManagerDialogsRef
                }
            }
        }
    }
}
