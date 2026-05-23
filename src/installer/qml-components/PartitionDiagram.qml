/*
 * PartitionDiagram — animated 3-segment bar showing the post-install
 * layout (EFI / SLM Desktop / Recovery) of the selected disk.
 *
 * §2.3 makes this "the one intentional motion in the installer." The
 * segments grow left-to-right when `animate` becomes true, with a 40ms
 * stagger between EFI → Root → Recovery so the bar reads as an
 * assembly, not three independent animations.
 *
 * Proportions are floored to a minimum (8% EFI, 12% Recovery) so labels
 * stay legible on disks near the lower bound (~40 GB). The cost is that
 * the bar is approximate-to-scale on small disks; design plan §2.3
 * accepts this trade-off.
 */
import QtQuick
import "."

Item {
    id: root

    required property real efiBytes
    required property real rootBytes
    required property real recoveryBytes

    // Flip true to trigger the entrance animation. Flipping false→true
    // again restarts it (used when re-selecting a previously-deselected
    // disk card).
    property bool animate: false

    implicitHeight: 48     // bar (28) + spacing (4) + label row (14) + 2
    implicitWidth: 200     // caller usually overrides via Layout.fillWidth

    readonly property real totalBytes: efiBytes + rootBytes + recoveryBytes

    // Floor each segment to a minimum ratio so labels remain legible.
    // The §2.3 design plan accepts that the bar is approximate-to-scale
    // for disks under ~100 GB in exchange for label readability.
    readonly property real _efiRatio:      Math.max(0.08, efiBytes      / totalBytes)
    readonly property real _recoveryRatio: Math.max(0.12, recoveryBytes / totalBytes)
    readonly property real _rootRatio:     Math.max(0.0,
                                                    1.0 - _efiRatio - _recoveryRatio)

    function reset() {
        efiSeg.width = 0
        rootSeg.width = 0
        recoverySeg.width = 0
        rootDelay.stop()
        recoveryDelay.stop()
    }

    function _playEntrance() {
        efiSeg.width = efiSeg._target
        rootDelay.restart()
        recoveryDelay.restart()
    }

    onAnimateChanged: {
        if (animate) {
            efiSeg.width = 0
            rootSeg.width = 0
            recoverySeg.width = 0
            _playEntrance()
        }
    }

    // Re-fire when the parent width settles (initial layout may report 0).
    onWidthChanged: {
        if (!animate) {
            efiSeg.width = efiSeg._target
            rootSeg.width = rootSeg._target
            recoverySeg.width = recoverySeg._target
        }
    }

    Column {
        anchors.fill: parent
        spacing: 4

        // ── Bar ────────────────────────────────────────────────────────────
        Row {
            id: barRow
            width: parent.width
            height: 28
            spacing: 2

            // EFI segment — starts at t=0
            Rectangle {
                id: efiSeg
                height: parent.height
                radius: InstallerTheme.radiusSm
                color: InstallerTheme.partitionEfi
                clip: true

                readonly property real _target:
                    barRow.width > 0 ? barRow.width * root._efiRatio : 0
                width: 0

                Behavior on width {
                    enabled: root.animate
                    NumberAnimation {
                        duration: InstallerTheme.durationDiagram
                        easing.type: InstallerTheme.easingDecelerate
                    }
                }
            }

            // Root segment — starts at t=+40ms
            Rectangle {
                id: rootSeg
                height: parent.height
                radius: InstallerTheme.radiusSm
                color: InstallerTheme.partitionRoot
                clip: true

                readonly property real _target:
                    barRow.width > 0 ? barRow.width * root._rootRatio : 0
                width: 0

                Timer {
                    id: rootDelay
                    interval: InstallerTheme.diagramStagger
                    onTriggered: rootSeg.width = rootSeg._target
                }

                Behavior on width {
                    enabled: root.animate
                    NumberAnimation {
                        duration: InstallerTheme.durationDiagram
                        easing.type: InstallerTheme.easingDecelerate
                    }
                }
            }

            // Recovery segment — starts at t=+80ms
            Rectangle {
                id: recoverySeg
                height: parent.height
                radius: InstallerTheme.radiusSm
                color: InstallerTheme.partitionRecovery
                clip: true

                readonly property real _target:
                    barRow.width > 0 ? barRow.width * root._recoveryRatio : 0
                width: 0

                Timer {
                    id: recoveryDelay
                    interval: InstallerTheme.diagramStagger * 2
                    onTriggered: recoverySeg.width = recoverySeg._target
                }

                Behavior on width {
                    enabled: root.animate
                    NumberAnimation {
                        duration: InstallerTheme.durationDiagram
                        easing.type: InstallerTheme.easingDecelerate
                    }
                }
            }
        }

        // ── Labels row ─────────────────────────────────────────────────────
        Item {
            width: parent.width
            height: 14

            Text {
                x: efiSeg.x
                text: qsTr("EFI  1 GB")
                font.pixelSize: InstallerTheme.fontPxXs
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textTertiary
            }

            // Center the root label under its segment.
            Text {
                id: rootLabel
                x: rootSeg.x + (rootSeg.width / 2) - (width / 2)
                text: qsTr("SLM Desktop")
                font.pixelSize: InstallerTheme.fontPxXs
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textSecondary
                visible: rootSeg.width > rootLabel.implicitWidth + 16
            }

            Text {
                id: recoveryLabel
                x: recoverySeg.x + recoverySeg.width - width
                text: qsTr("Recovery  8 GB")
                font.pixelSize: InstallerTheme.fontPxXs
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textTertiary
            }
        }
    }
}
