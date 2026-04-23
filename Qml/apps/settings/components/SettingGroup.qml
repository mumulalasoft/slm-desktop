import QtQuick 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

ColumnLayout {
    id: root
    spacing: Theme.metric("spacingSm")

    property alias title: titleText.text
    default property alias content: cardsLayout.data

    Text {
        id: titleText
        Layout.fillWidth: true
        Layout.leftMargin: Theme.metric("spacingXs")
        font.pixelSize: Theme.fontSize("small")
        font.weight: Theme.fontWeight("semibold")
        color: Theme.color("textSecondary")
        visible: text !== ""
    }

    // Single outer card wrapping all child SettingCards
    DSStyle.Card {
        Layout.fillWidth: true
        implicitHeight: cardsLayout.implicitHeight
        padding: 0
        cardColor: Theme.color("surface")
        cardBorderColor: Theme.color("panelBorder")
        cardRadius: Theme.radiusCard
        elevation: "low"
        clip: true

        ColumnLayout {
            id: cardsLayout
            anchors.fill: parent
            spacing: 0
        }
    }
}
