import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var resultData: ({})
    property bool selected: false
    property real pressScale: 1.0
    readonly property int motionFastDuration: Theme.durationFast
    readonly property int motionSnapDuration: Theme.durationSm
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
    // docs/APPDECK.md §20 — Result UX. Hover lifts -3 + hoverScale 1.04
    // (composed with pressScale so press-while-hovered still shrinks).
    property real liftOffset: root.selected ? -1 : (hover.containsMouse ? -3 : 0)
    property real hoverScale: hover.containsMouse ? 1.04 : 1.0
    transform: Translate { y: root.liftOffset }
    scale: root.pressScale * root.hoverScale

    Behavior on liftOffset {
        NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
    }
    // docs/APPDECK.md §19/§20 — Spring easing for the hover bloom pop.
    Behavior on scale {
        NumberAnimation {
            duration: root.motionSnapDuration
            easing.type: Theme.easingSpring
            easing.overshoot: 1.15
        }
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
            ColorAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
        }
        Behavior on border.color {
            ColorAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
        }
    }

    Rectangle {
        id: iconPlate
        width: 24
        height: 24
        radius: width * 0.5
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.color("shellIconPlateBg")
        border.width: Theme.borderWidthNone

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

    // docs/APPDECK_REDESIGN.md Phase 5 — type badge dropped; the icon and
    // section context already communicate "this is an action / command".

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onPressed: root.pressScale = 0.99
        onReleased: root.pressScale = 1.0
        onCanceled: root.pressScale = 1.0
        onClicked: root.hovered(root.resultId)
        onDoubleClicked: function(mouse) {
            if (mouse.button !== Qt.LeftButton) {
                return
            }
            root.activated(root.resultId)
        }
    }
}
