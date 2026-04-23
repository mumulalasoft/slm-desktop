import QtQuick 2.15
import QtQuick.Controls 2.15

Row {
    id: root
    property bool active: false
    property bool emphasize: false
    spacing: 12
    opacity: active ? (emphasize ? 0.9 : 0.62) : 0.0

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationMicro; easing.type: Theme.easingLight }
    }

    readonly property var hints: [
        { "k": "Enter", "t": "Open" },
        { "k": "Esc", "t": "Close" },
        { "k": "↑/↓", "t": "Navigate" },
        { "k": "Alt+←/→", "t": "Section" }
    ]

    Repeater {
        model: root.hints

        delegate: Row {
            spacing: 6

            Rectangle {
                radius: Theme.radiusPill
                color: Theme.color("pulseBadgeBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("pulseBadgeBorder")
                height: 22
                width: Math.max(34, keyLabel.implicitWidth + 12)

                Label {
                    id: keyLabel
                    anchors.centerIn: parent
                    text: String(modelData.k || "")
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Theme.fontWeight("semibold")
                }
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: String(modelData.t || "")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("xs")
            }
        }
    }
}
