import QtQuick 2.15
import Slm_Desktop

Row {
    id: root

    property var iconProvider: null
    property int buttonSize: 12
    spacing: 0

    signal closeRequested()
    signal minimizeRequested()
    signal maximizeRequested()

    function iconSourceFor(kind, hovered, pressed) {
        if (iconProvider && iconProvider.call) {
            return String(iconProvider(kind, hovered, pressed) || "")
        }
        return ""
    }

    Repeater {
        model: [
            { "kind": "close", "fallback": "#ff5f57", "action": "close" },
            { "kind": "minimize", "fallback": "#ffbd2e", "action": "minimize" },
            { "kind": "maximize", "fallback": "#28c840", "action": "maximize" }
        ]

        delegate: Item {
            required property var modelData

            width: root.buttonSize
            height: root.buttonSize

            property bool hovered: mouse.containsMouse
            property bool pressed: mouse.pressed
            property string iconSource: root.iconSourceFor(String(modelData.kind || ""), hovered, pressed)

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: iconSource.length > 0 ? "transparent" : String(modelData.fallback || Theme.color("controlBg"))
                border.width: Theme.borderWidthThin
                border.color: Theme.color("panelBorder")
            }

            Image {
                anchors.fill: parent
                source: iconSource
                visible: String(iconSource || "").length > 0
                smooth: true
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    var action = String(parent.modelData.action || "")
                    if (action === "close") {
                        root.closeRequested()
                    } else if (action === "minimize") {
                        root.minimizeRequested()
                    } else if (action === "maximize") {
                        root.maximizeRequested()
                    }
                }
            }
        }
    }
}
