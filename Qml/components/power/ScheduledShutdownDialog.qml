import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

FocusScope {
    id: root

    signal cancelRequested()
    signal scheduleAtRequested(string action, date executeAt)
    signal scheduleInRequested(string action, int minutes)
    signal executeNowRequested(string action)

    property string action: "shutdown"
    property string mode: "InDuration"
    property date atValue: new Date()
    property int durationMinutes: 30

    function normalizedAtValue() {
        var now = new Date()
        var selected = new Date(root.atValue)
        var when = new Date(now.getFullYear(), now.getMonth(), now.getDate(),
                            selected.getHours(), selected.getMinutes(), 0, 0)
        if (when.getTime() <= now.getTime()) {
            when.setDate(when.getDate() + 1)
        }
        return when
    }

    implicitWidth: 480
    implicitHeight: card.implicitHeight

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 48)
        radius: Theme.radiusWindow
        color: Theme.color("surface")
        border.color: Theme.color("panelBorder")
        border.width: Theme.borderWidthThin

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.action === "restart" ? qsTr("Schedule Restart") : qsTr("Schedule Shutdown")
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSize("subtitle")
                font.weight: Theme.fontWeight("semibold")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [
                        { key: "Now", label: qsTr("Now") },
                        { key: "AtTime", label: qsTr("At") },
                        { key: "InDuration", label: qsTr("In") }
                    ]
                    delegate: DSStyle.Button {
                        Layout.fillWidth: true
                        text: modelData.label
                        checkable: true
                        checked: root.mode === modelData.key
                        Accessible.name: text
                        onClicked: root.mode = modelData.key
                    }
                }
            }

            DSStyle.TimePicker {
                Layout.fillWidth: true
                visible: root.mode === "AtTime"
                use24Hour: true
                minuteStep: 5
                value: root.atValue
                onValueEdited: function(v) { root.atValue = v }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.mode === "InDuration"
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [
                            { label: qsTr("15 min"), minutes: 15 },
                            { label: qsTr("30 min"), minutes: 30 },
                            { label: qsTr("1 hour"), minutes: 60 },
                            { label: qsTr("2 hours"), minutes: 120 }
                        ]
                        delegate: DSStyle.Button {
                            Layout.fillWidth: true
                            text: modelData.label
                            onClicked: root.durationMinutes = modelData.minutes
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    DSStyle.Slider {
                        from: 5
                        to: 24 * 60
                        stepSize: 5
                        value: root.durationMinutes
                        Layout.fillWidth: true
                        onMoved: root.durationMinutes = Math.round(value / 5) * 5
                    }
                    DSStyle.Label {
                        text: root.durationMinutes < 60
                              ? qsTr("%1 min").arg(root.durationMinutes)
                              : qsTr("%1 h %2 min").arg(Math.floor(root.durationMinutes / 60)).arg(root.durationMinutes % 60)
                        color: Theme.color("textSecondary")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                DSStyle.Button {
                    text: qsTr("Cancel")
                    focus: true
                    Accessible.name: qsTr("Cancel")
                    onClicked: root.cancelRequested()
                }

                Item { Layout.fillWidth: true }

                DSStyle.Button {
                    text: root.mode === "Now" ? qsTr("Run Now") : qsTr("Schedule")
                    Accessible.name: text
                    onClicked: {
                        if (root.mode === "Now") {
                            root.executeNowRequested(root.action)
                        } else if (root.mode === "AtTime") {
                            root.scheduleAtRequested(root.action, root.normalizedAtValue())
                        } else {
                            root.scheduleInRequested(root.action, root.durationMinutes)
                        }
                    }
                }
            }
        }
    }
}
