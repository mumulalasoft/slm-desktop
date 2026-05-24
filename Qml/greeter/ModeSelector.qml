import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

// Mode selector button — looks like the SDDM "Other..." session button.
// Opens a popup listing Safe Mode / Recovery.
Column {
    id: root
    spacing: Math.round(8 * uiScale)

    property real   uiScale: 1.0
    property string currentMode: "normal"

    signal modeSelected(string mode)

    readonly property bool isAltMode: root.currentMode !== "normal"

    readonly property var modes: [
        { label: "Safe Mode", value: "safe",      desc: "Konfigurasi default" },
        { label: "Recovery",  value: "recovery",  desc: "Pemulihan" },
    ]

    function labelForMode(value) {
        for (var i = 0; i < modes.length; ++i) {
            if (modes[i].value === value) return modes[i].label
        }
        return "Normal"
    }

    Button {
        id: modeButton
        width: Math.round(48 * root.uiScale)
        height: width
        onClicked: modePopup.open()
        background: Rectangle {
            radius: width / 2
            color: modeButton.down ? Qt.rgba(1, 1, 1, 0.267)
                   : (root.isAltMode ? Qt.rgba(0.039, 0.518, 1.0, 0.125) : "transparent")
            border.width: Theme.borderWidthThin
            border.color: root.isAltMode ? Qt.rgba(0.039, 0.518, 1.0, 1.0) : Qt.rgba(1, 1, 1, 0.800)
            Behavior on color { ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingStandard } }
            Behavior on border.color { ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingStandard } }
        }
        contentItem: Text {
            text: "⇄"
            color: root.isAltMode ? Qt.rgba(0.039, 0.518, 1.0, 1.0) : Qt.rgba(1, 1, 1, 0.961)
            font.pixelSize: Math.round(24 * root.uiScale)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Popup {
            id: modePopup
            y: modeButton.height + Math.round(8 * root.uiScale)
            x: -Math.round(95 * root.uiScale)
            width: Math.round(240 * root.uiScale)
            modal: false
            focus: true
            padding: Math.round(8 * root.uiScale)
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            background: Rectangle {
                radius: Math.round(10 * root.uiScale)
                color: Qt.rgba(0.102, 0.141, 0.200, 0.890)
                border.width: Theme.borderWidthThin
                border.color: Qt.rgba(1, 1, 1, 0.439)
            }

            Column {
                width: parent.width
                spacing: 0

                Repeater {
                    model: root.modes
                    delegate: ItemDelegate {
                        width: parent.width
                        highlighted: root.currentMode === modelData.value
                        onClicked: {
                            root.currentMode = modelData.value
                            root.modeSelected(modelData.value)
                            modePopup.close()
                        }
                        background: Rectangle {
                            radius: Math.round(8 * root.uiScale)
                            color: parent.highlighted ? Qt.rgba(0.039, 0.518, 1.0, 1.0) : "transparent"
                            Behavior on color {
                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                            }
                        }
                        contentItem: Column {
                            spacing: 1
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: Math.round(10 * root.uiScale)

                            Text {
                                text: modelData.label
                                color: "white"
                                font.pixelSize: Math.round(15 * root.uiScale)
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.desc
                                color: Qt.rgba(1, 1, 1, 0.667)
                                font.pixelSize: Math.round(11 * root.uiScale)
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }

    Label {
        text: root.labelForMode(root.currentMode)
        color: root.isAltMode ? Qt.rgba(0.533, 0.784, 1.0, 1.0) : Qt.rgba(1, 1, 1, 0.847)
        anchors.horizontalCenter: parent.horizontalCenter
        font.pixelSize: Math.round(14 * root.uiScale)
    }
}
