import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property bool showEmptyState: true
    property string errorText: ""

    anchors.fill: parent
    visible: showEmptyState
    z: 3

    Label {
        anchors.centerIn: parent
        text: root.errorText.length > 0 ? root.errorText : "This folder is empty"
        color: Theme.color("textSecondary")
        font.pixelSize: Theme.fontSize("body")
    }
}
