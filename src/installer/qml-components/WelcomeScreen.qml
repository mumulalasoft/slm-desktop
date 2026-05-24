/*
 * WelcomeScreen — §2.1 (Welcome & Language) adapted for the SLM split:
 * the backend spec §1.2 moves all locale handling to the slm-welcome
 * OOBE (the post-install onboarding app), so the installer's welcome
 * screen is intentionally simpler than §2.1's full vision.
 *
 * What we keep from §2.1:
 *   - Wordmark + display headline ("Welcome to SLM Desktop.")
 *   - Calm-reassurance tone (one sentence below the headline)
 *   - "Continue" primary action
 *
 * What we defer to slm-welcome (OOBE):
 *   - Language list — locale picking happens after install, where the
 *     user picks their everyday language rather than the installer's.
 *   - Keyboard / accessibility — same reason; OOBE handles them per
 *     spec §1.2.
 *
 * Public API:
 *   continueRequested()    — caller advances to slm-disk-select.
 *
 * No viewmodule yet — upstream Calamares `welcome` keeps the pipeline
 * shipping until we either (a) replace it with a viewmodule that loads
 * this QML, or (b) decide the upstream module's brand presentation
 * is enough and quietly retire this file. Both are reasonable end-states;
 * landing the QML now means the design assets exist either way.
 */
import QtQuick
import QtQuick.Layouts
import "."

Rectangle {
    id: root

    signal continueRequested()

    color: InstallerTheme.windowBg

    InstallerCard {
        id: card
        anchors {
            top: parent.top
            topMargin: 96
            horizontalCenter: parent.horizontalCenter
        }

        // The headline IS the entire copy on this screen — leave
        // InstallerCard.headline/body unused and put the wordmark + display
        // text in the content slot so we can stack them with custom rhythm.
        Column {
            width: parent.width
            spacing: 16
            topPadding: 8
            bottomPadding: 8

            // Wordmark — 28px, semibold, slightly-negative tracking.
            // This is the first impression of SLM's typographic character.
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("SLM Desktop")
                font.pixelSize: InstallerTheme.fontPxDisplay
                font.weight: InstallerTheme.weightSemiBold
                font.family: InstallerTheme.fontFamilyUi
                font.letterSpacing: InstallerTheme.letterSpacingTitle3
                color: InstallerTheme.textPrimary
            }

            // Headline — calm, declarative. §2.1 nominally has a 36px
            // display token but the installer's overall hierarchy uses
            // fontPxDisplay (28px) — keeping the welcome consistent with
            // the rest of the surface rather than introducing a one-off
            // larger size.
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Welcome to SLM Desktop.")
                font.pixelSize: InstallerTheme.fontPxDisplay
                font.weight: InstallerTheme.weightBold
                font.family: InstallerTheme.fontFamilyUi
                font.letterSpacing: InstallerTheme.letterSpacingDisplay
                color: InstallerTheme.textPrimary
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            // One-sentence body — sets up the emotional arc per §1
            // ("Calm reassurance — I am in good hands").
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("This installer will set up SLM Desktop on your computer. The next step lets you choose where it should be installed.")
                font.pixelSize: InstallerTheme.fontPxBody
                font.family: InstallerTheme.fontFamilyUi
                color: InstallerTheme.textSecondary
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
                topPadding: 4
            }
        }

        footer: Item {
            width: parent.width
            height: continueButton.height

            InstallerPrimaryButton {
                id: continueButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Continue")
                onClicked: root.continueRequested()
            }
        }
    }
}
