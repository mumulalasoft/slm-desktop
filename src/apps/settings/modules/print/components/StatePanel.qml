import QtQuick 2.15

Item {
    id: root
    property bool active: false
    property int enterDuration: 190
    property int exitDuration: 120
    property real restY: 0

    default property alias contentData: content.data

    anchors.fill: parent
    visible: active || opacity > 0
    enabled: active
    opacity: active ? 1 : 0
    y: active ? restY : restY + 8

    Behavior on opacity {
        NumberAnimation {
            duration: root.active ? root.enterDuration : root.exitDuration
            easing.type: Easing.OutCubic
        }
    }

    Behavior on y {
        NumberAnimation {
            duration: root.active ? root.enterDuration : root.exitDuration
            easing.type: Easing.OutQuad
        }
    }

    Item {
        id: content
        anchors.fill: parent
    }
}
