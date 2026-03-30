import QtQuick 2.15
import Slm_Desktop

Item {
    id: root

    required property var iconProvider
    property int buttonSize: 20
    property int spacing: 0

    signal closeRequested()
    signal minimizeRequested()
    signal maximizeRequested()

    implicitWidth: (buttonSize * 3) + (spacing * 2)
    implicitHeight: buttonSize

    Row {
        anchors.fill: parent
        spacing: root.spacing

        Repeater {
            model: [
                { "kind": "close" },
                { "kind": "minimize" },
                { "kind": "maximize" }
            ]

            delegate: Item {
                required property var modelData

                width: root.buttonSize
                height: root.buttonSize
                scale: buttonMouse.pressed ? 0.9 : (buttonMouse.containsMouse ? 1.04 : 1.0)

                Behavior on scale {
                    NumberAnimation {
                        duration: Theme.durationMicro
                        easing.type: Theme.easingDefault
                    }
                }

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: root.iconProvider
                            ? String(root.iconProvider(String(modelData.kind || ""),
                                                       !!buttonMouse.containsMouse,
                                                       !!buttonMouse.pressed))
                            : ""
                    opacity: buttonMouse.pressed ? 0.9 : 1.0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: Theme.durationMicro
                            easing.type: Theme.easingDefault
                        }
                    }
                }

                MouseArea {
                    id: buttonMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        var kind = String(modelData.kind || "")
                        if (kind === "close") {
                            root.closeRequested()
                        } else if (kind === "minimize") {
                            root.minimizeRequested()
                        } else if (kind === "maximize") {
                            root.maximizeRequested()
                        }
                    }
                }
            }
        }
    }
}
