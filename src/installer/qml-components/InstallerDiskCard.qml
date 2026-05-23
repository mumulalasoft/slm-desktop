/*
 * InstallerDiskCard — selectable card for Screen 3 (Disk Selection),
 * per docs/SLM_INSTALLER_DESIGN.md §2.3.
 *
 * Visual contract:
 *   Unselected: surface bg, 1px borderSubtle stroke.
 *   Selected:   surface bg, 2px accent inner stroke, partition diagram
 *               and erasure-warning region expand below the subtitle.
 *   Hovered:    subtle 3% textPrimary wash overlays the surface.
 *   Focused:    2px accent ring 3px outside the card (offset so it never
 *               overlaps the surface and remains visible on selection).
 *   SMART failed: card opacity drops to opacityDisabled; an inline
 *               error sentence explains the block; click events are
 *               consumed and the parent screen must keep its primary
 *               action disabled. See design report §2 — divergence from
 *               backend spec §3 (which treats SMART failure as warning).
 *
 * Interaction contract:
 *   Emits `clicked` only when the card is not disabled. The parent
 *   screen is responsible for the radio-group exclusion logic — write
 *   `slm.target.disk` to Calamares GlobalStorage on the chosen card's
 *   clicked() and clear the others' `selected` props.
 */
import QtQuick
import QtQuick.Layouts
import "."

Rectangle {
    id: root

    // ── Public API ─────────────────────────────────────────────────────────
    required property string diskName     // "Samsung 970 EVO 1TB"
    required property string diskSize     // "931 GB free" or "1 TB"
    required property string diskPath     // "/dev/nvme0n1"
    required property string diskKind     // "NVMe" | "SATA SSD" | "HDD" | "USB"
    required property string health       // "healthy" | "warning" | "failed"
    required property bool   selected

    // Partition layout sizes. The caller wires these from
    // slm.target.{esp,recovery}_size_mb in GlobalStorage; the root size
    // is computed from the disk's total minus the other two.
    property real efiBytes:      1073741824                  // 1 GB default
    property real recoveryBytes: 8589934592                  // 8 GB default
    property real rootBytes:     0                            // caller sets

    readonly property bool cardDisabled: health === "failed"

    signal clicked()

    // ── Layout / sizing ────────────────────────────────────────────────────
    width: parent ? parent.width : 560
    implicitHeight: contentCol.implicitHeight + 32
    radius: InstallerTheme.radiusCard
    color: InstallerTheme.surface
    activeFocusOnTab: true

    // ── Layered visual decoration ──────────────────────────────────────────
    // Border / selection stroke. Drawn as a child Rectangle so the inset
    // stroke clips correctly on non-integer radii (QML's border.color
    // renders outside the corner radius path on a Rectangle).
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.width: root.selected ? InstallerTheme.borderAccent
                                    : InstallerTheme.borderThin
        border.color: root.selected ? InstallerTheme.accent
                                    : InstallerTheme.borderSubtle
        z: 2
        Behavior on border.color {
            ColorAnimation { duration: InstallerTheme.durationFast }
        }
    }

    // Hover wash — subtle 3% textPrimary tint, only when not selected.
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: InstallerTheme.textPrimary
        opacity: hoverHandler.hovered && !root.selected && !root.cardDisabled
                 ? InstallerTheme.opacityHoverWash
                 : InstallerTheme.opacityHidden
        Behavior on opacity {
            NumberAnimation { duration: InstallerTheme.durationFast }
        }
        z: 1
    }

    // Keyboard focus ring — 3px outside the card.
    Rectangle {
        anchors {
            fill: parent
            margins: -3
        }
        radius: parent.radius + 3
        color: "transparent"
        border.width: InstallerTheme.borderAccent
        border.color: InstallerTheme.accent
        opacity: root.activeFocus ? InstallerTheme.opacityShown
                                  : InstallerTheme.opacityHidden
        Behavior on opacity {
            NumberAnimation {
                duration: InstallerTheme.durationFast
                easing.type: InstallerTheme.easingDecelerate
            }
        }
        z: 3
    }

    HoverHandler { id: hoverHandler }

    TapHandler {
        enabled: !root.cardDisabled
        onTapped: {
            root.forceActiveFocus()
            root.clicked()
        }
    }

    Keys.onSpacePressed:  if (!root.cardDisabled) root.clicked()
    Keys.onReturnPressed: if (!root.cardDisabled) root.clicked()

    // Block synthesised mouse events when disabled. The TapHandler above
    // is enabled-gated already; this overlay also stops hover events from
    // reaching the rest of the card so the wash doesn't appear.
    MouseArea {
        anchors.fill: parent
        enabled: root.cardDisabled
        cursorShape: Qt.ForbiddenCursor
        hoverEnabled: true
        z: 4
    }

    // ── Accessibility ──────────────────────────────────────────────────────
    Accessible.role: Accessible.RadioButton
    Accessible.name: diskName + ", " + diskSize + ", "
                     + diskKind + ", health " + health
                     + (selected ? ", selected" : "")
    Accessible.description: cardDisabled
        ? qsTr("This disk reported a hardware failure and cannot be used for installation.")
        : qsTr("Installing here will erase all data on this disk.")
    Accessible.checkable: true
    Accessible.checked: selected
    Accessible.onPressAction: if (!cardDisabled) root.clicked()

    // Whole-card opacity dip for disabled state.
    opacity: cardDisabled ? InstallerTheme.opacityDisabled
                          : InstallerTheme.opacityShown
    Behavior on opacity {
        NumberAnimation { duration: InstallerTheme.durationFast }
    }

    // ── Content ────────────────────────────────────────────────────────────
    ColumnLayout {
        id: contentCol
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 16
        }
        spacing: 0

        // Header row: radio + disk name + device path
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            // Radio indicator
            Rectangle {
                width: 18
                height: 18
                radius: width / 2
                border.width: root.selected ? 0 : InstallerTheme.borderThin
                border.color: InstallerTheme.borderSubtle
                color: root.selected ? InstallerTheme.accent : "transparent"
                Behavior on color {
                    ColorAnimation { duration: InstallerTheme.durationFast }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 7
                    height: 7
                    radius: width / 2
                    color: InstallerTheme.textOnAccent
                    visible: root.selected
                }
            }

            Text {
                text: root.diskName
                font.pixelSize: InstallerTheme.fontPxSubtitle
                font.weight: InstallerTheme.weightSemiBold
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textPrimary
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                text: root.diskPath
                font.pixelSize: InstallerTheme.fontPxSm
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textTertiary
            }
        }

        // Subtitle row: disk kind · size · SMART status (with icon)
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Layout.leftMargin: 28      // aligns with disk name, past the radio
            spacing: 6

            Text {
                text: root.diskKind + " · " + root.diskSize + " · " + qsTr("SMART:")
                font.pixelSize: InstallerTheme.fontPxSm
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textSecondary
            }

            // SMART icon — coloured pill until SVG assets land.
            Rectangle {
                width: 14
                height: 14
                radius: width / 2
                color: {
                    if (root.health === "failed")  return InstallerTheme.error
                    if (root.health === "warning") return InstallerTheme.warning
                    return InstallerTheme.success
                }
            }

            Text {
                text: {
                    if (root.health === "failed")  return qsTr("Failed")
                    if (root.health === "warning") return qsTr("Caution")
                    return qsTr("Healthy")
                }
                font.pixelSize: InstallerTheme.fontPxSm
                font.family: InstallerTheme.fontFamilyUi
                color: {
                    if (root.health === "failed")  return InstallerTheme.error
                    if (root.health === "warning") return InstallerTheme.warning
                    return InstallerTheme.textSecondary
                }
                font.weight: root.health === "failed"
                             ? InstallerTheme.weightMedium
                             : InstallerTheme.weightRegular
            }
        }

        // SMART FAILED inline message — only visible on failed cards.
        Text {
            visible: root.health === "failed"
            text: qsTr("This disk reported a hardware failure. Select a different disk.")
            font.pixelSize: InstallerTheme.fontPxSm
            font.family: InstallerTheme.fontFamilyUi
            font.weight: InstallerTheme.weightMedium
            color: InstallerTheme.error
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 28
        }

        // Selection-revealed region: partition diagram + erasure warning.
        Item {
            id: revealRegion
            Layout.fillWidth: true
            Layout.topMargin: root.selected ? 14 : 0
            Layout.leftMargin: 28

            implicitHeight: root.selected
                            ? revealCol.implicitHeight + 14
                            : 0
            clip: true

            opacity: root.selected ? InstallerTheme.opacityShown
                                   : InstallerTheme.opacityHidden

            Behavior on implicitHeight {
                NumberAnimation {
                    duration: InstallerTheme.durationDiagram
                    easing.type: InstallerTheme.easingDecelerate
                }
            }
            Behavior on opacity {
                NumberAnimation { duration: InstallerTheme.durationFast }
            }

            Column {
                id: revealCol
                width: parent.width
                spacing: 0

                Text {
                    text: qsTr("Layout after installation:")
                    font.pixelSize: InstallerTheme.fontPxSm
                    font.family: InstallerTheme.fontFamilyUi
                    color: InstallerTheme.textSecondary
                    bottomPadding: 8
                }

                PartitionDiagram {
                    id: diagram
                    width: revealCol.width
                    efiBytes:      root.efiBytes
                    rootBytes:     root.rootBytes
                    recoveryBytes: root.recoveryBytes
                    animate:       root.selected
                }

                // Separator under the diagram. §5 inline-error grammar reuses
                // this region; the rule keeps the warning visually anchored.
                Rectangle {
                    width: parent.width
                    height: 1
                    color: InstallerTheme.borderSubtle
                    opacity: InstallerTheme.opacitySeparator
                    anchors.topMargin: 10
                }

                Row {
                    width: parent.width
                    topPadding: 8
                    spacing: 8

                    // Left accent rule (high-stakes disclosure marker per
                    // design report §2: the user's data is about to be erased).
                    Rectangle {
                        width: 3
                        height: erasureText.implicitHeight
                        radius: width / 2
                        color: InstallerTheme.error
                    }

                    Text {
                        id: erasureText
                        width: parent.width - 11
                        text: qsTr("Your files on this disk will be erased.")
                        font.pixelSize: InstallerTheme.fontPxSm
                        font.weight: InstallerTheme.weightMedium
                        font.family: InstallerTheme.fontFamilyUi
                        color: InstallerTheme.textPrimary
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    // Reset the diagram when this card is deselected, so re-selection
    // re-fires the entrance animation cleanly.
    onSelectedChanged: if (!selected) diagram.reset()
}
