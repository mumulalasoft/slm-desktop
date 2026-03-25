import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 24

        // System Header
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            Image {
                source: "image://icon/preferences-system"
                Layout.preferredWidth: 80
                Layout.preferredHeight: 80
                Layout.alignment: Qt.AlignHCenter
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: "SLM Desktop"
                font.pixelSize: 22
                font.weight: Font.Bold
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "Version 1.0.0 (Core)"
                font.pixelSize: 14
                color: "#8E8E93"
                Layout.alignment: Qt.AlignHCenter
            }
        }

        SettingGroup {
            title: qsTr("Hardware")
            Layout.fillWidth: true
            
            SettingCard {
                label: qsTr("Processor")
                description: "Generic x86_64 Processor"
                Layout.fillWidth: true
            }

            SettingCard {
                label: qsTr("Memory")
                description: "16 GB"
                Layout.fillWidth: true
            }

            SettingCard {
                label: qsTr("Graphics")
                description: "Software Rendering / Integrated"
                Layout.fillWidth: true
            }
        }

        SettingGroup {
            title: qsTr("Software")
            Layout.fillWidth: true
                
            SettingCard {
                label: qsTr("Kernel Version")
                description: "Linux 6.1.0-custom"
                Layout.fillWidth: true
            }

            SettingCard {
                label: qsTr("Qt Version")
                description: "Qt 6.10.2"
                Layout.fillWidth: true
            }
        }
    }
}
