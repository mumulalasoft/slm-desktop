/*
 * InstallerCard — the centered surface container used by every installer
 * screen. §4 Surfaces: max width 560px, radius 12 (radiusCard), surface
 * background, shadowMd. The background behind it is always windowBg.
 *
 * Slots:
 *   headline   — optional, 28px Open Sans 700 with display tracking
 *   body       — optional, 14px Open Sans 400, textSecondary
 *   default    — the screen's main content (placed below headline+body)
 *   footer     — pinned to the bottom of the card (typically the
 *                primary action button)
 */
import QtQuick
import QtQuick.Layouts
import "."

Rectangle {
    id: root

    property alias headline: headlineText.text
    property alias body: bodyText.text
    default property alias content: contentItem.data
    property alias footer: footerSlot.data

    readonly property real horizontalPadding: 32
    readonly property real verticalPadding: 28

    // §4: max-width 560 centered on windowBg.
    width: Math.min(parent ? parent.width - 96 : 560, 560)
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    implicitHeight: layout.implicitHeight + verticalPadding * 2

    radius: InstallerTheme.radiusCard
    color: InstallerTheme.surface

    // Soft drop shadow (shadowMd equivalent). Static — installer surfaces
    // do not animate elevation.
    Rectangle {
        anchors {
            fill: parent
            topMargin: 6
            leftMargin: 1
            rightMargin: 1
            bottomMargin: -6
        }
        radius: parent.radius
        color: InstallerTheme.textPrimary
        opacity: InstallerTheme.opacityHoverWash  // 3% — soft, not pronounced
        z: -1
    }

    ColumnLayout {
        id: layout
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            margins: root.verticalPadding
            leftMargin: root.horizontalPadding
            rightMargin: root.horizontalPadding
        }
        spacing: 0

        Text {
            id: headlineText
            visible: text.length > 0
            font.pixelSize: InstallerTheme.fontPxDisplay
            font.weight: InstallerTheme.weightBold
            font.family: InstallerTheme.fontFamilyUi
            font.letterSpacing: InstallerTheme.letterSpacingDisplay
            color: InstallerTheme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: 8
        }

        Text {
            id: bodyText
            visible: text.length > 0
            font.pixelSize: InstallerTheme.fontPxBody
            font.weight: InstallerTheme.weightRegular
            font.family: InstallerTheme.fontFamilyUi
            color: InstallerTheme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: 24
        }

        // Main content slot.
        Item {
            id: contentItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: childrenRect.height
        }

        // Footer slot — typically the primary action.
        Item {
            id: footerSlot
            visible: footerSlot.children.length > 0
            Layout.fillWidth: true
            Layout.topMargin: visible ? 24 : 0
            implicitHeight: childrenRect.height
        }
    }
}
