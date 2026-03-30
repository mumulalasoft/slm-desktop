import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import Slm_Desktop

Rectangle {
    id: root

    property int iconButtonW: Theme.metric("controlHeightRegular")
    property int iconButtonH: Theme.metric("controlHeightCompact")
    property int iconGlyph: 18

    signal clicked()

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    width: iconButtonW
    height: iconButtonH
    radius: Theme.radiusControl
    color: searchMouse.containsMouse ? Theme.color("accentSoft") : "transparent"
    Behavior on color {
        enabled: root.microAnimationAllowed()
        ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
    }

    IconImage {
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
        color: Theme.color("textOnGlass")
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
