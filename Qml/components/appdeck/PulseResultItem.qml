import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var resultData: ({})
    property bool selected: false
    property bool prominent: false
    property real pressScale: 1.0
    readonly property int motionFastDuration: Theme.durationFast
    readonly property int motionSnapDuration: Theme.durationSm
    readonly property real cardRadius: (typeof Theme !== "undefined"
                                        && Theme
                                        && Theme.radiusCard !== undefined)
                                       ? Number(Theme.radiusCard) : 10
    readonly property real pillRadius: (typeof Theme !== "undefined"
                                        && Theme
                                        && Theme.radiusPill !== undefined)
                                       ? Number(Theme.radiusPill) : 12

    readonly property string resultId: String(resultData && resultData.resultId ? resultData.resultId : "")
    readonly property string titleText: String(resultData && resultData.title ? resultData.title : "")
    readonly property string subtitleText: String(resultData && resultData.subtitle ? resultData.subtitle : "")
    readonly property string typeText: String(resultData && resultData.type ? resultData.type : "item")
    readonly property string clipboardType: String(resultData && resultData.clipboardType ? resultData.clipboardType : "")
    readonly property string badgeText: {
        var t = String(typeText || "").toLowerCase()
        if (t === "clipboard") {
            return clipboardType.length > 0 ? clipboardType.toUpperCase() : "CLIPBOARD"
        }
        if (t === "calculator") {
            return "CALC"
        }
        return String(typeText || "item")
    }
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
    readonly property string defaultIconName: {
        var t = String(typeText || "").toLowerCase()
        if (t === "app") return "application-x-executable-symbolic"
        if (t === "file" || t === "path" || t === "folder" || t === "recent") return "text-x-generic-symbolic"
        if (t === "action" || t === "command") return "system-run-symbolic"
        if (t === "clipboard") return "edit-paste-symbolic"
        if (t === "calculator") return "accessories-calculator-symbolic"
        return "application-x-executable-symbolic"
    }
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
    readonly property string qrcFallbackSource: {
        var t = String(typeText || "").toLowerCase()
        if (t === "app") return "qrc:/icons/apphub.svg"
        if (t === "file" || t === "path" || t === "folder" || t === "recent") return "qrc:/icons/logo.svg"
        if (t === "action" || t === "command") return "qrc:/icons/dark/pulse.svg"
        if (t === "calculator") return "qrc:/icons/dark/pulse.svg"
        return "qrc:/icons/logo.svg"
    }
    readonly property string fallbackGlyph: {
        var t = String(typeText || "").toLowerCase()
        if (t === "app") return "A"
        if (t === "file" || t === "path" || t === "folder") return "F"
        if (t === "action" || t === "command") return ">"
        if (t === "clipboard") return "C"
        if (t === "calculator") return "="
        return "•"
    }

    signal hovered(string resultId)
    signal activated(string resultId)
    signal contextActionRequested(string resultId, string action)

    implicitHeight: root.prominent ? 68 : 54
    property real liftOffset: root.selected ? -1 : (hover.containsMouse ? -1 : 0)
    transform: Translate { y: root.liftOffset }
    scale: root.pressScale

    Behavior on liftOffset {
        NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
    }
    Behavior on scale {
        NumberAnimation { duration: root.motionSnapDuration; easing.type: Theme.easingLight }
    }

    Rectangle {
        id: plate
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
        Behavior on opacity {
            NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingLight }
        }
    }

    Rectangle {
        id: iconPlate
        width: root.prominent ? 38 : 32
        height: width
        radius: width / 2
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        color: root.selected ? Theme.color("accentSoft") : Theme.color("shellIconPlateBg")
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
            text: root.fallbackGlyph
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("small")
            font.weight: Theme.fontWeight("semibold")
        }
    }

    Column {
        anchors.left: iconPlate.right
        anchors.leftMargin: 10
        anchors.right: typeBadge.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        Label {
            text: root.titleText
            color: Theme.color("textPrimary")
            font.pixelSize: root.prominent ? Theme.fontSize("bodyLarge") : Theme.fontSize("body")
            font.weight: Theme.fontWeight("semibold")
            elide: Text.ElideRight
        }

        Label {
            text: root.subtitleText
            visible: text.length > 0
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideRight
        }
    }

    Rectangle {
        id: typeBadge
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        height: 20
        radius: root.pillRadius
        width: Math.max(48, badgeLabel.implicitWidth + 14)
        color: root.selected ? Theme.color("accentSoft") : Theme.color("pulseBadgeBg")
        border.width: Theme.borderWidthNone

        Label {
            id: badgeLabel
            anchors.centerIn: parent
            text: root.badgeText
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("xs")
            font.weight: Theme.fontWeight("medium")
        }
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: root.pressScale = 0.99
        onReleased: root.pressScale = 1.0
        onCanceled: root.pressScale = 1.0
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                if (String(root.typeText || "").toLowerCase() === "clipboard") {
                    root.contextActionRequested(root.resultId, "paste")
                } else {
                    root.contextActionRequested(root.resultId, "openContainingFolder")
                }
                return
            }
            root.hovered(root.resultId)
        }
        onDoubleClicked: function(mouse) {
            if (mouse.button !== Qt.LeftButton) {
                return
            }
            root.activated(root.resultId)
        }
    }
}
