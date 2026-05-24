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
    readonly property real hostWindowWidth: (hostRoot && hostRoot.hostWindow)
                                           ? Number(hostRoot.hostWindow.width || 0)
                                           : Number(width || 0)
    readonly property real adaptiveSidebarWidth: Math.max(
                                                     188,
                                                     Math.min(
                                                         248,
                                                         Math.round(
                                                             hostWindowWidth * 0.21)))

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
                Layout.preferredWidth: root.adaptiveSidebarWidth
                Layout.fillHeight: true
                Layout.leftMargin: Theme.metric("spacingLg")
                Layout.topMargin: Theme.metric("spacingLg")
                Layout.bottomMargin: Theme.metric("spacingLg")
                Layout.rightMargin: Theme.metric("spacingLg")
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
                    Layout.preferredHeight: Theme.fileManagerToolbarHeight + Theme.metric("spacingXs")
                    Layout.leftMargin: Theme.metric("spacingMd")
                    Layout.rightMargin: Theme.metric("spacingLg")
                    Layout.topMargin: Theme.metric("spacingXs")
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
