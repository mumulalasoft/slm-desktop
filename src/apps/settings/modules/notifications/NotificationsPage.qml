import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "file:/usr/lib/settings/components"

Item {
    id: root
    anchors.fill: parent

    property string highlightSettingId: ""
    property int navIndex: 0
    property string selectedAppId: ""

    property var enabledBinding: SettingsApp.createBindingFor("notifications", "enabled", true)
    property var lockscreenBinding: SettingsApp.createBindingFor("notifications", "show-on-lockscreen", true)
    property var soundBinding: SettingsApp.createBindingFor("notifications", "notification-sound", true)
    property var badgeBinding: SettingsApp.createBindingFor("notifications", "badge-count", true)
    property var popupPositionBinding: SettingsApp.createBindingFor("notifications", "popup-position", "top-right")
    property var groupBinding: SettingsApp.createBindingFor("notifications", "group-notifications", true)
    property var adaptiveQuietBinding: SettingsApp.createBindingFor("notifications", "adaptive-quiet-mode", true)
    property var bannerDurationBinding: SettingsApp.createBindingFor("notifications", "banner-duration", 5000)
    property var criticalBinding: SettingsApp.createBindingFor("notifications", "allow-critical-alerts", true)
    property var fullscreenBinding: SettingsApp.createBindingFor("notifications", "silence-fullscreen", true)
    property var screenShareBinding: SettingsApp.createBindingFor("notifications", "silence-screen-share", true)
    property var focusBinding: SettingsApp.createBindingFor("notifications", "focus-mode-integration", true)
    property var quietBinding: SettingsApp.createBindingFor("notifications", "deliver-quietly", false)
    readonly property var sectionKeys: ["notifications", "focus", "apps", "history", "advanced"]

    readonly property var navItems: [
        { key: "notifications", label: qsTr("Notifications") },
        { key: "focus", label: qsTr("Focus") },
        { key: "apps", label: qsTr("App Notifications") },
        { key: "history", label: qsTr("History") },
        { key: "advanced", label: qsTr("Advanced") }
    ]

    function durationLabel(ms) {
        var sec = Math.max(1, Math.round(Number(ms || 0) / 1000))
        return sec + " s"
    }

    function sectionIndexForKey(key) {
        var normalized = String(key || "").toLowerCase()
        var idx = sectionKeys.indexOf(normalized)
        return idx >= 0 ? idx : 0
    }

    function sectionIndexForSetting(settingId) {
        var sid = String(settingId || "").trim().toLowerCase()
        if (sid.length === 0) {
            return root.navIndex
        }
        if (sid === "enabled"
                || sid === "show-on-lockscreen"
                || sid === "notification-sound"
                || sid === "badge-count"
                || sid === "popup-position"
                || sid === "group-notifications"
                || sid === "adaptive-quiet-mode") {
            return sectionIndexForKey("notifications")
        }
        if (sid === "focus"
                || sid === "allow-critical-alerts"
                || sid === "silence-fullscreen"
                || sid === "silence-screen-share"
                || sid === "focus-mode-integration"
                || sid === "deliver-quietly") {
            return sectionIndexForKey("focus")
        }
        if (sid === "apps") {
            return sectionIndexForKey("apps")
        }
        if (sid === "history") {
            return sectionIndexForKey("history")
        }
        if (sid === "advanced" || sid === "banner-duration") {
            return sectionIndexForKey("advanced")
        }
        return root.navIndex
    }

    function sectionItemForIndex(index) {
        switch (index) {
        case 0: return notificationsSection
        case 1: return focusSection
        case 2: return appsSection
        case 3: return historySection
        case 4: return advancedSection
        default: return notificationsSection
        }
    }

    function scrollToSection(index) {
        var target = sectionItemForIndex(index)
        if (!target) {
            return
        }
        Qt.callLater(function() {
            if (!panelFlick || !target) {
                return
            }
            var margin = Theme.metric("spacingSm")
            var maxY = Math.max(0, panelFlick.contentHeight - panelFlick.height)
            panelFlick.contentY = Math.max(0, Math.min(target.y - margin, maxY))
        })
    }

    function isHighlighted(settingId) {
        return String(root.highlightSettingId || "") === String(settingId || "")
    }

    function focusSetting(settingId) {
        var sid = String(settingId || "")
        root.highlightSettingId = sid
        root.navIndex = sectionIndexForSetting(sid)
        scrollToSection(root.navIndex)
    }

    onHighlightSettingIdChanged: {
        if (highlightSettingId.length > 0) {
            root.navIndex = sectionIndexForSetting(highlightSettingId)
            scrollToSection(root.navIndex)
        }
    }

    function openAppDetail(appId) {
        selectedAppId = String(appId || "")
        appDetailDrawer.open()
    }

    function readAppPref(name, fallback) {
        if (typeof NotificationSettingsController === "undefined" || !NotificationSettingsController || selectedAppId.length === 0) {
            return fallback
        }
        return NotificationSettingsController.appPreference(selectedAppId, name, fallback)
    }

    function writeAppPref(name, value) {
        if (typeof NotificationSettingsController === "undefined" || !NotificationSettingsController || selectedAppId.length === 0) {
            return
        }
        NotificationSettingsController.setAppPreference(selectedAppId, name, value)
    }

    function readAppPrefFor(appId, name, fallback) {
        if (typeof NotificationSettingsController === "undefined" || !NotificationSettingsController) {
            return fallback
        }
        return NotificationSettingsController.appPreference(String(appId || ""), name, fallback)
    }

    function writeAppPrefFor(appId, name, value) {
        if (typeof NotificationSettingsController === "undefined" || !NotificationSettingsController) {
            return
        }
        NotificationSettingsController.setAppPreference(String(appId || ""), name, value)
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Rectangle {
            SplitView.preferredWidth: Math.max(188, root.width * 0.22)
            SplitView.minimumWidth: 170
            color: Theme.darkMode ? Qt.rgba(0.08, 0.10, 0.13, 0.72) : Qt.rgba(0.96, 0.97, 0.99, 0.84)
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            ListView {
                id: navList
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                model: root.navItems
                spacing: 6
                keyNavigationWraps: true
                currentIndex: root.navIndex
                onCurrentIndexChanged: {
                    root.navIndex = currentIndex
                    root.scrollToSection(currentIndex)
                }

                delegate: ItemDelegate {
                    width: navList.width
                    height: 38
                    text: modelData.label
                    highlighted: ListView.isCurrentItem
                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: ListView.isCurrentItem
                            ? Theme.color("accent")
                            : (parent.hovered ? Theme.color("controlBgHover") : "transparent")
                        opacity: ListView.isCurrentItem ? 0.20 : 1.0
                    }
                    contentItem: Label {
                        text: parent.text
                        color: ListView.isCurrentItem ? Theme.color("textPrimary") : Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: ListView.isCurrentItem ? Theme.fontWeight("semibold") : Theme.fontWeight("medium")
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        navList.currentIndex = index
                        root.navIndex = index
                        root.scrollToSection(index)
                    }
                }
            }
        }

        Flickable {
            id: panelFlick
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            clip: true
            contentHeight: contentCol.implicitHeight + 40

            ColumnLayout {
                id: contentCol
                width: panelFlick.width
                spacing: 18
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20

                Item {
                    id: notificationsSection
                    Layout.fillWidth: true
                    implicitHeight: notificationsColumn.implicitHeight

                    ColumnLayout {
                        id: notificationsColumn
                        width: contentCol.width
                        spacing: 18

                        SettingGroup {
                            title: qsTr("Notifications")
                            Layout.fillWidth: true

                            GridLayout {
                                columns: width > 920 ? 2 : 1
                                columnSpacing: 14
                                rowSpacing: 14
                                Layout.fillWidth: true

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Enable Notifications")
                                    description: qsTr("Allow notifications across Crown, Pulse, and AppDeck surfaces.")
                                    highlighted: root.isHighlighted("enabled")
                                    Switch { checked: !!root.enabledBinding.value; onToggled: root.enabledBinding.value = checked }
                                }

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Show on Lockscreen")
                                    description: qsTr("Show notification previews while device is locked.")
                                    highlighted: root.isHighlighted("show-on-lockscreen")
                                    Switch { checked: !!root.lockscreenBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.lockscreenBinding.value = checked }
                                }

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Notification Sound")
                                    description: qsTr("Play subtle sounds for incoming notifications.")
                                    highlighted: root.isHighlighted("notification-sound")
                                    Switch { checked: !!root.soundBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.soundBinding.value = checked }
                                }

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Badge Count")
                                    description: qsTr("Display unread counters on compatible surfaces.")
                                    highlighted: root.isHighlighted("badge-count")
                                    Switch { checked: !!root.badgeBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.badgeBinding.value = checked }
                                }
                            }

                            SettingCard {
                                Layout.fillWidth: true
                                label: qsTr("Popup Position")
                                description: qsTr("Choose where banners appear on screen.")
                                highlighted: root.isHighlighted("popup-position")
                                RowLayout {
                                    spacing: 8
                                    Repeater {
                                        model: ["top-right", "top-left", "bottom-right", "bottom-left"]
                                        delegate: Button {
                                            text: modelData
                                            checkable: true
                                            checked: String(root.popupPositionBinding.value) === modelData
                                            onClicked: root.popupPositionBinding.value = modelData
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 14

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Group Notifications")
                                    description: qsTr("Stack by app, collapse intelligently, and keep the feed tidy.")
                                    highlighted: root.isHighlighted("group-notifications")
                                    Switch { checked: !!root.groupBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.groupBinding.value = checked }
                                }

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Adaptive Quiet Mode")
                                    description: qsTr("Reduce interruptions when activity intensity is high.")
                                    highlighted: root.isHighlighted("adaptive-quiet-mode")
                                    Switch { checked: !!root.adaptiveQuietBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.adaptiveQuietBinding.value = checked }
                                }
                            }
                        }
                    }
                }

                Item {
                    id: focusSection
                    Layout.fillWidth: true
                    implicitHeight: focusColumn.implicitHeight

                    ColumnLayout {
                        id: focusColumn
                        width: contentCol.width
                        spacing: 18

                        SettingGroup {
                            title: qsTr("Focus")
                            Layout.fillWidth: true

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Allow Critical Alerts")
                                    description: qsTr("Critical alerts always surface even in quiet states.")
                                    highlighted: root.isHighlighted("allow-critical-alerts")
                                    Switch { checked: !!root.criticalBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.criticalBinding.value = checked }
                                }

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Silence During Fullscreen")
                                    description: qsTr("Suppress normal notifications while fullscreen.")
                                    highlighted: root.isHighlighted("silence-fullscreen")
                                    Switch { checked: !!root.fullscreenBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.fullscreenBinding.value = checked }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Silence During Screen Share")
                                    description: qsTr("Hide interruptive alerts while sharing your screen.")
                                    highlighted: root.isHighlighted("silence-screen-share")
                                    Switch { checked: !!root.screenShareBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.screenShareBinding.value = checked }
                                }

                                SettingCard {
                                    Layout.fillWidth: true
                                    label: qsTr("Focus Mode Integration")
                                    description: qsTr("Use active focus profile to tune delivery policy.")
                                    highlighted: root.isHighlighted("focus-mode-integration")
                                    Switch { checked: !!root.focusBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.focusBinding.value = checked }
                                }
                            }

                            SettingCard {
                                Layout.fillWidth: true
                                label: qsTr("Deliver Quietly")
                                description: qsTr("Notifications appear silently in Pulse Feed without interrupting your workflow.")
                                highlighted: root.isHighlighted("deliver-quietly")
                                Switch { checked: !!root.quietBinding.value; enabled: !!root.enabledBinding.value; onToggled: root.quietBinding.value = checked }
                            }
                        }
                    }
                }

                Item {
                    id: appsSection
                    Layout.fillWidth: true
                    implicitHeight: appsColumn.implicitHeight

                    ColumnLayout {
                        id: appsColumn
                        width: contentCol.width
                        spacing: 18

                        SettingGroup {
                            title: qsTr("App Notifications")
                            Layout.fillWidth: true

                            SettingCard {
                                Layout.fillWidth: true
                                label: qsTr("Per-App Controls")
                                description: qsTr("Fine-tune popups, Pulse feed visibility, and priority per app.")

                                ListView {
                                    id: appList
                                    Layout.fillWidth: true
                                    implicitHeight: Math.min(320, contentHeight)
                                    clip: true
                                    spacing: 8
                                    model: NotificationSettingsController ? NotificationSettingsController.appRows : []

                                    delegate: Rectangle {
                                        width: appList.width
                                        height: 68
                                        radius: Theme.radiusMd
                                        color: Theme.color("surface")
                                        border.width: Theme.borderWidthThin
                                        border.color: Theme.color("panelBorder")

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 10

                                            Image {
                                                source: "image://icon/" + (modelData.appIcon || "applications-system")
                                                width: 22
                                                height: 22
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: modelData.appName || modelData.appId || qsTr("Unknown app")
                                                    elide: Text.ElideRight
                                                    font.weight: Theme.fontWeight("semibold")
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: modelData.lastSummary || qsTr("No recent message")
                                                    color: Theme.color("textSecondary")
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            Switch {
                                                checked: !!root.readAppPrefFor(modelData.appId || "", "allow_notifications", true)
                                                onToggled: root.writeAppPrefFor(modelData.appId || "", "allow_notifications", checked)
                                            }

                                            Button {
                                                text: qsTr("Details")
                                                onClicked: root.openAppDetail(modelData.appId || "")
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    id: historySection
                    Layout.fillWidth: true
                    implicitHeight: historyColumn.implicitHeight

                    ColumnLayout {
                        id: historyColumn
                        width: contentCol.width
                        spacing: 18

                        SettingGroup {
                            title: qsTr("History")
                            Layout.fillWidth: true

                            SettingCard {
                                Layout.fillWidth: true
                                label: qsTr("Interactive Preview")
                                description: qsTr("Send live test notifications and observe stack/group behavior.")

                                ColumnLayout {
                                    spacing: 10

                                    RowLayout {
                                        spacing: 8
                                        Button {
                                            text: qsTr("Preview Normal")
                                            onClicked: NotificationSettingsController.sendPreviewNotification("slm.preview", "Daily Brief", "Your workspace is ready.", "normal", true)
                                        }
                                        Button {
                                            text: qsTr("Preview Group Burst")
                                            onClicked: {
                                                NotificationSettingsController.sendPreviewNotification("slm.preview", "Build Complete", "Desktop shell build succeeded.", "normal", true)
                                                NotificationSettingsController.sendPreviewNotification("slm.preview", "Sync Finished", "Files synced to Pulse feed.", "normal", true)
                                            }
                                        }
                                        Button {
                                            text: qsTr("Preview Critical")
                                            onClicked: NotificationSettingsController.sendPreviewNotification("slm.system", "Battery Critical", "Connect power to avoid shutdown.", "high", true)
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        radius: Theme.radiusLg
                                        color: Theme.color("controlBg")
                                        border.width: Theme.borderWidthThin
                                        border.color: Theme.color("panelBorder")
                                        implicitHeight: 220

                                        ListView {
                                            id: previewList
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 8
                                            clip: true
                                            model: NotificationSettingsController ? NotificationSettingsController.appRows : []

                                            delegate: Rectangle {
                                                width: previewList.width
                                                height: 64
                                                radius: Theme.radiusMd
                                                color: Theme.color("surface")
                                                border.width: Theme.borderWidthThin
                                                border.color: Theme.color("panelBorder")

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    spacing: 10

                                                    Rectangle {
                                                        Layout.preferredWidth: 30
                                                        Layout.preferredHeight: 30
                                                        radius: Theme.radiusWindowAlt
                                                        color: Theme.color("controlBg")
                                                        Image { anchors.centerIn: parent; source: "image://icon/" + (modelData.appIcon || "preferences-system-notifications"); width: 18; height: 18 }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 2
                                                        Label {
                                                            Layout.fillWidth: true
                                                            text: (modelData.appName || "App") + " (" + String(modelData.count || 0) + ")"
                                                            font.weight: Theme.fontWeight("semibold")
                                                            elide: Text.ElideRight
                                                        }
                                                        Label {
                                                            Layout.fillWidth: true
                                                            text: modelData.lastSummary || qsTr("No recent notification")
                                                            color: Theme.color("textSecondary")
                                                            elide: Text.ElideRight
                                                        }
                                                    }

                                                    Label {
                                                        text: Number(modelData.unreadCount || 0) > 0 ? String(modelData.unreadCount) : ""
                                                        color: Theme.color("accent")
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    id: advancedSection
                    Layout.fillWidth: true
                    implicitHeight: advancedColumn.implicitHeight

                    ColumnLayout {
                        id: advancedColumn
                        width: contentCol.width
                        spacing: 18

                        SettingGroup {
                            title: qsTr("Advanced")
                            Layout.fillWidth: true

                            SettingCard {
                                Layout.fillWidth: true
                                label: qsTr("Banner Duration")
                                description: qsTr("Balance visibility and interruption window.")
                                highlighted: root.isHighlighted("banner-duration")
                                RowLayout {
                                    spacing: 12
                                    Slider {
                                        id: durationSlider
                                        Layout.preferredWidth: 240
                                        from: 1000
                                        to: 15000
                                        stepSize: 500
                                        value: Number(root.bannerDurationBinding.value)
                                        onMoved: root.bannerDurationBinding.value = Math.round(value)
                                    }
                                    Label {
                                        text: root.durationLabel(durationSlider.value)
                                        color: Theme.color("textSecondary")
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Button {
                                    text: qsTr("Refresh History")
                                    onClicked: NotificationSettingsController.refresh()
                                }
                                Button {
                                    text: qsTr("Clear Notification History")
                                    onClicked: NotificationSettingsController.clearHistory()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Drawer {
        id: appDetailDrawer
        edge: Qt.RightEdge
        width: Math.min(root.width * 0.45, 460)
        height: root.height
        modal: true

        background: Rectangle {
            color: Theme.color("menuBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")
        }

        Flickable {
            anchors.fill: parent
            contentHeight: detailCol.implicitHeight + 28

            ColumnLayout {
                id: detailCol
                width: parent.width
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: selectedAppId.length > 0 ? selectedAppId : qsTr("App Notification")
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("bold")
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Allow Notifications")
                    Switch {
                        checked: !!root.readAppPref("allow_notifications", true)
                        onToggled: root.writeAppPref("allow_notifications", checked)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Show Popups")
                    Switch {
                        checked: !!root.readAppPref("show_popups", true)
                        onToggled: root.writeAppPref("show_popups", checked)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Show in Pulse Feed")
                    Switch {
                        checked: !!root.readAppPref("show_in_pulse_feed", true)
                        onToggled: root.writeAppPref("show_in_pulse_feed", checked)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Sound")
                    Switch {
                        checked: !!root.readAppPref("sound", true)
                        onToggled: root.writeAppPref("sound", checked)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Badge")
                    Switch {
                        checked: !!root.readAppPref("badge", true)
                        onToggled: root.writeAppPref("badge", checked)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Priority")
                    ComboBox {
                        id: priorityCombo
                        model: ["critical", "high", "normal", "low", "silent"]
                        currentIndex: Math.max(0, model.indexOf(String(root.readAppPref("priority", "normal"))))
                        onActivated: root.writeAppPref("priority", currentText)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Grouping")
                    ComboBox {
                        model: ["auto", "always", "never"]
                        currentIndex: Math.max(0, model.indexOf(String(root.readAppPref("grouping", "auto"))))
                        onActivated: root.writeAppPref("grouping", currentText)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Lockscreen Visibility")
                    ComboBox {
                        model: ["show", "hide_content", "hide_all"]
                        currentIndex: Math.max(0, model.indexOf(String(root.readAppPref("lockscreen_visibility", "show"))))
                        onActivated: root.writeAppPref("lockscreen_visibility", currentText)
                    }
                }

                SettingCard {
                    Layout.fillWidth: true
                    label: qsTr("Mini Preview")
                    description: qsTr("Send a test notification for this app profile.")
                    Button {
                        text: qsTr("Send Preview")
                        onClicked: NotificationSettingsController.sendPreviewNotification(selectedAppId, "Preview", "Per-app profile applied.", String(root.readAppPref("priority", "normal")), true)
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (NotificationSettingsController) {
            NotificationSettingsController.refresh()
        }
        if (root.highlightSettingId.length > 0) {
            root.focusSetting(root.highlightSettingId)
        } else {
            root.scrollToSection(root.navIndex)
        }
    }
}
