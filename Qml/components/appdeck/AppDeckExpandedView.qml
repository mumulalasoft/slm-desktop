import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "../apphub" as AppHubComp
import "zones" as Zones

Item {
    id: root

    required property var appsModel
    required property var desktopScene

    property int panelHeight: 0
    property int bottomSafeInset: 160
    property string apphubSearchSeed: ""

    // AppHub needs DesktopAppModel (full catalog), not AppDeckModel (dock-only).
    readonly property var allAppsModel: (typeof AppModel !== "undefined") ? AppModel : null

    property real revealProgress: 1.0

    signal dismissRequested()
    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    anchors.fill: parent

    onVisibleChanged: {
        if (visible) {
            revealProgress = 0.0
            revealAnim.restart()
        } else {
            revealAnim.stop()
            revealProgress = 0.0
        }
    }

    NumberAnimation {
        id: revealAnim
        target: root
        property: "revealProgress"
        from: 0.0
        to: 1.0
        duration: Theme.durationNormal
        easing.type: Theme.easingDecelerate
    }

    Zones.SuggestedZone {
        id: suggestedZone
        appsModel: root.allAppsModel
        revealProgress: root.revealProgress
    }

    // Suggested apps strip — visible above the main grid when there are frequent apps.
    Item {
        id: suggestedStrip
        visible: suggestedZone.hasApps
        opacity: Math.max(0, (root.revealProgress - 0.3) / 0.7)
        transform: Translate { y: (1.0 - Math.min(1.0, root.revealProgress * 1.5)) * 14 }

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: root.panelHeight + 12
        anchors.leftMargin: Math.max(56, parent.width * 0.07)
        anchors.rightMargin: Math.max(56, parent.width * 0.07)
        height: visible ? (stripIconSize + stripLabelHeight + 32) : 0

        readonly property real stripIconSize: 52
        readonly property real stripLabelHeight: Math.ceil(Theme.fontSize("small") * 1.6)
        readonly property real stripCellWidth: stripIconSize + 16

        Label {
            id: stripHeader
            text: "Sering Dipakai"
            font.pixelSize: Theme.fontSize("small")
            font.weight: Theme.fontWeight("semibold")
            color: Theme.color("textOnGlass")
            opacity: Theme.opacitySeparator
            anchors.top: parent.top
            anchors.left: parent.left
        }

        Row {
            id: stripRow
            anchors.top: stripHeader.bottom
            anchors.topMargin: 6
            anchors.left: parent.left
            spacing: 4

            Repeater {
                model: suggestedZone.apps

                delegate: Item {
                    width: suggestedStrip.stripCellWidth
                    height: suggestedStrip.stripIconSize + 6 + suggestedStrip.stripLabelHeight

                    readonly property var appEntry: modelData
                    readonly property real itemProgress: {
                        var delay = index * 0.06
                        if (delay >= 1.0) return 0.0
                        return Math.max(0.0, Math.min(1.0,
                            (root.revealProgress - delay) / (1.0 - Math.max(0.001, delay))))
                    }

                    opacity: itemProgress
                    transform: Translate { y: (1.0 - itemProgress) * 10 }

                    Rectangle {
                        id: iconPlate
                        width: suggestedStrip.stripIconSize
                        height: width
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: Theme.radiusMax
                        color: itemMouse.containsMouse
                               ? Theme.color("apphubSegmentActive") : "transparent"
                        border.width: itemMouse.containsMouse
                                      ? Theme.borderWidthThin : Theme.borderWidthNone
                        border.color: Theme.color("apphubIconRing")
                        scale: itemMouse.containsMouse ? 1.08 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                        }

                        Image {
                            anchors.centerIn: parent
                            width: Math.round(parent.width * 0.78)
                            height: width
                            source: (appEntry.iconName && appEntry.iconName.length > 0)
                                    ? ("image://themeicon/" + appEntry.iconName + "?v="
                                       + ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                          ? ThemeIconController.revision : 0))
                                    : (appEntry.iconSource || "")
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                    Label {
                        anchors.top: iconPlate.bottom
                        anchors.topMargin: 4
                        anchors.horizontalCenter: iconPlate.horizontalCenter
                        width: parent.width
                        height: suggestedStrip.stripLabelHeight
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: appEntry.name
                        font.pixelSize: Theme.fontSize("small")
                        color: Theme.color("textOnGlass")
                        opacity: Theme.opacityMuted
                    }

                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.appChosen(appEntry)
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.color("panelBorder")
            opacity: Theme.opacitySubtle
        }
    }

    readonly property int suggestedStripExtraInset: suggestedZone.hasApps
        ? Math.round(suggestedStrip.height + 16)
        : 0

    AppHubComp.AppHub {
        id: apphubContent
        anchors.fill: parent
        desktopScene: root.desktopScene
        appsModel: root.allAppsModel
        externalSearchSeed: root.apphubSearchSeed
        topSafeInset: root.panelHeight + root.suggestedStripExtraInset
        bottomSafeInset: root.bottomSafeInset
        revealProgress: root.revealProgress
        onDismissRequested: root.dismissRequested()
        onAppChosen: function(appData) { root.appChosen(appData) }
        onAddToDockRequested: function(appData) { root.addToDockRequested(appData) }
        onAddToDesktopRequested: function(appData) { root.addToDesktopRequested(appData) }
    }
}
