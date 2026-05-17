import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight + 48
    clip: true
    property string highlightSettingId: ""
    property bool settingsUnlocked: false
    property bool unlockBusy: false
    property string unlockRequestId: ""

    property var autoTimeBinding:      SettingsApp.createBindingFor("timedate", "auto-time",        true)
    property var ntpServerBinding:     SettingsApp.createBindingFor("timedate", "ntp-server",       "time.google.com")
    property var autoTimezoneBinding:  SettingsApp.createBindingFor("timedate", "auto-timezone",    true)
    property var timezoneBinding:      SettingsApp.createBindingFor("timedate", "timezone",         "Asia/Jakarta")
    property var use24HourBinding:     SettingsApp.createBindingFor("timedate", "use-24-hour",      false)
    property var showSecondsBinding:   SettingsApp.createBindingFor("timedate", "show-seconds",     false)
    property var showDayOfWeekBinding: SettingsApp.createBindingFor("timedate", "show-day-of-week", true)
    property var showDateBinding:      SettingsApp.createBindingFor("timedate", "show-date",        true)

    property bool applyBusy: false
    property string applyError: ""
    property string authError: ""

    function relockSettings() {
        settingsUnlocked = false
        unlockBusy = false
        unlockRequestId = ""
    }

    onVisibleChanged: {
        if (!visible) relockSettings()
    }

    function applyDateTimeNow() {
        if (applyBusy) return
        applyBusy = true
        applyError = ""
        var result = TimeDateController.setDateTime(
            yearSpin.value,
            monthCombo.currentIndex + 1,
            daySpin.value,
            hourSpin.value,
            minuteSpin.value,
            0)
        applyBusy = false
        if (result && result.ok === true) {
            clockState.now = new Date()
            dateTimeDialog.close()
            return
        }
        applyError = String(result && result.message ? result.message : qsTr("Failed to apply date/time."))
    }

    function submitDateTimeFromDialog() {
        if (applyBusy) return
        if (!settingsUnlocked) {
            applyError = qsTr("Unlock settings first to make changes.")
            return
        }
        applyError = ""
        applyDateTimeNow()
    }

    // ── Live clock state ──────────────────────────────────────────────────────
    QtObject {
        id: clockState
        property var now: new Date()
        property int hours:   now.getHours()
        property int minutes: now.getMinutes()
        property int seconds: now.getSeconds()

        readonly property var monthNames: [
            qsTr("January"), qsTr("February"), qsTr("March"),    qsTr("April"),
            qsTr("May"),     qsTr("June"),     qsTr("July"),      qsTr("August"),
            qsTr("September"), qsTr("October"), qsTr("November"), qsTr("December")
        ]
        readonly property var monthNamesShort: [
            "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
        ]
        readonly property var dayNames: [
            qsTr("Sunday"),   qsTr("Monday"), qsTr("Tuesday"), qsTr("Wednesday"),
            qsTr("Thursday"), qsTr("Friday"), qsTr("Saturday")
        ]
        readonly property var dayNamesShort: ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"]

        property string formattedTime: {
            var h = hours, m = minutes
            if (Boolean(root.use24HourBinding.value))
                return h.toString().padStart(2, '0') + ":" + m.toString().padStart(2, '0')
            var ampm = h >= 12 ? "PM" : "AM"
            h = h % 12; if (h === 0) h = 12
            return h + ":" + m.toString().padStart(2, '0') + " " + ampm
        }

        property string formattedDate: {
            return dayNames[now.getDay()] + ", " +
                   monthNames[now.getMonth()] + " " +
                   now.getDate() + ", " + now.getFullYear()
        }

        // Live preview of what the crown bar will show
        property string crownPreview: {
            var parts = []
            if (Boolean(root.showDayOfWeekBinding.value))
                parts.push(dayNamesShort[now.getDay()])
            if (Boolean(root.showDateBinding.value))
                parts.push(monthNamesShort[now.getMonth()] + " " + now.getDate())
            var h = hours, m = minutes
            var t
            if (Boolean(root.use24HourBinding.value)) {
                t = h.toString().padStart(2, '0') + ":" + m.toString().padStart(2, '0')
            } else {
                var ampm = h >= 12 ? "PM" : "AM"
                h = h % 12; if (h === 0) h = 12
                t = h + ":" + m.toString().padStart(2, '0') + " " + ampm
            }
            if (Boolean(root.showSecondsBinding.value))
                t += ":" + seconds.toString().padStart(2, '0')
            parts.push(t)
            return parts.join("  ")
        }

        property string timezoneCity: {
            var tz = String(root.timezoneBinding.value)
            return tz.split("/").pop().replace(/_/g, " ")
        }
    }

    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: clockState.now = new Date()
    }

    // ── Layout ────────────────────────────────────────────────────────────────
    ColumnLayout {
        id: mainLayout
        anchors.left:      parent.left
        anchors.right:     parent.right
        anchors.top:       parent.top
        anchors.margins:   24
        anchors.topMargin: 32
        spacing: 24

        // ── Hero card ─────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: heroRow.implicitHeight + 48
            Layout.bottomMargin: 4

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusWindow
                color: Qt.rgba(0.5, 0.5, 0.5, 0.05)
                border.color: Qt.rgba(0.5, 0.5, 0.5, 0.10)
                border.width: Theme.borderWidthThin
            }

            RowLayout {
                id: heroRow
                anchors.centerIn: parent
                spacing: 28

                // Analog clock face
                Item {
                    id: clockFace
                    width: 120; height: 120

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: Qt.rgba(0.5, 0.5, 0.5, 0.07)
                        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.20)
                        border.width: Theme.borderWidthThin
                    }

                    Repeater {
                        model: 12
                        Item {
                            anchors.centerIn: parent
                            width: parent.width; height: parent.height
                            rotation: index * 30
                            Rectangle {
                                width:  (index % 3 === 0) ? 2.5 : 1
                                height: (index % 3 === 0) ? 10  : 5
                                color:  Qt.rgba(0.5, 0.5, 0.5, (index % 3 === 0) ? 0.60 : 0.28)
                                radius: width / 2
                                anchors.top: parent.top
                                anchors.topMargin: 9
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    Item {
                        anchors.centerIn: parent
                        width: parent.width; height: parent.height
                        rotation: (clockState.hours % 12) * 30 + clockState.minutes * 0.5
                        Rectangle {
                            width: 3.5; height: 30
                            radius: width / 2
                            color: Theme.color("textPrimary")
                            anchors.bottom: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }

                    Item {
                        anchors.centerIn: parent
                        width: parent.width; height: parent.height
                        rotation: clockState.minutes * 6 + clockState.seconds * 0.1
                        Rectangle {
                            width: 2.5; height: 42
                            radius: width / 2
                            color: Theme.color("textPrimary")
                            anchors.bottom: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }

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
                            width: 1.5; height: 47
                            radius: width / 2
                            color: Theme.color("accent")
                            anchors.bottom: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Rectangle {
                            width: 1.5; height: 13
                            radius: width / 2
                            color: Theme.color("accent")
                            anchors.top: parent.verticalCenter
                            anchors.topMargin: 2
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }

                    Rectangle {
                        width: 8; height: 8
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
                    spacing: 3

                    Text {
                        text: clockState.formattedTime
                        font.pixelSize: Theme.fontSize("titleLarge")
                        font.weight: Theme.fontWeight("light")
                        color: Theme.color("textPrimary")
                    }

                    Text {
                        text: clockState.formattedDate
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textSecondary")
                    }

                    Text {
                        text: clockState.timezoneCity
                        font.pixelSize: Theme.fontSize("caption")
                        color: Theme.color("textTertiary")
                        topPadding: 2
                    }
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
                    : qsTr("Use a network time server to keep your clock accurate.")
                Layout.fillWidth: true

                SettingToggle {
                    id: autoTimeToggle
                    checked: Boolean(root.autoTimeBinding.value)
                    onToggled: root.autoTimeBinding.value = checked
                }
            }

            // Manual date & time — single card, shown only when auto is off
            SettingCard {
                objectName: "date-time-manual"
                visible: !autoTimeToggle.checked
                highlighted: root.highlightSettingId === "date" || root.highlightSettingId === "time"
                label: qsTr("Date & Time")
                description: clockState.formattedDate + "   ·   " + clockState.formattedTime
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8

                    Button {
                        visible: !root.settingsUnlocked
                        text: root.unlockBusy ? qsTr("Unlocking…") : qsTr("Unlock…")
                        enabled: !root.unlockBusy
                        onClicked: {
                            root.authError = ""
                            root.unlockBusy = true
                            root.unlockRequestId = SettingsApp.requestSettingAuthorization("timedate", "time")
                        }
                    }

                    Button {
                        visible: root.settingsUnlocked
                        text: qsTr("Change…")
                        onClicked: {
                            var now = new Date()
                            yearSpin.value        = now.getFullYear()
                            monthCombo.currentIndex = now.getMonth()
                            daySpin.value         = now.getDate()
                            hourSpin.value        = now.getHours()
                            minuteSpin.value      = now.getMinutes()
                            root.applyError = ""
                            dateTimeDialog.open()
                        }
                    }
                }
            }

            // Auth error (shown outside the dialog, below the card)
            Text {
                visible: root.authError.length > 0
                text: root.authError
                font.pixelSize: Theme.fontSize("caption")
                color: Theme.color("error")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 4
            }
        }

        // ── Time Zone ─────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Time Zone")
            Layout.fillWidth: true

            SettingCard {
                objectName: "auto-timezone"
                highlighted: root.highlightSettingId === "auto-timezone"
                label: qsTr("Set Automatically")
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
                        "Africa/Cairo",        "America/Chicago",    "America/Denver",
                        "America/Los_Angeles", "America/New_York",   "America/Sao_Paulo",
                        "Asia/Bangkok",        "Asia/Dubai",         "Asia/Hong_Kong",
                        "Asia/Jakarta",        "Asia/Karachi",       "Asia/Kolkata",
                        "Asia/Seoul",          "Asia/Shanghai",      "Asia/Singapore",
                        "Asia/Taipei",         "Asia/Tokyo",         "Australia/Adelaide",
                        "Australia/Sydney",    "Europe/Amsterdam",   "Europe/Berlin",
                        "Europe/Istanbul",     "Europe/London",      "Europe/Moscow",
                        "Europe/Paris",        "Pacific/Auckland",   "Pacific/Honolulu",
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

            // Live crown preview — updates as toggles change
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                Layout.topMargin: 4
                Layout.bottomMargin: 2

                Rectangle {
                    anchors.centerIn: parent
                    width: previewLabel.implicitWidth + 36
                    height: 30
                    radius: Theme.radiusWindowAlt
                    color: Qt.rgba(0.5, 0.5, 0.5, 0.08)
                    border.color: Qt.rgba(0.5, 0.5, 0.5, 0.15)
                    border.width: Theme.borderWidthThin

                    Text {
                        id: previewLabel
                        anchors.centerIn: parent
                        text: clockState.crownPreview
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textPrimary")
                        Behavior on text { }
                    }
                }
            }

            SettingCard {
                objectName: "use-24-hour"
                highlighted: root.highlightSettingId === "use-24-hour"
                label: qsTr("Use 24-Hour Time")
                description: qsTr("Show hours from 0 to 23 instead of AM/PM.")
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

    // ── Connections ───────────────────────────────────────────────────────────
    Connections {
        target: root.timezoneBinding
        function onValueChanged() {
            timezoneCombo.currentIndex = Math.max(0, timezoneCombo.model.indexOf(String(root.timezoneBinding.value)))
        }
    }

    Connections {
        target: SettingsApp
        function onSettingAuthorizationFinished(requestId, moduleId, settingId, allowed, reason) {
            if (String(requestId) !== root.unlockRequestId) return
            root.unlockBusy = false
            root.unlockRequestId = ""
            if (!allowed) {
                root.authError = String(reason || qsTr("Authorization denied."))
                return
            }
            root.settingsUnlocked = true
            root.authError = ""
        }
    }

    // ── Set Date & Time dialog ────────────────────────────────────────────────
    Dialog {
        id: dateTimeDialog
        modal: true
        title: qsTr("Set Date & Time")
        standardButtons: Dialog.Cancel
        anchors.centerIn: Overlay.overlay

        contentItem: ColumnLayout {
            spacing: 20

            GridLayout {
                columns: 2
                columnSpacing: 20
                rowSpacing: 14

                // Date row
                Text {
                    text: qsTr("Date")
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textSecondary")
                    Layout.alignment: Qt.AlignVCenter
                }

                RowLayout {
                    spacing: 6

                    ComboBox {
                        id: monthCombo
                        implicitWidth: 130
                        model: clockState.monthNames
                    }

                    SpinBox {
                        id: daySpin
                        from: 1; to: 31
                        value: new Date().getDate()
                        editable: true
                        implicitWidth: 76
                    }

                    SpinBox {
                        id: yearSpin
                        from: 1970; to: 2099
                        value: new Date().getFullYear()
                        editable: true
                        implicitWidth: 92
                    }
                }

                // Time row
                Text {
                    text: qsTr("Time")
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textSecondary")
                    Layout.alignment: Qt.AlignVCenter
                }

                RowLayout {
                    spacing: 4

                    SpinBox {
                        id: hourSpin
                        from: 0; to: 23
                        value: new Date().getHours()
                        editable: true
                        implicitWidth: 76
                    }

                    Text {
                        text: ":"
                        font.pixelSize: Theme.fontSize("title")
                        font.weight: Theme.fontWeight("light")
                        color: Theme.color("textSecondary")
                        verticalAlignment: Text.AlignVCenter
                    }

                    SpinBox {
                        id: minuteSpin
                        from: 0; to: 59
                        value: new Date().getMinutes()
                        editable: true
                        implicitWidth: 76
                    }
                }
            }

            Text {
                visible: root.applyError.length > 0
                text: root.applyError
                color: Theme.color("error")
                font.pixelSize: Theme.fontSize("caption")
                wrapMode: Text.Wrap
                Layout.preferredWidth: 400
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: root.applyBusy ? qsTr("Applying…") : qsTr("Apply")
                    enabled: !root.applyBusy
                    onClicked: root.submitDateTimeFromDialog()
                }
            }
        }
    }
}
