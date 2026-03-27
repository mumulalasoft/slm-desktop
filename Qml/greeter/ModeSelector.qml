import QtQuick 2.15
import QtQuick.Controls 2.15

// Mode selector button — looks like the SDDM "Other..." session button.
// Opens a popup listing Normal / Safe Mode / Recovery.
Column {
    id: root
    spacing: Math.round(8 * uiScale)

    property real   uiScale: 1.0
    property string currentMode: "normal"

    signal modeSelected(string mode)

    readonly property var modes: [
        { label: "Normal",    value: "normal",   desc: "Sesi biasa" },
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
            color: modeButton.down ? "#44ffffff"
                   : (root.currentMode !== "normal" ? "#200a84ff" : "transparent")
            border.width: 1
            border.color: root.currentMode !== "normal" ? "#0a84ff" : "#ccffffff"
        }
        contentItem: Text {
            text: "⇄"
            color: root.currentMode !== "normal" ? "#0a84ff" : "#f5ffffff"
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
                color: "#e31a2433"
                border.width: 1
                border.color: "#70ffffff"
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
                            color: parent.highlighted ? "#0a84ff" : "transparent"
                        }
                        contentItem: Column {
                            spacing: 1
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: Math.round(10 * root.uiScale)

                            Text {
                                text: modelData.label
                                color: "#ffffff"
                                font.pixelSize: Math.round(15 * root.uiScale)
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.desc
                                color: "#aaffffff"
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
        color: root.currentMode !== "normal" ? "#88c8ff" : "#d8ffffff"
        anchors.horizontalCenter: parent.horizontalCenter
        font.pixelSize: Math.round(14 * root.uiScale)
    }
}
