import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property bool active: false
    signal suggestionChosen(string text)

    radius: Theme.radiusWindow
    color: Theme.color("pulseSectionBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("pulseResultsBorder")
    opacity: active ? Theme.opacitySurfaceStrong : 0.0
    y: active ? 0 : 8

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
    }
    Behavior on y {
        NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDecelerate }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label {
            text: "Pulse Ready"
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("semibold")
        }

        Label {
            text: "Start typing to search apps, files, commands, and actions."
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Row {
            spacing: 8

            Repeater {
                model: ["settings", "documents", "terminal", "> open appdeck"]

                delegate: Rectangle {
                    height: 30
                    width: chip.implicitWidth + 20
                    radius: Theme.radiusPill
                    color: hover.containsMouse ? Theme.color("pulseBadgeActive") : Theme.color("pulseBadgeBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("pulseBadgeBorder")

                    Label {
                        id: chip
                        anchors.centerIn: parent
                        text: modelData
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("xs")
                    }

                    MouseArea {
                        id: hover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.suggestionChosen(String(modelData || ""))
                    }
                }
            }
        }
    }
}
