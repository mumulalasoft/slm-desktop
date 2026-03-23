import QtQuick 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: root
    spacing: 8
    
    property alias title: titleText.text
    default property alias content: cardsLayout.data

    Text {
        id: titleText
        Layout.fillWidth: true
        Layout.leftMargin: 4
        font.pixelSize: 13
        font.weight: Font.DemiBold
        color: "#8E8E93"
        visible: text !== ""
    }

    ColumnLayout {
        id: cardsLayout
        Layout.fillWidth: true
        spacing: 0
    }
}
