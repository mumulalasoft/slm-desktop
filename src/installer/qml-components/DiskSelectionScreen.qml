/*
 * DiskSelectionScreen — §2.3 Disk Selection. The single most consequential
 * decision in the installer.
 *
 * Composition:
 *   ┌─ windowBg fill ─────────────────────────────────────────────────┐
 *   │                                                                 │
 *   │           ┌─ InstallerCard ───────────────────────────┐         │
 *   │           │  Headline + body                          │         │
 *   │           │  ─────────────────────────────────────    │         │
 *   │           │  ListView of InstallerDiskCard, OR        │         │
 *   │           │  empty-state error block                  │         │
 *   │           │  ─────────────────────────────────────    │         │
 *   │           │  Continue (InstallerPrimaryButton)        │         │
 *   │           └───────────────────────────────────────────┘         │
 *   └─────────────────────────────────────────────────────────────────┘
 *
 * Public API (the consuming viewmodule wires these):
 *   model              — list-like model of {path, name, size, kind, health,
 *                        rootBytes, ...}. Each row maps 1:1 to an
 *                        InstallerDiskCard.
 *   selectedPath       — read/write; the user's chosen device path.
 *                        Empty when nothing selected.
 *   isRefreshing       — bind to true while the viewmodule re-enumerates
 *                        disks; the empty-state Refresh button reflects it.
 *
 * Signals:
 *   continueRequested(string path)
 *   refreshRequested()
 *   quitRequested()
 *
 * The viewmodule writes slm.target.disk to GlobalStorage on
 * continueRequested(). slm.target.confirmed is written by the Summary
 * screen, not here.
 */
import QtQuick
import QtQuick.Layouts
import "."

Rectangle {
    id: root

    property var model: null
    property string selectedPath: ""
    property bool isRefreshing: false

    signal continueRequested(string path)
    signal refreshRequested()
    signal quitRequested()

    color: InstallerTheme.windowBg

    readonly property int diskCount: model ? model.count : 0
    readonly property bool hasDisks: diskCount > 0

    // Single-disk auto-select: when exactly one disk is present and nothing
    // is selected yet, wait 300ms then select it. The delay preserves the
    // partition-diagram entrance animation and gives screen readers time
    // to announce the disk details before "selected" arrives. Agent edge
    // case from the design review.
    Timer {
        id: singleDiskTimer
        interval: 300
        repeat: false
        onTriggered: {
            if (root.diskCount === 1 && root.selectedPath === ""
                && root.model && root.model.get) {
                const row = root.model.get(0)
                if (row && row.health !== "failed") {
                    root.selectedPath = row.path
                }
            }
        }
    }

    Connections {
        target: root
        function onDiskCountChanged() {
            if (root.diskCount === 1 && root.selectedPath === "") {
                singleDiskTimer.restart()
            }
        }
    }

    Component.onCompleted: {
        if (diskCount === 1 && selectedPath === "") {
            singleDiskTimer.restart()
        }
    }

    InstallerCard {
        id: card
        anchors {
            top: parent.top
            topMargin: 56
            horizontalCenter: parent.horizontalCenter
        }

        headline: qsTr("Choose where to install SLM Desktop")
        body: root.hasDisks
              ? qsTr("Select a disk. SLM will create the partitions it needs and prepare a recovery partition you can return to anytime.")
              : ""

        // Disk list — visible only when there are disks.
        Column {
            id: diskList
            visible: root.hasDisks
            width: parent.width
            spacing: 12

            Repeater {
                model: root.model
                delegate: InstallerDiskCard {
                    width: diskList.width
                    diskName:      model.name
                    diskSize:      model.size
                    diskPath:      model.path
                    diskKind:      model.kind
                    health:        model.health
                    rootBytes:     model.rootBytes
                    efiBytes:      model.efiBytes !== undefined
                                   ? model.efiBytes : 1073741824
                    recoveryBytes: model.recoveryBytes !== undefined
                                   ? model.recoveryBytes : 8589934592
                    existingEsp:   model.hasExistingEsp === true
                    selected: model.path === root.selectedPath
                    onClicked: root.selectedPath = model.path
                }
            }
        }

        // Empty state — §5 blocking-error pattern.
        Item {
            visible: !root.hasDisks
            width: parent.width
            implicitHeight: emptyCol.implicitHeight

            Column {
                id: emptyCol
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width
                    text: qsTr("No disks found")
                    font.pixelSize: InstallerTheme.fontPxSubtitle
                    font.weight: InstallerTheme.weightSemiBold
                    font.family: InstallerTheme.fontFamilyUi
                    color: InstallerTheme.textPrimary
                }
                Text {
                    width: parent.width
                    text: qsTr("SLM couldn't detect any storage on this computer. Connect a disk and click Refresh, or check that the disk is recognized in your firmware settings.")
                    font.pixelSize: InstallerTheme.fontPxBody
                    font.family: InstallerTheme.fontFamilyUi
                    color: InstallerTheme.textSecondary
                    wrapMode: Text.WordWrap
                }
                Item { width: 1; height: 8 }
                Row {
                    spacing: 12
                    InstallerPrimaryButton {
                        label: qsTr("Refresh")
                        loading: root.isRefreshing
                        onClicked: root.refreshRequested()
                    }
                    // Secondary Quit lives as a flat text button. Inlined
                    // here rather than promoted to a component because v1
                    // surfaces no other "ghost" buttons.
                    Item {
                        width: quitLabel.implicitWidth + 24
                        height: 40
                        Text {
                            id: quitLabel
                            anchors.centerIn: parent
                            text: qsTr("Quit installer")
                            font.pixelSize: InstallerTheme.fontPxBody
                            font.weight: InstallerTheme.weightMedium
                            font.family: InstallerTheme.fontFamilyUi
                            color: InstallerTheme.textSecondary
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: root.quitRequested() }
                    }
                }
            }
        }

        footer: Item {
            width: parent.width
            height: continueButton.height
            visible: root.hasDisks

            // Right-anchored primary action.
            InstallerPrimaryButton {
                id: continueButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Continue")
                enabled: root.selectedPath !== ""
                onClicked: root.continueRequested(root.selectedPath)
            }
        }
    }
}
