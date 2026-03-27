import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

// Sidebar — Settings home page.
// Shows all modules as an icon grid grouped by category.

Rectangle {
    id: root
    color: Theme.color("windowBg")

    property var    moduleModel: []
    property string searchQuery: ""

    signal moduleSelected(string id)

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
            spacing: 0

            // Empty search state
            Item {
                Layout.fillWidth: true
                implicitHeight: 80
                visible: root.searchQuery.trim().length > 0 && root.groupedModules.length === 0

                Text {
                    anchors.centerIn: parent
                    text: qsTr("No results for \"%1\"").arg(root.searchQuery)
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                }
            }

            // Groups
            Repeater {
                id: groupRepeater
                model: root.groupedModules

                delegate: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    // ── Group header ────────────────────────────────────────
                    Item {
                        Layout.fillWidth: true
                        height: 38

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 28
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 6
                            text: modelData.name
                            font.pixelSize: Theme.fontSize("xs")
                            font.weight: Font.DemiBold
                            color: Theme.color("textSecondary")
                            textFormat: Text.PlainText
                        }
                    }

                    // ── Module tiles ────────────────────────────────────────
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: tilesFlow.implicitHeight + 16

                        Flow {
                            id: tilesFlow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            spacing: 10

                            Repeater {
                                model: modelData.items

                                delegate: ItemDelegate {
                                    width: Math.floor((tilesFlow.width - 20) / 3)
                                    height: 64
                                    padding: 0

                                    background: Rectangle {
                                        radius: Theme.radiusCard || 8
                                        color: parent.hovered
                                            ? Theme.color("controlBgHover")
                                            : Theme.color("surface")
                                        border.color: Theme.color("panelBorder")
                                        border.width: 1
                                        Behavior on color {
                                            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    contentItem: RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 14
                                        anchors.rightMargin: 10
                                        spacing: 12

                                        // Module icon
                                        Image {
                                            source: "image://icon/" + (modelData.icon || "preferences-system")
                                            Layout.preferredWidth: 36
                                            Layout.preferredHeight: 36
                                            smooth: true
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        // Module name
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name || ""
                                            font.pixelSize: Theme.fontSize("sm")
                                            font.weight: Font.Medium
                                            color: Theme.color("textPrimary")
                                            wrapMode: Text.WordWrap
                                            elide: Text.ElideRight
                                            maximumLineCount: 2
                                            Layout.alignment: Qt.AlignVCenter
                                        }
                                    }

                                    onClicked: root.moduleSelected(modelData.id || "")

                                    // Subtle press scale
                                    scale: pressed ? 0.97 : 1.0
                                    Behavior on scale {
                                        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
                                    }
                                }
                            }
                        }
                    }

                    // Section divider (not after last group)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        height: 1
                        color: Theme.color("panelBorder")
                        visible: index < root.groupedModules.length - 1
                    }
                }
            }

            // Bottom padding
            Item { Layout.fillWidth: true; height: 28 }
        }
    }
}
