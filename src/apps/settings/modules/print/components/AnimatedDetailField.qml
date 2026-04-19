import QtQuick 2.15
import QtQuick.Layouts 1.15
import SlmStyle

Item {
    id: root

    property string label: ""
    property bool active: false

    default property alias fieldContent: fieldHost.data

    implicitHeight: row.implicitHeight
    opacity: active ? 1.0 : 0.9
    y: active ? 0 : 4

    Behavior on opacity {
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }
    Behavior on y {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: 10

        Text {
            text: root.label
            font.pixelSize: Theme.fontSize("small")
            color: Theme.color("textSecondary")
            Layout.preferredWidth: 70
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignVCenter
        }

        Item {
            id: fieldHost
            Layout.fillWidth: true
            implicitHeight: childrenRect.height
        }
    }
}
