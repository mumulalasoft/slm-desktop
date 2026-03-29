import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    property var notificationManager: null
    property bool open: false

    signal dismissRequested(int notificationId)
    signal notificationClicked(int notificationId)
    signal closeRequested()

    readonly property int panelWidth: 420

    anchors.fill: parent
    visible: dimmer.opacity > 0.01 || panel.x < width

    Rectangle {
        id: dimmer
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.32)
        opacity: root.open ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.open
            onClicked: root.closeRequested()
        }
    }

    Rectangle {
        id: panel
        width: root.panelWidth
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: root.open ? (root.width - width) : root.width
        color: Theme.color("menuBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")

        Behavior on x {
            NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Notification Center")
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("title")
                    font.weight: Theme.fontWeight("semiBold")
                    elide: Text.ElideRight
                }

                Button {
                    text: qsTr("Clear All")
                    enabled: root.notificationManager && root.notificationManager.count > 0
                    onClicked: {
                        if (root.notificationManager && root.notificationManager.clearAll) {
                            root.notificationManager.clearAll()
                        }
                    }
                }
            }

            ListView {
                id: centerList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 10
                model: root.notificationManager ? root.notificationManager.notifications : null
                focus: root.open
                currentIndex: count > 0 ? 0 : -1
                section.property: "appName"
                section.criteria: ViewSection.FullString
                section.delegate: Rectangle {
                    width: centerList.width
                    height: 28
                    radius: 8
                    color: Theme.color("controlBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: section
                        color: Theme.color("textSecondary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: Theme.fontWeight("semiBold")
                        elide: Text.ElideRight
                    }
                }
                delegate: NotificationCard {
                    width: ListView.view ? ListView.view.width : 380
                    title: String(summary || appName || "")
                    opacity: ListView.isCurrentItem ? 1 : 0.96
                    border.color: ListView.isCurrentItem ? Theme.color("accent") : Theme.color("panelBorder")

                    onClicked: {
                        var id = Number(notificationId || -1)
                        if (id > 0) root.notificationClicked(id)
                    }
                    onDismissRequested: {
                        var id = Number(notificationId || -1)
                        if (id > 0) root.dismissRequested(id)
                    }
                }

                Keys.onPressed: function(event) {
                    if (!root.open) {
                        return
                    }
                    if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                        var row = centerList.currentItem
                        if (row && row.notificationId !== undefined) {
                            root.dismissRequested(Number(row.notificationId))
                            event.accepted = true
                        }
                        return
                    }
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        var rowOpen = centerList.currentItem
                        if (rowOpen && rowOpen.notificationId !== undefined) {
                            root.notificationClicked(Number(rowOpen.notificationId))
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
