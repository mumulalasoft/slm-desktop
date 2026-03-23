import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    property var notificationManager: NotificationManager
    property bool showing: false
    property int notificationId: -1
    property string summaryText: ""
    property string bodyText: ""

    width: Math.min(380, parent ? parent.width : 380)
    height: bubbleRect.implicitHeight
    visible: showing || opacity > 0.01
    opacity: showing ? 1 : 0
    y: showing ? 0 : -Theme.metric("spacingMd")
    scale: showing ? 1.0 : 0.97

    function applyLatest() {
        if (!root.notificationManager || !root.notificationManager.latestNotification) {
            return
        }
        var data = root.notificationManager.latestNotification
        var summary = (data.summary || "").toString()
        var body = (data.body || "").toString()
        if (summary.length === 0 && body.length === 0) {
            return
        }
        root.notificationId = Number(data.id || -1)
        root.summaryText = summary.length > 0 ? summary : (data.appName || "Notification")
        root.bodyText = body
        root.showing = true
        hideTimer.interval = root.notificationManager.bubbleDurationMs > 0
                             ? root.notificationManager.bubbleDurationMs : 5000
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: 5000
        repeat: false
        onTriggered: root.showing = false
    }

    Rectangle {
        id: bubbleRect
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: contentLayout.implicitHeight + (Theme.metric("spacingLg") + Theme.metric("spacingSm"))
        radius: Theme.radiusPopup
        color: Theme.color("menuBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("menuBorder")

        // Drop shadow — manual glow Rectangle (no Qt5Compat.GraphicalEffects needed).
        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.shadowMd.radius * 0.3
            radius: parent.radius + Theme.shadowMd.radius * 0.3
            color: "transparent"
            border.width: Theme.shadowMd.radius
            border.color: Qt.rgba(0, 0, 0, Theme.shadowMd.opacity * (Theme.darkMode ? 1.6 : 1.0))
            z: -1
            opacity: 0.45
            scale: 1.0
        }

        ColumnLayout {
            id: contentLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.metric("spacingMd")
            spacing: Theme.metric("spacingXxs")

            Text {
                Layout.fillWidth: true
                text: root.summaryText
                color: Theme.color("textPrimary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("smallPlus")
                font.weight: Theme.fontWeight("bold")
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.bodyText
                color: Theme.color("textSecondary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: Theme.lineHeight("normal")
                maximumLineCount: 3
                elide: Text.ElideRight
                visible: text.length > 0
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.showing = false
            if (root.notificationManager && root.notificationId > 0) {
                root.notificationManager.closeById(root.notificationId)
            }
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: showing ? Theme.durationMd : Theme.durationSm
            easing.type: showing ? Theme.easingDecelerate : Theme.easingAccelerate
        }
    }

    Behavior on y {
        NumberAnimation {
            duration: showing ? Theme.durationLg : Theme.durationSm
            easing.type: showing ? Theme.easingSpring : Theme.easingAccelerate
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: showing ? Theme.durationMd : Theme.durationSm
            easing.type: showing ? Theme.easingDecelerate : Theme.easingAccelerate
        }
    }

    Connections {
        target: root.notificationManager
        function onLatestNotificationChanged() {
            root.applyLatest()
        }
    }
}
