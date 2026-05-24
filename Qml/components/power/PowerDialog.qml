import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

FocusScope {
    id: root

    signal cancelRequested()
    signal actionRequested(string action)
    signal scheduleRequested(string action)
    signal scheduleAtRequested(string action, date executeAt)
    signal scheduleInRequested(string action, int minutes)

    property string action: "shutdown"
    property bool busy: false
    property string scheduleMode: "now"
    property date atValue: new Date()
    property int durationMinutes: 30
    readonly property int controlTextSize: Math.max(13, Theme.fontSize("body"))
    readonly property int tabTextSize: Math.max(13, Theme.fontSize("body"))

    readonly property string normalizedAction: String(action || "").toLowerCase()
    readonly property bool contextualAction: normalizedAction === "shutdown"
                                            || normalizedAction === "restart"
                                            || normalizedAction === "sleep"
                                            || normalizedAction === "logout"
    readonly property bool canSchedule: normalizedAction === "shutdown"
                                        || normalizedAction === "restart"
    readonly property color actionColor: normalizedAction === "shutdown"
                                        ? Theme.color("destructive")
                                        : normalizedAction === "restart"
                                          ? Theme.color("accent")
                                          : normalizedAction === "logout"
                                            ? Theme.color("warning")
                                            : Theme.color("accent")

    function titleFor(actionName) {
        switch (actionName) {
        case "restart": return qsTr("Restart this computer?")
        case "sleep": return qsTr("Put this computer to sleep?")
        case "logout": return qsTr("Log out now?")
        case "shutdown":
        default: return qsTr("Shut down this computer?")
        }
    }

    function descriptionFor(actionName) {
        if (root.busy) {
            return qsTr("Preparing your session...")
        }
        switch (actionName) {
        case "restart":
            return qsTr("SLM will ask open apps to close, then restart.")
        case "sleep":
            return qsTr("Your session stays ready and the display will turn off.")
        case "logout":
            return qsTr("SLM will ask open apps to close, then end this session.")
        case "shutdown":
        default:
            return qsTr("SLM will ask open apps to close, then power off.")
        }
    }

    function confirmLabelFor(actionName) {
        switch (actionName) {
        case "restart": return qsTr("Restart")
        case "sleep": return qsTr("Sleep")
        case "logout": return qsTr("Log Out")
        case "shutdown":
        default: return qsTr("Shut Down")
        }
    }

    function iconFor(actionName) {
        switch (actionName) {
        case "restart": return "system-reboot"
        case "sleep": return "system-suspend"
        case "logout": return "system-log-out"
        case "shutdown":
        default: return "system-shutdown"
        }
    }

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

    function durationLabel(minutes) {
        var value = Math.max(1, Number(minutes || 0))
        if (value < 60) {
            return qsTr("%1 min").arg(value)
        }
        var hours = Math.floor(value / 60)
        var mins = value % 60
        if (mins === 0) {
            return hours === 1 ? qsTr("1 hour") : qsTr("%1 hours").arg(hours)
        }
        return qsTr("%1 h %2 min").arg(hours).arg(mins)
    }

    function modeLabel(modeName) {
        var verb = root.normalizedAction === "restart" ? qsTr("Restart") : qsTr("Shut Down")
        if (modeName === "at") {
            return qsTr("%1 At").arg(verb)
        }
        if (modeName === "in") {
            return qsTr("%1 In").arg(verb)
        }
        return qsTr("Now")
    }

    function confirmTextFor(actionName) {
        if (!root.canSchedule || root.scheduleMode === "now") {
            return root.confirmLabelFor(actionName)
        }
        if (root.scheduleMode === "at") {
            return actionName === "restart" ? qsTr("Schedule Restart") : qsTr("Schedule Shutdown")
        }
        return actionName === "restart" ? qsTr("Restart Later") : qsTr("Shut Down Later")
    }

    function runPrimaryAction() {
        if (!root.canSchedule || root.scheduleMode === "now") {
            root.actionRequested(root.normalizedAction)
            return
        }
        if (root.scheduleMode === "at") {
            root.scheduleAtRequested(root.normalizedAction, root.normalizedAtValue())
            return
        }
        root.scheduleInRequested(root.normalizedAction, root.durationMinutes)
    }

    implicitWidth: 500
    implicitHeight: card.height

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(500, parent.width - 48)
        height: Math.min(parent.height - 72, content.implicitHeight + 52)
        radius: 20
        color: Theme.color("surface")
        border.color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.14 : 0.42)
        border.width: Theme.borderWidthThin
        layer.enabled: MotionController.effectsEnabled
        layer.smooth: true

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 26
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 70

                Rectangle {
                    anchors.centerIn: parent
                    width: 58
                    height: 58
                    radius: 29
                    color: Qt.rgba(root.actionColor.r, root.actionColor.g, root.actionColor.b,
                                   Theme.darkMode ? 0.18 : 0.11)
                    border.color: Qt.rgba(root.actionColor.r, root.actionColor.g, root.actionColor.b,
                                          Theme.darkMode ? 0.22 : 0.16)
                    border.width: Theme.borderWidthThin

                    Image {
                        id: actionIcon
                        anchors.centerIn: parent
                        width: 25
                        height: 25
                        source: "image://themeicon/" + root.iconFor(root.normalizedAction)
                        smooth: true
                        fillMode: Image.PreserveAspectFit
                    }

                    DSStyle.Label {
                        anchors.centerIn: parent
                        visible: actionIcon.status !== Image.Ready
                        text: root.normalizedAction === "restart" ? "R"
                              : root.normalizedAction === "sleep" ? "S"
                              : root.normalizedAction === "logout" ? "L" : "P"
                        color: root.actionColor
                        font.pixelSize: Theme.fontSize("subtitle")
                        font.weight: Theme.fontWeight("semibold")
                    }
                }
            }

            DSStyle.Label {
                Layout.fillWidth: true
                Layout.topMargin: 8
                text: root.contextualAction ? root.titleFor(root.normalizedAction) : qsTr("Power")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSize("title")
                font.weight: Theme.fontWeight("semibold")
                color: Theme.color("textPrimary")
            }

            DSStyle.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 10
                text: root.contextualAction
                      ? root.descriptionFor(root.normalizedAction)
                      : qsTr("Choose what you want SLM to do.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                lineHeight: Theme.lineHeightTight
                color: Theme.color("textSecondary")
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 22
                Layout.bottomMargin: 18
                height: 1
                color: Theme.color("panelBorder")
                opacity: Theme.opacityMuted
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !root.contextualAction
                spacing: 10

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10

                    PowerActionButton {
                        text: qsTr("Restart")
                        actionKey: "restart"
                        iconName: "system-reboot"
                        subtitle: qsTr("Close apps, then restart")
                        Layout.fillWidth: true
                        enabled: !root.busy
                        onActionInvoked: root.actionRequested(action)
                    }

                    PowerActionButton {
                        actionKey: "shutdown"
                        iconName: "system-shutdown"
                        subtitle: qsTr("Close apps, then power off")
                        Layout.fillWidth: true
                        enabled: !root.busy
                        onActionInvoked: root.actionRequested(action)
                    }

                    PowerActionButton {
                        text: qsTr("Sleep")
                        actionKey: "sleep"
                        iconName: "system-suspend"
                        subtitle: qsTr("Pause this session")
                        destructive: false
                        Layout.fillWidth: true
                        enabled: !root.busy
                        onActionInvoked: root.actionRequested(action)
                    }

                    PowerActionButton {
                        text: qsTr("Log Out")
                        actionKey: "logout"
                        iconName: "system-log-out"
                        subtitle: qsTr("End this session")
                        destructive: false
                        Layout.fillWidth: true
                        enabled: !root.busy
                        onActionInvoked: root.actionRequested(action)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.canSchedule
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 34
                    radius: 9
                    color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.055)
                                          : Qt.rgba(0, 0, 0, 0.055)
                    border.color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.28 : 0.10)
                    border.width: Theme.borderWidthThin

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 3
                        spacing: 3

                        Repeater {
                            model: [ "now", "at", "in" ]
                            delegate: Rectangle {
                                readonly property bool active: root.scheduleMode === String(modelData)
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 7
                                color: active ? Theme.color("surface") : "transparent"
                                border.color: active
                                              ? Qt.rgba(0, 0, 0, Theme.darkMode ? 0.30 : 0.13)
                                              : "transparent"
                                border.width: active ? Theme.borderWidthThin : 0

                                Behavior on color {
                                    ColorAnimation {
                                        duration: MotionController.preset("hover").duration
                                        easing.type: MotionController.preset("hover").easing
                                    }
                                }

                                DSStyle.Label {
                                    anchors.centerIn: parent
                                    width: parent.width - 8
                                    text: root.modeLabel(String(modelData))
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                    color: active ? Theme.color("textPrimary") : Theme.color("textSecondary")
                                    font.pixelSize: root.tabTextSize
                                    font.weight: active ? Theme.fontWeight("semibold")
                                                        : Theme.fontWeight("regular")
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.scheduleMode = String(modelData)
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.scheduleMode === "now" ? 0
                                           : root.scheduleMode === "at" ? 76 : 94
                    visible: root.scheduleMode !== "now"
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        visible: root.scheduleMode === "at"

                        DSStyle.TimePicker {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: Math.min(320, parent.width)
                            use24Hour: true
                            minuteStep: 5
                            value: root.atValue
                            onValueEdited: function(v) { root.atValue = v }
                        }

                        DSStyle.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("caption")
                            text: {
                                var when = root.normalizedAtValue()
                                var hh = when.getHours()
                                var mm = when.getMinutes()
                                var time = (hh < 10 ? "0" : "") + hh + ":" + (mm < 10 ? "0" : "") + mm
                                return root.normalizedAction === "restart"
                                       ? qsTr("Restart scheduled for %1").arg(time)
                                       : qsTr("Shutdown scheduled for %1").arg(time)
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        visible: root.scheduleMode === "in"

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
                                delegate: Rectangle {
                                    readonly property bool active: root.durationMinutes === Number(modelData.minutes)
                                    Layout.fillWidth: true
                                    implicitHeight: 30
                                    radius: 8
                                    color: active ? Qt.rgba(root.actionColor.r, root.actionColor.g,
                                                           root.actionColor.b, Theme.darkMode ? 0.20 : 0.11)
                                                  : Theme.color("controlBg")
                                    border.color: active ? Qt.rgba(root.actionColor.r, root.actionColor.g,
                                                                   root.actionColor.b, 0.36)
                                                         : Theme.color("panelBorder")
                                    border.width: Theme.borderWidthThin

                                    DSStyle.Label {
                                        anchors.centerIn: parent
                                        text: String(modelData.label || "")
                                        color: active ? root.actionColor : Theme.color("textPrimary")
                                        font.pixelSize: root.controlTextSize
                                        font.weight: active ? Theme.fontWeight("semibold")
                                                            : Theme.fontWeight("regular")
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.durationMinutes = Number(modelData.minutes)
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            DSStyle.Slider {
                                from: 5
                                to: 24 * 60
                                stepSize: 5
                                value: root.durationMinutes
                                Layout.fillWidth: true
                                onMoved: root.durationMinutes = Math.round(value / 5) * 5
                            }

                            DSStyle.Label {
                                Layout.preferredWidth: 82
                                horizontalAlignment: Text.AlignRight
                                color: Theme.color("textSecondary")
                                text: root.durationLabel(root.durationMinutes)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: root.contextualAction ? 20 : 18
                spacing: 10

                DSStyle.Button {
                    id: cancelButton
                    text: qsTr("Cancel")
                    focus: true
                    enabled: !root.busy
                    font.pixelSize: root.controlTextSize
                    Accessible.name: qsTr("Cancel")
                    KeyNavigation.tab: confirmButton
                    onClicked: root.cancelRequested()
                }

                Item { Layout.fillWidth: true }

                Button {
                    id: confirmButton
                    text: root.contextualAction ? root.confirmTextFor(root.normalizedAction)
                                                : qsTr("Continue")
                    enabled: !root.busy
                    font.pixelSize: root.controlTextSize
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: text
                    KeyNavigation.tab: cancelButton
                    KeyNavigation.backtab: cancelButton
                    implicitWidth: Math.max(128, confirmText.implicitWidth + 34)
                    implicitHeight: Theme.metric("controlHeightRegular")
                    onClicked: root.runPrimaryAction()

                    contentItem: Text {
                        id: confirmText
                        text: confirmButton.text
                        font: confirmButton.font
                        color: Theme.color("accentText")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        radius: Theme.radiusControl
                        color: !confirmButton.enabled
                               ? Theme.color("controlDisabledBg")
                               : confirmButton.down
                                 ? Qt.darker(root.actionColor, Theme.darkMode ? 1.24 : 1.14)
                                 : confirmButton.hovered || confirmButton.visualFocus
                                   ? Qt.lighter(root.actionColor, Theme.darkMode ? 1.12 : 1.06)
                                   : root.actionColor
                        border.width: confirmButton.visualFocus ? Theme.borderWidthThin : 0
                        border.color: confirmButton.visualFocus ? Theme.color("focusRing") : "transparent"

                        Behavior on color {
                            ColorAnimation {
                                duration: MotionController.preset("hover").duration
                                easing.type: MotionController.preset("hover").easing
                            }
                        }
                    }
                }
            }
        }
    }
}
