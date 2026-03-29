import QtQuick 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

ColumnLayout {
    id: root

    property string groupTitle: ""
    property var model: null
    property bool expanded: true

    signal toggleRequested()
    signal dismissRequested(int notificationId)
    signal notificationClicked(int notificationId)

    spacing: 8

    Rectangle {
        Layout.fillWidth: true
        height: 34
        radius: 10
        color: Theme.color("controlBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10

            Text {
                Layout.fillWidth: true
                text: root.groupTitle
                color: Theme.color("textPrimary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("semiBold")
                elide: Text.ElideRight
            }

            Text {
                text: root.expanded ? "\u25be" : "\u25b8"
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.toggleRequested()
        }
    }

    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: root.expanded ? contentHeight : 0
        visible: root.expanded
        interactive: false
        spacing: 8
        model: root.model

        delegate: NotificationCard {
            width: ListView.view ? ListView.view.width : 360
            title: String(summary || appName || "")

            onClicked: {
                var id = Number(notificationId || -1)
                if (id > 0) root.notificationClicked(id)
            }
            onDismissRequested: {
                var id = Number(notificationId || -1)
                if (id > 0) root.dismissRequested(id)
            }
        }
    }
}
