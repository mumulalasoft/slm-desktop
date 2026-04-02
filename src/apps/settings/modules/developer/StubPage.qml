import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    anchors.fill: parent

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Coming Soon")
            font.pixelSize: Theme.fontSize("h2")
            font.weight: Font.DemiBold
            color: Theme.color("textPrimary")
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("This page is not yet implemented.")
            font.pixelSize: Theme.fontSize("body")
            color: Theme.color("textSecondary")
        }
    }
}
