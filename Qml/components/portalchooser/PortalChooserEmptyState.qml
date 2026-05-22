import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root

    property bool showEmptyState: true
    property string errorText: ""

    anchors.fill: parent
    visible: showEmptyState
    z: 3

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 360)
        spacing: Theme.metric("spacingSm")

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 32
            height: 32
            fillMode: Image.PreserveAspectFit
            source: "image://themeicon/" + (root.errorText.length > 0 ? "dialog-warning-symbolic" : "folder-open-symbolic")
            opacity: Theme.opacityMuted
        }

        DSStyle.Label {
            width: parent.width
            text: root.errorText.length > 0 ? root.errorText : "This folder is empty"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("body")
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
