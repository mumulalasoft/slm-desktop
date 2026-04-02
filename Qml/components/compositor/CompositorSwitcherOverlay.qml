import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root
    anchors.fill: parent
    visible: opacity > 0.01
    opacity: (typeof CompositorStateModel !== "undefined" &&
              CompositorStateModel &&
              CompositorStateModel.switcherActive) ? 1.0 : 0.0
    z: 450

    readonly property int switchCount: (typeof CompositorStateModel !== "undefined" &&
                                        CompositorStateModel)
                                       ? Math.max(0, CompositorStateModel.switcherCount) : 0
    readonly property int shownCount: Math.min(9, switchCount)
    readonly property var entries: (typeof CompositorStateModel !== "undefined" && CompositorStateModel)
                                   ? CompositorStateModel.switcherEntries : []
    readonly property int selectedIndex: (shownCount > 0 && typeof CompositorStateModel !== "undefined" &&
                                          CompositorStateModel)
                                         ? Math.max(0, Math.min(shownCount - 1,
                                                                CompositorStateModel.switcherSelectedSlot))
                                         : 0
    readonly property int absoluteIndex: (typeof CompositorStateModel !== "undefined" && CompositorStateModel)
                                         ? Math.max(0, CompositorStateModel.switcherIndex) : 0
    readonly property string selectedTitle: {
        if (typeof CompositorStateModel === "undefined" || !CompositorStateModel) {
            return ""
        }
        if (entries && selectedIndex >= 0 && selectedIndex < entries.length) {
            var e = entries[selectedIndex]
            if (e && e.title && e.title.length > 0) {
                return e.title
            }
            if (e && e.appId && e.appId.length > 0) {
                return e.appId
            }
        }
        if (CompositorStateModel.switcherAppId && CompositorStateModel.switcherAppId.length > 0) {
            return CompositorStateModel.switcherAppId
        }
        return ""
    }
    readonly property real cardWidth: 108
    readonly property real cardHeight: 120
    readonly property real rowSpacing: 14
    readonly property real cardPitch: cardWidth + rowSpacing
    readonly property int visibleCards: Math.max(1, Math.min(5, shownCount))

    function slotLabel(idx) {
        if (typeof CompositorStateModel === "undefined" || !CompositorStateModel) {
            return String(idx + 1)
        }
        if (!entries || idx < 0 || idx >= entries.length) {
            return String(idx + 1)
        }
        var w = entries[idx]
        var s = ""
        if (w.appId && w.appId.length > 0) {
            s = w.appId
        } else if (w.title && w.title.length > 0) {
            s = w.title
        } else {
            return String(idx + 1)
        }
        for (var i = 0; i < s.length; ++i) {
            var ch = s.charAt(i)
            if (/[A-Za-z0-9]/.test(ch)) {
                return ch.toUpperCase()
            }
        }
        return String(idx + 1)
    }

    function slotIconSource(idx) {
        if (typeof CompositorStateModel === "undefined" || !CompositorStateModel) {
            return ""
        }
        if (!entries || idx < 0 || idx >= entries.length) {
            return ""
        }
        var w = entries[idx]
        if (w && w.iconName && w.iconName.length > 0) {
            return "image://themeicon/" + w.iconName + "?v=" +
                    ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                     ? ThemeIconController.revision : 0)
        }
        if (w && w.iconSource && w.iconSource.length > 0) {
            return w.iconSource
        }
        return ""
    }

    function slotAppName(idx) {
        if (!entries || idx < 0 || idx >= entries.length) {
            return "App"
        }
        var w = entries[idx]
        if (w.title && w.title.length > 0) {
            return w.title
        }
        if (w.appId && w.appId.length > 0) {
            return w.appId
        }
        return "App"
    }

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.durationFast
            easing.type: Theme.easingDefault
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color("compositorOverlayScrim")
        visible: root.opacity > 0.01
    }

    Rectangle {
        id: panel
        visible: root.shownCount > 0
        width: Math.min(parent.width * 0.92, Math.max(360, 52 + root.visibleCards * root.cardPitch))
        height: 214
        radius: Theme.radiusMax
        color: Theme.color("compositorPanelBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("compositorPanelBorder")
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        layer.enabled: true
        layer.smooth: true

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            hoverEnabled: true
            onWheel: function(wheel) {
                if (!root.visible || root.shownCount <= 1) {
                    return
                }
                if (typeof WindowingBackend === "undefined" || !WindowingBackend || !WindowingBackend.sendCommand) {
                    return
                }
                if (wheel.angleDelta.y < 0 || wheel.angleDelta.x < 0) {
                    WindowingBackend.sendCommand("switcher-next")
                    wheel.accepted = true
                    return
                }
                if (wheel.angleDelta.y > 0 || wheel.angleDelta.x > 0) {
                    WindowingBackend.sendCommand("switcher-prev")
                    wheel.accepted = true
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.color("compositorPanelSheen") }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        Item {
            id: slotsViewport
            anchors.top: parent.top
            anchors.topMargin: 22
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 52
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.right: parent.right
            anchors.rightMargin: 18
            clip: true

            Row {
                id: slots
                y: 0
                spacing: root.rowSpacing
                x: (slotsViewport.width - root.cardWidth) / 2 - (root.selectedIndex * root.cardPitch)

                Behavior on x {
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                }

                Repeater {
                    model: root.shownCount
                    delegate: Item {
                        required property int index
                        readonly property int distance: Math.abs(index - root.selectedIndex)
                        width: root.cardWidth
                        height: root.cardHeight
                        scale: distance === 0 ? 1.0
                                               : (distance === 1 ? 0.9
                                                                 : (distance === 2 ? 0.82 : 0.74))
                        y: distance === 0 ? 0
                                          : (distance === 1 ? 6
                                                            : (distance === 2 ? 11 : 15))
                        opacity: distance === 0 ? 1.0
                                                : (distance === 1 ? 0.86
                                                                  : (distance === 2 ? 0.68 : 0.5))

                        Behavior on scale {
                            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
                        }
                        Behavior on y {
                            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
                        }
                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radiusXxl
                            color: distance === 0
                                   ? Theme.color("compositorCardBgPrimary")
                                   : (distance === 1
                                      ? Theme.color("compositorCardBgSecondary")
                                      : Theme.color("compositorCardBgTertiary"))
                            border.width: distance === 0 ? 2 : 1
                            border.color: distance === 0
                                          ? Theme.color("compositorCardBorderActive")
                                          : Theme.color("compositorCardBorderInactive")
                        }

                        Rectangle {
                            visible: distance === 0
                            width: parent.width - 24
                            height: 4
                            radius: Theme.radiusTiny
                            color: Theme.color("accent")
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 10
                        }

                        Image {
                            id: slotIcon
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 18
                            width: 42
                            height: 42
                            sourceSize.width: 42
                            sourceSize.height: 42
                            source: root.slotIconSource(index)
                            visible: status === Image.Ready && source.toString().length > 0
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 28
                            text: root.slotLabel(index)
                            font.pixelSize: Theme.fontSize("titleLarge")
                            font.weight: Theme.fontWeight("bold")
                            color: Theme.color("compositorLabelPrimary")
                            visible: !slotIcon.visible
                        }

                        Label {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 16
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: root.slotAppName(index)
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: (distance === 0) ? Theme.fontWeight("bold") : Theme.fontWeight("normal")
                            color: distance === 0
                                   ? Theme.color("compositorLabelSecondaryActive")
                                   : Theme.color("compositorLabelSecondaryInactive")
                        }
                    }
                }
            }
        }

        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: {
                var title = root.selectedTitle
                var pos = ""
                if (root.switchCount > 0) {
                    pos = String(root.absoluteIndex + 1) + " / " + String(root.switchCount)
                }
                if (title.length > 0 && pos.length > 0) {
                    return title + "  (" + pos + ")"
                }
                if (title.length > 0) {
                    return title
                }
                return pos
            }
            color: Theme.color("compositorFooterLabel")
            font.pixelSize: Theme.fontSize("smallPlus")
            font.weight: Theme.fontWeight("bold")
            visible: text.length > 0
        }
    }
}
