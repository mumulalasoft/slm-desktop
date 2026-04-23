import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: root
    property bool focused: false
    property string titleText: ""
    property string subtitleText: ""

    height: 78
    radius: Theme.radiusWindow
    color: focused ? Theme.color("pulseRowActive") : Theme.color("pulseResultsBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("pulseResultsBorder")

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Label {
            text: "Top Result"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("xs")
        }
        Label {
            Layout.fillWidth: true
            text: root.titleText
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("bodyLarge")
            font.weight: Theme.fontWeight("bold")
            elide: Text.ElideRight
        }
        Label {
            text: root.subtitleText
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideMiddle
        }
    }
}
