import QtQuick 2.15

Item {
    id: root
    default property alias content: slot.data

    Item {
        id: slot
        anchors.fill: parent
    }
}
