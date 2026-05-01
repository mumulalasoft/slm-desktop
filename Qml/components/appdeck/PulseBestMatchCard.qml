import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var resultData: null
    property bool active: false
    property real revealAmount: active ? 1.0 : 0.0
    property bool selected: false
    property real pressScale: 1.0
    readonly property int motionFastDuration: Theme.durationFast
    readonly property int motionNormalDuration: Theme.durationNormal
    readonly property real cardRadius: (typeof Theme !== "undefined"
                                        && Theme
                                        && Theme.radiusWindow !== undefined)
                                       ? Number(Theme.radiusWindow) : 14
    readonly property real pillRadius: (typeof Theme !== "undefined"
                                        && Theme
                                        && Theme.radiusPill !== undefined)
                                       ? Number(Theme.radiusPill) : 12

    readonly property bool available: !!resultData
    readonly property string resultId: available ? String(resultData.resultId || "") : ""
    readonly property string titleText: available ? String(resultData.title || "") : ""
    readonly property string subtitleText: available ? String(resultData.subtitle || "") : ""
    readonly property string typeText: available ? String(resultData.type || "best") : "best"
    readonly property string iconName: available ? String(resultData.icon || "") : ""
    readonly property string iconPath: available ? String(resultData.iconSource || "") : ""
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
        if (t === "app") return "qrc:/icons/appdeck.svg"
        if (t === "file" || t === "path" || t === "folder" || t === "recent") return "qrc:/icons/logo.svg"
        if (t === "action" || t === "command") return "qrc:/icons/dark/pulse.svg"
        if (t === "calculator") return "qrc:/icons/dark/pulse.svg"
        return "qrc:/icons/logo.svg"
    }

    signal hovered(string resultId)
    signal activated(string resultId)

    implicitHeight: 112
    visible: available
    opacity: revealAmount
    y: (1.0 - revealAmount) * 7
    scale: (0.985 + (0.015 * revealAmount)) * pressScale

    Behavior on opacity {
        NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
    }
    Behavior on y {
        NumberAnimation { duration: root.motionNormalDuration; easing.type: Theme.easingDecelerate }
    }
    Behavior on scale {
        NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingDecelerate }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cardRadius
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.selected ? Qt.rgba(1.0, 0.82, 0.46, 0.92) : Qt.rgba(0.90, 0.94, 0.99, 0.94) }
            GradientStop { position: 1.0; color: root.selected ? Qt.rgba(1.0, 0.76, 0.36, 0.92) : Qt.rgba(0.84, 0.90, 0.98, 0.92) }
        }
        border.width: root.selected ? Theme.borderWidthThin : 0
        border.color: Theme.color("panelBorderStrong")
        opacity: hover.containsMouse ? 1.0 : 0.98

        Behavior on opacity {
            NumberAnimation { duration: root.motionFastDuration; easing.type: Theme.easingLight }
        }
    }

    Rectangle {
        width: 58
        height: 58
        radius: width * 0.5
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.color("shellIconPlateBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")

        Image {
            id: iconImage
            anchors.centerIn: parent
            width: 38
            height: 38
            source: root.effectiveIconSource
            fillMode: Image.PreserveAspectFit
            visible: String(source).length > 0 && status !== Image.Error
        }

        Image {
            id: iconQrcFallback
            anchors.centerIn: parent
            width: iconImage.width
            height: iconImage.height
            source: root.qrcFallbackSource
            asynchronous: false
            fillMode: Image.PreserveAspectFit
            visible: !iconImage.visible && !iconQrcFallback.visible && String(source).length > 0 && status !== Image.Error
        }

        Label {
            anchors.centerIn: parent
            visible: !iconImage.visible && !iconQrcFallback.visible
            text: root.titleText.length > 0 ? root.titleText.charAt(0).toUpperCase() : "?"
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("semibold")
        }
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 96
        anchors.right: parent.right
        anchors.rightMargin: 104
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Label {
            text: root.titleText
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("title")
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
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        height: 26
        width: Math.max(60, typeLabel.implicitWidth + 16)
        radius: root.pillRadius
        color: Theme.color("pulseBadgeBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("pulseBadgeBorder")

        Label {
            id: typeLabel
            anchors.centerIn: parent
            text: root.typeText === "calculator" ? "CALC" : root.typeText
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("xs")
            font.weight: Theme.fontWeight("semibold")
        }
    }

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
