import QtQuick 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

ColumnLayout {
    id: root
    spacing: 8

    property alias title: titleText.text
    default property alias content: cardsLayout.data

    Text {
        id: titleText
        Layout.fillWidth: true
        Layout.leftMargin: 4
        font.pixelSize: Theme.fontSize("small")
        font.weight: Font.DemiBold
        color: Theme.color("textSecondary")
        visible: text !== ""
    }

    // Single outer card wrapping all child SettingCards
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: cardsLayout.implicitHeight
        color: Theme.color("surface")
        border.color: Theme.color("panelBorder")
        border.width: 1
        radius: Theme.radiusCard
        clip: true

        ColumnLayout {
            id: cardsLayout
            anchors.fill: parent
            spacing: 0
        }
    }
}
