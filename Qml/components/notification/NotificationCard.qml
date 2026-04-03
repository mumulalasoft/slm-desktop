import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Effects
import Slm_Desktop

Rectangle {
    id: root

    property string appName: ""
    property string appIcon: ""
    property int notificationId: -1
    property string title: ""
    property string body: ""
    property string timestamp: ""
    property string priority: "normal"
    property bool compact: false
    property bool read: false
    property var actionsModel: []

    signal actionTriggered(string actionKey)
    signal dismissRequested()
    signal clicked()

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    readonly property bool _isHigh: String(priority || "").toLowerCase() === "high"
    readonly property bool _hasPairedActions: !!actionsModel
                                              && actionsModel.length >= 2
                                              && (actionsModel.length % 2 === 0)
    readonly property int _visibleActionCount: {
        if (!actionsModel || actionsModel.length <= 0) {
            return 0
        }
        var total = _hasPairedActions ? Math.floor(actionsModel.length / 2) : actionsModel.length
        return Math.min(total, 2)
    }
    readonly property string _timestampLabel: {
        if (!timestamp || String(timestamp).length <= 0) {
            return ""
        }
        var d = new Date(timestamp)
        if (isNaN(d.getTime())) {
            return String(timestamp)
        }
        var now = new Date()
        var sameDay = d.getFullYear() === now.getFullYear()
                && d.getMonth() === now.getMonth()
                && d.getDate() === now.getDate()
        if (sameDay) {
            return Qt.formatTime(d, "hh:mm")
        }
        return Qt.formatDateTime(d, "dd MMM hh:mm")
    }

    radius: Theme.notificationCardRadius
    color: Theme.notificationCardBackground
    border.width: _isHigh ? Theme.borderWidthThick : Theme.borderWidthThin
    border.color: _isHigh ? Theme.color("accent") : Theme.color("panelBorder")
    opacity: root.read ? 0.72 : 1.0

    implicitWidth: compact ? 340 : 380
    implicitHeight: contentRow.implicitHeight + (Theme.notificationCardPadding * 2)

    layer.enabled: !compact
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.22 : Theme.elevationMedium.opacity)
        shadowBlur: 0.45
        shadowVerticalOffset: 3
        shadowHorizontalOffset: 0
    }

    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.margins: Theme.notificationCardPadding
        spacing: Theme.notificationVerticalRhythm

        // App icon
        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 2
            width: 28
            height: 28
            radius: Theme.radiusControl
            color: Theme.color("controlBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            Image {
                anchors.fill: parent
                anchors.margins: 5
                source: root.appIcon
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: source.length > 0
            }
        }

        // Content column
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.notificationVerticalRhythm

            // Title row with inline timestamp + dismiss
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: root.title.length > 0 ? root.title : root.appName
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Theme.fontWeight("semiBold")
                    elide: Text.ElideRight
                }

                Text {
                    text: root._timestampLabel
                    visible: text.length > 0 && compact
                    color: Theme.color("textMuted")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("micro")
                }

                // Dismiss button (center mode only)
                Item {
                    id: dismissBtn
                    width: 18
                    height: 18
                    visible: !compact

                    Text {
                        anchors.centerIn: parent
                        text: "\u2715"
                        color: dismissHover.containsMouse ? Theme.color("textPrimary") : Theme.color("textMuted")
                        font.pixelSize: Theme.fontSize("tiny")
                        Behavior on color {
                            enabled: root.microAnimationAllowed()
                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
                        }
                    }

                    HoverHandler { id: dismissHover }

                    TapHandler {
                        onTapped: root.dismissRequested()
                    }
                }
            }

            // Body text
            Text {
                Layout.fillWidth: true
                text: root.body
                color: Theme.color("textSecondary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                maximumLineCount: compact ? 2 : 3
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }

            // Compact action chips (max 3)
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.notificationVerticalRhythm
                visible: root._visibleActionCount > 0

                Repeater {
                    model: root._visibleActionCount
                    delegate: Rectangle {
                        id: actionChip
                        required property int index
                        readonly property int rawIndex: root._hasPairedActions ? (index * 2) : index
                        readonly property string actionKey: root.actionsModel && rawIndex < root.actionsModel.length
                            ? String(root.actionsModel[rawIndex] || "") : ""
                        readonly property string actionLabel: {
                            if (!root.actionsModel) {
                                return ""
                            }
                            if (root._hasPairedActions && (rawIndex + 1) < root.actionsModel.length) {
                                return String(root.actionsModel[rawIndex + 1] || actionKey)
                            }
                            return actionKey
                        }
                        readonly property bool primary: index === 0

                        height: 26
                        implicitWidth: Math.max(62, Math.min(chipLabel.implicitWidth + 22, 132))
                        radius: Theme.radiusControl
                        color: primary
                               ? (chipHover.containsMouse ? Theme.color("accentHover") : Theme.color("accent"))
                               : (chipHover.containsMouse ? Theme.color("controlBgHover") : Theme.color("controlBg"))
                        border.width: Theme.borderWidthThin
                        border.color: primary ? Theme.color("accent") : Theme.color("panelBorder")

                        Behavior on color {
                            enabled: root.microAnimationAllowed()
                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                        }

                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            width: parent.width - 18
                            text: actionChip.actionLabel
                            color: actionChip.primary ? Theme.color("accentText") : Theme.color("textPrimary")
                            font.family: Theme.fontFamilyUi
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Theme.fontWeight("medium")
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            horizontalAlignment: Text.AlignHCenter
                        }

                        HoverHandler { id: chipHover }

                        TapHandler {
                            onTapped: root.actionTriggered(actionChip.actionKey)
                        }
                    }
                }
            }

            // Timestamp footer (center mode)
            Text {
                Layout.fillWidth: true
                text: root._timestampLabel
                visible: text.length > 0 && !compact
                color: Theme.color("textMuted")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("micro")
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
    }

    TapHandler {
        onTapped: root.clicked()
    }
}
