import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

// EnvVarRow — a single environment variable row.
// Expects the following properties from the delegate context:
//   model.key, model.value, model.enabled, model.sensitive,
//   model.severity, model.modifiedAt, model.comment
Item {
    id: root

    implicitHeight: 44
    implicitWidth: parent ? parent.width : 600

    required property string  entryKey
    required property string  entryValue
    required property bool    entryEnabled
    required property bool    entrySensitive
    required property string  entrySeverity
    required property string  entryModifiedAt
    required property string  entryComment

    signal editRequested()
    signal deleteRequested()

    // Subtle amber left-border for sensitive keys
    Rectangle {
        width: 2
        height: parent.height
        color: {
            if (!root.entrySensitive)             return "transparent"
            if (root.entrySeverity === "critical") return Theme.color("destructive")
            return "#E8A000"
        }
    }

    // Hover background
    Rectangle {
        anchors.fill: parent
        color: hoverHandler.hovered ? Theme.color("hoverOverlay") : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    HoverHandler { id: hoverHandler }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 12
        spacing: 8

        // Enable toggle
        SettingToggle {
            id: enableToggle
            checked: root.entryEnabled
            Layout.preferredWidth: 38
            Layout.alignment: Qt.AlignVCenter
            onToggled: EnvVarController.setEnabled(root.entryKey, checked)
        }

        // Key
        Text {
            text: root.entryKey
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSize("sm")
            font.weight: Font.Medium
            color: root.entryEnabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
            elide: Text.ElideRight
            Layout.preferredWidth: 200
            Layout.alignment: Qt.AlignVCenter
        }

        // Value
        Text {
            text: valueMasked ? "••••••••" : root.entryValue
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSize("sm")
            color: root.entryEnabled ? Theme.color("textSecondary") : Theme.color("textDisabled")
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            property bool valueMasked: {
                const k = root.entryKey
                return k.includes("KEY") || k.includes("SECRET")
                    || k.includes("TOKEN") || k.includes("PASSWORD")
                    || k.includes("PASSWD")
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: parent.valueMasked ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (parent.valueMasked) parent.valueMasked = false
            }
        }

        // Scope badge — always "User" in Phase 1
        Rectangle {
            radius: 4
            implicitWidth: scopeLabel.implicitWidth + 10
            implicitHeight: 20
            color: Qt.rgba(0.18, 0.6, 0.3, 0.15)
            Layout.alignment: Qt.AlignVCenter

            Text {
                id: scopeLabel
                anchors.centerIn: parent
                text: qsTr("User")
                font.pixelSize: Theme.fontSize("xs")
                font.weight: Font.DemiBold
                font.letterSpacing: 0.4
                color: "#2E9950"
            }
        }

        // Modified time
        Text {
            text: root.entryModifiedAt !== "" ? formatRelative(root.entryModifiedAt) : ""
            font.pixelSize: Theme.fontSize("xs")
            color: Theme.color("textDisabled")
            Layout.preferredWidth: 70
            Layout.alignment: Qt.AlignVCenter
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight

            function formatRelative(iso) {
                const d = new Date(iso)
                const now = new Date()
                const diffMs = now - d
                const diffDays = Math.floor(diffMs / 86400000)
                if (diffDays < 1)   return qsTr("today")
                if (diffDays === 1) return qsTr("1d ago")
                if (diffDays < 30)  return qsTr("%1d ago").arg(diffDays)
                const diffMo = Math.floor(diffDays / 30)
                return qsTr("%1mo ago").arg(diffMo)
            }
        }

        // Action buttons — visible on hover or keyboard focus
        Row {
            spacing: 0
            Layout.alignment: Qt.AlignVCenter
            visible: hoverHandler.hovered || editBtn.activeFocus || deleteBtn.activeFocus

            ToolButton {
                id: editBtn
                icon.name: "document-edit"
                icon.width: 16; icon.height: 16
                implicitWidth: 30; implicitHeight: 30
                padding: 0
                ToolTip.text: qsTr("Edit")
                ToolTip.visible: hovered
                ToolTip.delay: 600
                onClicked: root.editRequested()
                background: Rectangle {
                    radius: 6
                    color: editBtn.hovered ? Theme.color("hoverOverlay") : "transparent"
                }
            }

            ToolButton {
                id: deleteBtn
                icon.name: "edit-delete"
                icon.width: 16; icon.height: 16
                implicitWidth: 30; implicitHeight: 30
                padding: 0
                ToolTip.text: qsTr("Delete")
                ToolTip.visible: hovered
                ToolTip.delay: 600
                onClicked: root.deleteRequested()
                background: Rectangle {
                    radius: 6
                    color: deleteBtn.hovered ? Qt.rgba(1, 0.2, 0.2, 0.15) : "transparent"
                }
            }
        }
    }

    // Bottom separator
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        height: 1
        color: Theme.color("panelBorder")
        visible: {
            if (!root.parent) return false
            for (var i = root.parent.children.length - 1; i >= 0; i--) {
                const c = root.parent.children[i]
                if (c === root) return false
                if (c.visible) return true
            }
            return false
        }
    }
}
