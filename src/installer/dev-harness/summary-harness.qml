/*
 * Harness scene for §2.5 SummaryScreen.
 *
 * Sample data covers the realistic case: one disk selected (the kind a
 * mid-pipeline user would see), a multi-row Btrfs subvolume list for the
 * Advanced disclosure, and two warning codes wired to exercise the
 * amber-banner translation pipeline.
 */
import QtQuick
import SlmInstaller 1.0

SummaryScreen {
    width: 960
    height: 720

    diskName: "Samsung 970 EVO 1TB — 1 TB"
    diskPath: "/dev/nvme0n1"
    espSizeMb: 1024
    recoverySizeMb: 8192
    subvolumes: [
        "@",
        "@home",
        "@var",
        "@log",
        "@cache",
        "@snapshots",
        "@resources",
        "@recovery-staging"
    ]
    factorySnapshotEnabled: true
    warnings: [ "UEFI_W001", "HW_W001" ]
    language: qsTr("English (United States)")
    timezone: qsTr("Auto-detected from network (can be changed after install)")

    onInstallRequested: {
        console.log("[harness] installRequested")
        Qt.quit()
    }
    onBackRequested: {
        console.log("[harness] backRequested")
        Qt.quit()
    }
}
