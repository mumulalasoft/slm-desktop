import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

// Sidebar — Settings home page.
// Shows all modules as an icon grid grouped by category.

Rectangle {
    id: root
    color: "transparent"

    property var    moduleModel: []
    property string searchQuery: ""
    readonly property int horizontalMargin: Math.max(28, Math.min(64, width * 0.06))
    readonly property int tileMinWidth: 240

    signal moduleSelected(string id)

    function firewallPendingPromptCount() {
        if (typeof FirewallServiceClient === "undefined" || !FirewallServiceClient) {
            return 0
        }
        const pending = FirewallServiceClient.pendingPrompts
        if (!pending || typeof pending.length !== "number") {
            return 0
        }
        return Math.max(0, pending.length)
    }

    function moduleBadgeCount(moduleId) {
        const id = String(moduleId || "").trim().toLowerCase()
        if (id === "firewall") {
            return root.firewallPendingPromptCount()
        }
        return 0
    }

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    // ── Compute groups from flat module list ───────────────────────────────

    readonly property var groupedModules: {
        if (!moduleModel || moduleModel.length === 0) return []
        const groups = {}
        const order  = []
        for (let i = 0; i < moduleModel.length; i++) {
            const mod = moduleModel[i]
            // Filter by search
            if (root.searchQuery.trim().length > 0) {
                const q = root.searchQuery.trim().toLowerCase()
                const name = (mod.name || "").toLowerCase()
                const kws  = (mod.keywords || []).join(" ").toLowerCase()
                if (!name.includes(q) && !kws.includes(q)) continue
            }
            const g = mod.group || "General"
            if (!groups[g]) { groups[g] = []; order.push(g) }
            groups[g].push(mod)
        }
        return order.map(g => ({ name: g, items: groups[g] }))
    }

    // ── Layout ────────────────────────────────────────────────────────────

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.metric("spacingLg")

            Item {
                Layout.fillWidth: true
                implicitHeight: Theme.metric("spacingLg")
            }

            // Empty search state
            DSStyle.Card {
                Layout.fillWidth: true
                Layout.leftMargin: root.horizontalMargin
                Layout.rightMargin: root.horizontalMargin
                implicitHeight: 80
                visible: root.searchQuery.trim().length > 0 && root.groupedModules.length === 0
                cardColor: Theme.color("surface")
                cardBorderColor: Theme.color("panelBorder")
                elevation: "low"

                Text {
                    anchors.centerIn: parent
                    text: qsTr("No results for \"%1\"").arg(root.searchQuery)
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textSecondary")
                }
            }

            // Groups
            Repeater {
                id: groupRepeater
                model: root.groupedModules

                delegate: ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: root.horizontalMargin
                    Layout.rightMargin: root.horizontalMargin
                    spacing: Theme.metric("spacingSm")

                    // ── Group header ────────────────────────────────────────
                    Item {
                        Layout.fillWidth: true
                        height: 30

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.metric("spacingXs")
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: Theme.metric("spacingXxs")
                            text: modelData.name
                            font.pixelSize: Theme.fontSize("small")
                            font.weight: Theme.fontWeight("semibold")
                            color: Theme.color("textSecondary")
                            textFormat: Text.PlainText
                        }
                    }

                    // ── Module tiles ────────────────────────────────────────
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: tilesFlow.implicitHeight

                        Flow {
                            id: tilesFlow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            spacing: Theme.metric("spacingMd")

                            Repeater {
                                model: modelData.items

                                delegate: ItemDelegate {
                                    readonly property int columnCount: Math.max(2, Math.floor((tilesFlow.width + tilesFlow.spacing) / (root.tileMinWidth + tilesFlow.spacing)))
                                    width: Math.floor((tilesFlow.width - (tilesFlow.spacing * (columnCount - 1))) / columnCount)
                                    height: 86
                                    padding: 0
                                    readonly property int moduleBadge: root.moduleBadgeCount(modelData.id)

                                    background: Rectangle {
                                        radius: Theme.radiusCard
                                        color: parent.down
                                            ? Theme.color("controlBgPressed")
                                            : (parent.hovered ? Theme.color("controlBgHover") : Theme.color("surface"))
                                        border.color: parent.hovered ? Theme.color("panelBorderStrong") : Theme.color("panelBorder")
                                        border.width: Theme.borderWidthThin
                                        Behavior on color {
                                            enabled: root.microAnimationAllowed()
                                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
                                        }
                                    }

                                    contentItem: RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.metric("spacingLg")
                                        anchors.rightMargin: Theme.metric("spacingLg")
                                        spacing: Theme.metric("spacingLg")

                                        // Module icon
                                        Rectangle {
                                            Layout.preferredWidth: 46
                                            Layout.preferredHeight: 46
                                            radius: Theme.radiusControlLarge
                                            color: Theme.color("controlBg")
                                            Layout.alignment: Qt.AlignVCenter

                                            Image {
                                                anchors.centerIn: parent
                                                source: "image://icon/" + (modelData.icon || "preferences-system")
                                                width: 30
                                                height: 30
                                                smooth: true
                                            }
                                        }

                                        // Module name
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignVCenter
                                            spacing: Theme.metric("spacingXxs")

                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name || ""
                                                font.pixelSize: Theme.fontSize("body")
                                                font.weight: Theme.fontWeight("semibold")
                                                color: Theme.color("textPrimary")
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.keywords || []).slice(0, 3).join("  ·  ")
                                                visible: text.length > 0
                                                font.pixelSize: Theme.fontSize("xs")
                                                color: Theme.color("textSecondary")
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Rectangle {
                                            visible: moduleBadge > 0
                                            Layout.alignment: Qt.AlignVCenter
                                            implicitHeight: 22
                                            implicitWidth: Math.max(22, badgeText.implicitWidth + 10)
                                            radius: Theme.radiusLg
                                            color: Theme.color("accent")
                                            border.width: Theme.borderWidthThin
                                            border.color: Qt.rgba(0, 0, 0, 0.14)

                                            Text {
                                                id: badgeText
                                                anchors.centerIn: parent
                                                text: moduleBadge > 99 ? "99+" : String(moduleBadge)
                                                font.pixelSize: Theme.fontSize("xs")
                                                font.weight: Theme.fontWeight("semibold")
                                                color: Theme.color("onAccent")
                                            }
                                        }
                                    }

                                    onClicked: root.moduleSelected(modelData.id || "")

                                    // Subtle press scale
                                    scale: pressed ? 0.98 : 1.0
                                    Behavior on scale {
                                        enabled: root.microAnimationAllowed()
                                        NumberAnimation { duration: Theme.durationMicro; easing.type: Theme.easingDefault }
                                    }
                                }
                            }
                        }
                    }

                    // Section divider (not after last group)
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
                        visible: index < root.groupedModules.length - 1
                    }
                }
            }

            // Bottom padding
            Item { Layout.fillWidth: true; height: 28 }
        }
    }
}
