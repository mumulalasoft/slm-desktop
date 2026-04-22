import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var resultData: ({})
    property bool selected: false
    property real pressScale: 1.0
    readonly property real cardRadius: (typeof Theme !== "undefined"
                                        && Theme
                                        && Theme.radiusCard !== undefined)
                                       ? Number(Theme.radiusCard) : 10
    readonly property string resultId: String(resultData && resultData.resultId ? resultData.resultId : "")
    readonly property string titleText: String(resultData && resultData.title ? resultData.title : "")
    readonly property string subtitleText: String(resultData && resultData.subtitle ? resultData.subtitle : "")
    readonly property string typeText: String(resultData && resultData.type ? resultData.type : "action")
    readonly property string iconName: String(resultData && resultData.icon ? resultData.icon : "")
    readonly property string iconPath: String(resultData && resultData.iconSource ? resultData.iconSource : "")
    readonly property bool iconPathIsUrl: iconPath.indexOf("/") === 0
                                          || iconPath.indexOf("file:/") === 0
                                          || iconPath.indexOf("qrc:/") === 0
                                          || iconPath.indexOf("http://") === 0
                                          || iconPath.indexOf("https://") === 0
                                          || iconPath.indexOf("image://") === 0
    readonly property string resolvedIconName: iconName.length > 0
                                               ? iconName
                                               : (iconPathIsUrl ? "" : iconPath)
    readonly property string cleanedIconName: {
        var v = String(resolvedIconName || "").trim()
        if (v.length <= 0) {
            return ""
        }
        var comma = v.indexOf(",")
        if (comma > 0) {
            v = v.slice(0, comma).trim()
        }
        var parts = v.split(/\s+/)
        for (var i = 0; i < parts.length; ++i) {
            var p = String(parts[i] || "").trim()
            if (p.length <= 0) continue
            if (p === "GThemedIcon") continue
            if (p === "ThemedIcon") continue
            if (p === ".") continue
            return p
        }
        return v
    }
    readonly property string defaultIconName: "system-run-symbolic"
    readonly property string effectiveIconName: cleanedIconName.length > 0 ? cleanedIconName : defaultIconName
    readonly property string themeIconSource: {
        if (effectiveIconName.indexOf("/") === 0
                || effectiveIconName.indexOf("file:/") === 0
                || effectiveIconName.indexOf("qrc:/") === 0
                || effectiveIconName.indexOf("http://") === 0
                || effectiveIconName.indexOf("https://") === 0) {
            return effectiveIconName
        }
        return effectiveIconName.length > 0
               ? ("image://themeicon/" + effectiveIconName + "?v="
                  + ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                     ? ThemeIconController.revision : 0))
               : ""
    }
    readonly property string primaryIconSource: iconPathIsUrl ? iconPath : themeIconSource
    readonly property string effectiveIconSource: primaryIconSource.length > 0 ? primaryIconSource : themeIconSource
    readonly property string qrcFallbackSource: "qrc:/icons/dark/pulse.svg"

    signal hovered(string resultId)
    signal activated(string resultId)
    signal contextActionRequested(string resultId, string action)

    implicitHeight: 44
    property real liftOffset: root.selected ? -1 : (hover.containsMouse ? -1 : 0)
    transform: Translate { y: root.liftOffset }
    scale: root.pressScale

    Behavior on liftOffset {
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cardRadius
        color: root.selected
               ? Theme.color("pulseRowActive")
               : (hover.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06))
        border.width: root.selected ? Theme.borderWidthThin : 0
        border.color: Theme.color("panelBorderStrong")
        opacity: root.selected ? 0.95 : 1.0

        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: iconPlate
        width: 24
        height: 24
        radius: 12
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.color("shellIconPlateBg")
        border.width: 0

        Image {
            id: iconImage
            anchors.fill: parent
            source: root.effectiveIconSource
            fillMode: Image.PreserveAspectFit
            visible: String(source).length > 0 && status !== Image.Error
        }

        Image {
            id: iconQrcFallback
            anchors.fill: parent
            source: root.qrcFallbackSource
            asynchronous: false
            fillMode: Image.PreserveAspectFit
            visible: !iconImage.visible && !iconQrcFallback.visible && String(source).length > 0 && status !== Image.Error
        }

        Label {
            anchors.centerIn: parent
            visible: !iconImage.visible && !iconQrcFallback.visible
            text: ">"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            font.weight: Theme.fontWeight("semibold")
        }
    }

    Label {
        anchors.left: parent.left
        anchors.leftMargin: 38
        anchors.right: badge.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.titleText.length > 0 ? root.titleText : root.subtitleText
        color: Theme.color("textPrimary")
        font.pixelSize: Theme.fontSize("small")
        font.weight: Theme.fontWeight("medium")
        elide: Text.ElideRight
    }

    Rectangle {
        id: badge
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        height: 18
        width: Math.max(46, badgeLabel.implicitWidth + 12)
        radius: height / 2
        color: Theme.color("pulseBadgeBg")
        border.width: 0

        Label {
            id: badgeLabel
            anchors.centerIn: parent
            text: root.typeText
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("xs")
        }
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        onPressed: root.pressScale = 0.99
        onReleased: root.pressScale = 1.0
        onCanceled: root.pressScale = 1.0
        onEntered: root.hovered(root.resultId)
        onClicked: root.activated(root.resultId)
    }
}
