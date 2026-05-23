/*
 * SummaryScreen — §2.5 Summary & Confirmation. The final checkpoint
 * before the installer commits anything destructive.
 *
 * Two-column label/value summary, an optional warning banner if Screen 2
 * recorded advisories, and an "Advanced technical details" disclosure for
 * curious users. The primary action reads "Install SLM Desktop" — not
 * "Next" — because §2.5 mandates that the verb name the irreversible
 * outcome. A textTertiary subline reinforces the destructive nature.
 *
 * Public API (viewmodule binds these from GlobalStorage):
 *   diskName, diskPath              — slm.target.{disk_name,disk}
 *   espSizeMb, recoverySizeMb       — slm.target.{esp,recovery}_size_mb
 *   subvolumes                      — slm.btrfs.subvolumes
 *   factorySnapshotEnabled          — slm.snapshot.enabled / fallback true
 *   warnings                        — slm.hw.warnings (code list)
 *   language, timezone              — locale from welcome / OOBE handoff
 *
 * Signals:
 *   installRequested()              — viewmodule writes slm.target.confirmed
 *                                     and advances
 *   backRequested()                 — viewmodule pops back to disk-select
 */
import QtQuick
import QtQuick.Layouts
import "."

Rectangle {
    id: root

    // ── Public API ─────────────────────────────────────────────────────────
    property string diskName: ""
    property string diskPath: ""
    property int    espSizeMb: 1024
    property int    recoverySizeMb: 8192
    property var    subvolumes: []
    property bool   factorySnapshotEnabled: true
    property var    warnings: []
    property string language: qsTr("English (United States)")
    property string timezone: qsTr("Auto-detected from network (can be changed after install)")

    signal installRequested()
    signal backRequested()

    color: InstallerTheme.windowBg

    // Translation table for §9 warning codes → user-facing copy. Lives in
    // QML so localisation rides the qsTr() pipeline.
    function _warningMessage(code) {
        switch (code) {
        case "HW_W001":   return qsTr("Graphics driver may need additional configuration after install.")
        case "HW_W002":   return qsTr("This computer has a Broadcom wireless chip that may require an additional driver.")
        case "HW_W003":   return qsTr("Selected disk reported a SMART warning. Consider replacing it soon.")
        case "UEFI_W001": return qsTr("Secure Boot is enabled. SLM will install with an unsigned bootloader.")
        case "UEFI_W002": return qsTr("Memory is below the recommended 2 GB.")
        default:          return code
        }
    }

    InstallerCard {
        id: card
        anchors {
            top: parent.top
            topMargin: 56
            horizontalCenter: parent.horizontalCenter
        }

        headline: qsTr("Confirm installation")
        body: qsTr("SLM will set up your computer with the choices below. You can still go back.")

        // ── Summary table (label : value) ─────────────────────────────────
        ColumnLayout {
            width: parent.width
            spacing: 0

            // Row delegate — kept inline since it doesn't earn a separate
            // component file at v1. If a third screen needs a label/value
            // table, promote to qml-components/.
            component SummaryRow: RowLayout {
                property string label: ""
                property string value: ""
                Layout.fillWidth: true
                spacing: 16

                Text {
                    text: parent.label
                    font.pixelSize: InstallerTheme.fontPxBody
                    font.weight: InstallerTheme.weightMedium
                    font.family: InstallerTheme.fontFamilyUi
                    color: InstallerTheme.textSecondary
                    Layout.preferredWidth: 140
                    Layout.alignment: Qt.AlignTop
                }
                Text {
                    text: parent.value
                    font.pixelSize: InstallerTheme.fontPxBody
                    font.family: InstallerTheme.fontFamilyUi
                    color: InstallerTheme.textPrimary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            SummaryRow {
                label: qsTr("Disk")
                value: root.diskName.length > 0
                       ? root.diskName + "  (" + root.diskPath + ")"
                       : qsTr("(no disk selected)")
            }
            Item { Layout.fillWidth: true; height: 10 }

            SummaryRow {
                label: qsTr("Filesystem")
                value: qsTr("Modern (snapshot-enabled)")
            }
            Item { Layout.fillWidth: true; height: 10 }

            SummaryRow {
                label: qsTr("Recovery")
                value: qsTr("Included (%1 GB)").arg(Math.round(root.recoverySizeMb / 1024))
            }
            Item { Layout.fillWidth: true; height: 10 }

            SummaryRow {
                label: qsTr("Factory snapshot")
                value: root.factorySnapshotEnabled ? qsTr("Enabled") : qsTr("Skipped")
            }
            Item { Layout.fillWidth: true; height: 10 }

            SummaryRow {
                label: qsTr("Language")
                value: root.language
            }
            Item { Layout.fillWidth: true; height: 10 }

            SummaryRow {
                label: qsTr("Timezone")
                value: root.timezone
            }
        }

        // ── Warning banner ────────────────────────────────────────────────
        Item {
            visible: root.warnings.length > 0
            width: parent.width
            implicitHeight: warningCol.implicitHeight + 16

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 16
                radius: InstallerTheme.radiusControl
                color: InstallerTheme.surface

                // Amber left rule — §5 inline-error grammar in advisory mode.
                Rectangle {
                    width: 3
                    radius: width / 2
                    color: InstallerTheme.warning
                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        left: parent.left
                        topMargin: 12
                        bottomMargin: 12
                        leftMargin: 0
                    }
                }

                ColumnLayout {
                    id: warningCol
                    anchors {
                        fill: parent
                        leftMargin: 16
                        rightMargin: 16
                        topMargin: 12
                        bottomMargin: 12
                    }
                    spacing: 4

                    Text {
                        text: qsTr("Advisories from compatibility check")
                        font.pixelSize: InstallerTheme.fontPxSm
                        font.weight: InstallerTheme.weightSemiBold
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textPrimary
                        Layout.fillWidth: true
                    }
                    Repeater {
                        model: root.warnings
                        delegate: Text {
                            required property string modelData
                            text: "• " + root._warningMessage(modelData)
                            font.pixelSize: InstallerTheme.fontPxSm
                            font.family: InstallerTheme.fontFamilyUi
                            color: InstallerTheme.textSecondary
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // ── Advanced disclosure ──────────────────────────────────────────
        Item {
            id: advanced
            width: parent.width
            implicitHeight: discloseRow.implicitHeight
                            + (advanced._expanded ? advancedBody.implicitHeight + 12 : 0)
            clip: true

            property bool _expanded: false

            Behavior on implicitHeight {
                NumberAnimation {
                    duration: InstallerTheme.durationFast
                    easing.type: InstallerTheme.easingDecelerate
                }
            }

            Column {
                width: parent.width
                spacing: 0
                topPadding: 16

                Row {
                    id: discloseRow
                    spacing: 6
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: advanced._expanded = !advanced._expanded }

                    Text {
                        text: advanced._expanded ? "▾" : "▸"
                        font.pixelSize: InstallerTheme.fontPxXs
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textTertiary
                    }
                    Text {
                        text: qsTr("Advanced technical details")
                        font.pixelSize: InstallerTheme.fontPxXs
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textTertiary
                    }
                }

                Column {
                    id: advancedBody
                    width: parent.width
                    visible: advanced._expanded
                    topPadding: 8
                    spacing: 2

                    Text {
                        text: qsTr("Filesystem: Btrfs with %1 subvolumes").arg(root.subvolumes.length)
                        font.pixelSize: InstallerTheme.fontPxXs
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textTertiary
                    }
                    Text {
                        text: qsTr("Bootloader: systemd-boot")
                        font.pixelSize: InstallerTheme.fontPxXs
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textTertiary
                    }
                    Text {
                        text: qsTr("EFI partition: %1 MB").arg(root.espSizeMb)
                        font.pixelSize: InstallerTheme.fontPxXs
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textTertiary
                    }
                    Text {
                        visible: root.subvolumes.length > 0
                        text: qsTr("Subvolumes: %1").arg(root.subvolumes.join(", "))
                        font.pixelSize: InstallerTheme.fontPxXs
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textTertiary
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }
        }

        // ── Footer: Install action + erasure subline ─────────────────────
        footer: Column {
            width: parent.width
            spacing: 6

            InstallerPrimaryButton {
                anchors.right: parent.right
                label: qsTr("Install SLM Desktop")
                enabled: root.diskPath.length > 0
                onClicked: root.installRequested()
            }

            Text {
                anchors.right: parent.right
                text: qsTr("This will erase everything on the selected disk.")
                font.pixelSize: InstallerTheme.fontPxXs
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textTertiary
            }
        }
    }
}
