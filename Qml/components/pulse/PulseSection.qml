import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: root
    property string titleText: ""
    property bool previewStyle: false
    property alias contentData: contentHost.data

    radius: Theme.radiusWindow
    color: previewStyle ? Theme.color("pulsePreviewBg") : Theme.color("pulseResultsBg")
    border.width: Theme.borderWidthThin
    border.color: previewStyle ? Theme.color("pulsePreviewBorder") : Theme.color("pulseResultsBorder")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Label {
            text: root.titleText
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            font.weight: Theme.fontWeight("bold")
        }

        Item {
            id: contentHost
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
