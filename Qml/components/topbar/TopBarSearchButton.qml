import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Rectangle {
    id: root

    property int iconButtonW: Theme.metric("controlHeightRegular")
    property int iconButtonH: Theme.metric("controlHeightCompact")
    property int iconGlyph: 18

    signal clicked()

    width: iconButtonW
    height: iconButtonH
    radius: Theme.radiusControl
    color: searchMouse.containsMouse ? Theme.color("accentSoft") : "transparent"

    Image {
        id: searchIcon
        anchors.centerIn: parent
        width: root.iconGlyph
        height: root.iconGlyph
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: true
        source: "image://themeicon/system-search-symbolic?v=" +
                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                 ? ThemeIconController.revision : 0)
    }

    Label {
        anchors.centerIn: parent
        visible: searchIcon.status !== Image.Ready
        text: "\u2315"
        color: Theme.color("textOnGlass")
        font.pixelSize: Theme.fontSize("bodyLarge")
    }

    MouseArea {
        id: searchMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
