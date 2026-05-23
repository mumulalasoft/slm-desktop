/*
 * Harness scene for §2.3 DiskSelectionScreen.
 *
 * Three sample disks exercise the major visual branches:
 *   - nvme0n1: NVMe, healthy, has existing ESP → ESP-reuse advisory
 *   - sda:     SATA SSD, warning SMART → caution badge
 *   - sdb:     external USB, FAILED SMART → SMART-FAILED block path
 */
import QtQuick
import SlmInstaller 1.0

Rectangle {
    width: 960
    height: 720
    color: InstallerTheme.windowBg

    ListModel {
        id: mockDisks
        ListElement {
            path: "/dev/nvme0n1"
            name: "Samsung 970 EVO 1TB — 1 TB"
            size: "1 TB"
            kind: "NVMe"
            health: "healthy"
            rootBytes: 991238877184
            efiBytes: 1073741824
            recoveryBytes: 8589934592
            removable: false
            hasExistingEsp: true
        }
        ListElement {
            path: "/dev/sda"
            name: "Samsung SSD 870 — 500 GB"
            size: "500 GB"
            kind: "SATA SSD"
            health: "warning"
            rootBytes: 490528632832
            efiBytes: 1073741824
            recoveryBytes: 8589934592
            removable: false
            hasExistingEsp: false
        }
        ListElement {
            path: "/dev/sdb"
            name: "Generic External — 2 TB"
            size: "2 TB"
            kind: "USB"
            health: "failed"
            rootBytes: 1991238877184
            efiBytes: 1073741824
            recoveryBytes: 8589934592
            removable: true
            hasExistingEsp: false
        }
    }

    DiskSelectionScreen {
        anchors.fill: parent
        model: mockDisks
        isRefreshing: false

        onContinueRequested: function (path) {
            console.log("[harness] continueRequested:", path)
        }
        onRefreshRequested: console.log("[harness] refreshRequested")
        onQuitRequested: Qt.quit()
    }
}
