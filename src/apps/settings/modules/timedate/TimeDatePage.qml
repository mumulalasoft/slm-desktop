import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight + 48
    clip: true
    property string highlightSettingId: ""

    property var autoTimeBinding:      SettingsApp.createBindingFor("timedate", "auto-time",        true)
    property var ntpServerBinding:     SettingsApp.createBindingFor("timedate", "ntp-server",       "time.google.com")
    property var autoTimezoneBinding:  SettingsApp.createBindingFor("timedate", "auto-timezone",    true)
    property var timezoneBinding:      SettingsApp.createBindingFor("timedate", "timezone",         "Asia/Jakarta")
    property var use24HourBinding:     SettingsApp.createBindingFor("timedate", "use-24-hour",      false)
    property var showSecondsBinding:   SettingsApp.createBindingFor("timedate", "show-seconds",     false)
    property var showDayOfWeekBinding: SettingsApp.createBindingFor("timedate", "show-day-of-week", true)
    property var showDateBinding:      SettingsApp.createBindingFor("timedate", "show-date",        true)

    // ── Live clock state ─────────────────────────────────────────────────────
    QtObject {
        id: clockState
        property var   now:     new Date()
        property int   hours:   now.getHours()
        property int   minutes: now.getMinutes()
        property int   seconds: now.getSeconds()

        property string formattedTime: {
            var h = hours
            var m = minutes
            if (Boolean(root.use24HourBinding.value)) {
                return h.toString().padStart(2, '0') + ":" + m.toString().padStart(2, '0')
            }
            var ampm = h >= 12 ? "PM" : "AM"
            h = h % 12
            if (h === 0) h = 12
            return h + ":" + m.toString().padStart(2, '0') + " " + ampm
        }

        property string formattedDate: {
            var days   = ["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"]
            var months = ["January","February","March","April","May","June",
                          "July","August","September","October","November","December"]
            return days[now.getDay()] + ", " + now.getDate() + " " + months[now.getMonth()] + " " + now.getFullYear()
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: clockState.now = new Date()
    }

    // ── Layout ───────────────────────────────────────────────────────────────
    ColumnLayout {
        id: mainLayout
        anchors.left:  parent.left
        anchors.right: parent.right
        anchors.top:   parent.top
        anchors.margins:    24
        anchors.topMargin:  32
        spacing: 24

        // ── Hero: analog + digital ────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth:    true
            Layout.bottomMargin: 8
            spacing: 20

            // Analog clock face — pure QML, no Canvas
            Item {
                id: clockFace
                width: 96; height: 96
                Layout.alignment: Qt.AlignHCenter

                // Face background
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: Qt.rgba(0.5, 0.5, 0.5, 0.07)
                    border.color: Qt.rgba(0.5, 0.5, 0.5, 0.20)
                    border.width: Theme.borderWidthThin
                }

                // Hour tick marks
                Repeater {
                    model: 12
                    Item {
                        anchors.centerIn: parent
                        width: parent.width; height: parent.height
                        rotation: index * 30
                        Rectangle {
                            width:  (index % 3 === 0) ? 2   : 1
                            height: (index % 3 === 0) ? 9   : 5
                            color:  Qt.rgba(0.5, 0.5, 0.5, (index % 3 === 0) ? 0.60 : 0.28)
                            radius: width / 2
                            anchors.top: parent.top
                            anchors.topMargin: 8
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }

                // Hour hand
                Item {
                    anchors.centerIn: parent
                    width: parent.width; height: parent.height
                    rotation: (clockState.hours % 12) * 30 + clockState.minutes * 0.5
                    Rectangle {
                        width: 3.5; height: 24
                        radius: width / 2
                        color: Theme.color("textPrimary")
                        anchors.bottom: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                // Minute hand
                Item {
                    anchors.centerIn: parent
                    width: parent.width; height: parent.height
                    rotation: clockState.minutes * 6 + clockState.seconds * 0.1
                    Rectangle {
                        width: 2.5; height: 33
                        radius: width / 2
                        color: Theme.color("textPrimary")
                        anchors.bottom: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                // Second hand + tail
                Item {
                    anchors.centerIn: parent
                    width: parent.width; height: parent.height
                    rotation: clockState.seconds * 6

                    Behavior on rotation {
                        enabled: clockState.seconds !== 0
                        RotationAnimation {
                            duration: Theme.durationSm
                            easing.type: Theme.easingSpring
                            direction: RotationAnimation.Clockwise
                        }
                    }

                    Rectangle {
                        width: 1.5; height: 37
                        radius: width / 2
                        color: Theme.color("accent")
                        anchors.bottom: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Rectangle {
                        width: 1.5; height: 10
                        radius: width / 2
                        color: Theme.color("accent")
                        anchors.top: parent.verticalCenter
                        anchors.topMargin: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                // Center cap
                Rectangle {
                    width: 7; height: 7
                    radius: width / 2
                    color: Theme.color("accent")
                    border.color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    anchors.centerIn: parent
                    z: 10
                }
            }

            // Digital readout
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4

                Text {
                    text: clockState.formattedTime
                    font.pixelSize: Theme.fontSize("titleLarge")
                    font.weight:    Theme.fontWeight("light")
                    color: Theme.color("textPrimary")
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: clockState.formattedDate
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textSecondary")
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        // ── Date & Time ───────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Date & Time")
            Layout.fillWidth: true

            SettingCard {
                objectName: "auto-time"
                highlighted: root.highlightSettingId === "auto-time"
                label: qsTr("Set Automatically")
                description: autoTimeToggle.checked
                    ? qsTr("Syncing with %1").arg(String(root.ntpServerBinding.value))
                    : qsTr("Synchronize date and time using a network time server.")
                Layout.fillWidth: true

                SettingToggle {
                    id: autoTimeToggle
                    checked: Boolean(root.autoTimeBinding.value)
                    onToggled: root.autoTimeBinding.value = checked
                }
            }

            SettingCard {
                objectName: "date"
                visible: !autoTimeToggle.checked
                highlighted: root.highlightSettingId === "date"
                label: qsTr("Date")
                description: clockState.formattedDate
                Layout.fillWidth: true

                Button { text: qsTr("Change…") }
            }

            SettingCard {
                objectName: "time"
                visible: !autoTimeToggle.checked
                highlighted: root.highlightSettingId === "time"
                label: qsTr("Time")
                description: clockState.formattedTime
                Layout.fillWidth: true

                Button { text: qsTr("Change…") }
            }
        }

        // ── Time Zone ─────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Time Zone")
            Layout.fillWidth: true

            SettingCard {
                objectName: "auto-timezone"
                highlighted: root.highlightSettingId === "auto-timezone"
                label: qsTr("Set Time Zone Automatically")
                description: qsTr("Use your current location to determine the time zone.")
                Layout.fillWidth: true

                SettingToggle {
                    id: autoTimezoneToggle
                    checked: Boolean(root.autoTimezoneBinding.value)
                    onToggled: root.autoTimezoneBinding.value = checked
                }
            }

            SettingCard {
                objectName: "timezone"
                visible: !autoTimezoneToggle.checked
                highlighted: root.highlightSettingId === "timezone"
                label: qsTr("Time Zone")
                Layout.fillWidth: true

                ComboBox {
                    id: timezoneCombo
                    implicitWidth: 220
                    model: [
                        "Africa/Cairo",
                        "America/Chicago",
                        "America/Denver",
                        "America/Los_Angeles",
                        "America/New_York",
                        "America/Sao_Paulo",
                        "Asia/Bangkok",
                        "Asia/Dubai",
                        "Asia/Hong_Kong",
                        "Asia/Jakarta",
                        "Asia/Karachi",
                        "Asia/Kolkata",
                        "Asia/Seoul",
                        "Asia/Shanghai",
                        "Asia/Singapore",
                        "Asia/Taipei",
                        "Asia/Tokyo",
                        "Australia/Adelaide",
                        "Australia/Sydney",
                        "Europe/Amsterdam",
                        "Europe/Berlin",
                        "Europe/Istanbul",
                        "Europe/London",
                        "Europe/Moscow",
                        "Europe/Paris",
                        "Pacific/Auckland",
                        "Pacific/Honolulu",
                        "UTC"
                    ]
                    currentIndex: Math.max(0, model.indexOf(String(root.timezoneBinding.value)))
                    onActivated: root.timezoneBinding.value = currentText
                }
            }
        }

        // ── Clock ─────────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Clock")
            Layout.fillWidth: true

            SettingCard {
                objectName: "use-24-hour"
                highlighted: root.highlightSettingId === "use-24-hour"
                label: qsTr("Use 24-Hour Time")
                description: qsTr("Show hours from 0 to 23 instead of using AM/PM.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: Boolean(root.use24HourBinding.value)
                    onToggled: root.use24HourBinding.value = checked
                }
            }

            SettingCard {
                objectName: "show-seconds"
                highlighted: root.highlightSettingId === "show-seconds"
                label: qsTr("Show Seconds")
                description: qsTr("Display seconds in the menu bar clock.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: Boolean(root.showSecondsBinding.value)
                    onToggled: root.showSecondsBinding.value = checked
                }
            }

            SettingCard {
                objectName: "show-day-of-week"
                highlighted: root.highlightSettingId === "show-day-of-week"
                label: qsTr("Show Day of Week")
                description: qsTr("Display the abbreviated day name next to the time.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: Boolean(root.showDayOfWeekBinding.value)
                    onToggled: root.showDayOfWeekBinding.value = checked
                }
            }

            SettingCard {
                objectName: "show-date"
                highlighted: root.highlightSettingId === "show-date"
                label: qsTr("Show Date")
                description: qsTr("Display the date alongside the time in the menu bar.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: Boolean(root.showDateBinding.value)
                    onToggled: root.showDateBinding.value = checked
                }
            }
        }
    }

    Connections {
        target: root.timezoneBinding
        function onValueChanged() {
            timezoneCombo.currentIndex = Math.max(0, timezoneCombo.model.indexOf(String(root.timezoneBinding.value)))
        }
    }
}
