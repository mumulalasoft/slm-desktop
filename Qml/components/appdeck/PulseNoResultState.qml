import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property bool active: false
    property string queryText: ""
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
        spacing: 10

        Label {
            text: "No matching results"
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("semibold")
        }

        Label {
            text: "Try another keyword or use a command-style query."
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: "Query: " + root.queryText
            visible: root.queryText.length > 0
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("xs")
        }

        Row {
            spacing: 8
            Repeater {
                model: ["recent", "apps", "files"]
                delegate: Button {
                    text: modelData
                    onClicked: root.suggestionChosen(String(modelData || ""))
                }
            }
        }
    }
}
