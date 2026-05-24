/*
 * SLM Installer — progress screen ambient illustration.
 *
 * See docs/SLM_INSTALLER_DESIGN.md §6: this is the slow, ambient composition
 * the user sees during the 8–14 minute install. It must NOT render a feature
 * carousel or a log ticker. Keep it quiet.
 *
 * This is a scaffold placeholder; the real composition will be implemented
 * once branding assets and the InstallerProgressStage component land.
 */

import QtQuick

Item {
    id: root
    width: 800
    height: 520

    Rectangle {
        anchors.fill: parent
        color: "#1a1a1c"
    }

    Text {
        anchors.centerIn: parent
        text: qsTr("Installing SLM Desktop")
        color: "#f5f5f7"
        font.pixelSize: 22
        font.weight: Font.DemiBold
    }
}
