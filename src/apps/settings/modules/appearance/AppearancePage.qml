import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 48
    clip: true

    property var themeBinding: SettingsApp.createBindingFor("appearance", "theme", "default")
    property var accentBinding: SettingsApp.createBindingFor("appearance", "accent", "#007AFF")

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 24

        SettingGroup {
            title: qsTr("Appearance")
            Layout.fillWidth: true
            
            SettingCard {
                label: qsTr("Color Theme")
                description: qsTr("Choose how the system looks.")
                Layout.fillWidth: true
                
                RowLayout {
                    spacing: 12
                    Button {
                        text: "Light"
                        highlighted: root.themeBinding.value === "prefer-light"
                        onClicked: root.themeBinding.value = "prefer-light"
                    }
                    Button {
                        text: "Dark"
                        highlighted: root.themeBinding.value === "prefer-dark"
                        onClicked: root.themeBinding.value = "prefer-dark"
                    }
                    Button {
                        text: "Auto"
                        highlighted: root.themeBinding.value === "default"
                        onClicked: root.themeBinding.value = "default"
                    }
                }
            }

            SettingCard {
                label: qsTr("Accent Color")
                description: qsTr("Change the system accent color.")
                Layout.fillWidth: true
                
                RowLayout {
                    spacing: 8
                    Repeater {
                        model: ["#007AFF", "#34C759", "#FF9500", "#FF3B30", "#AF52DE"]
                        delegate: Rectangle {
                            width: 24; height: 24; radius: 12; color: modelData
                            border.color: root.accentBinding.value === modelData ? "black" : "transparent"
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.accentBinding.value = modelData
                            }
                        }
                    }
                }
            }
        }

        SettingGroup {
            title: qsTr("Desktop")
            Layout.fillWidth: true
            
            SettingCard {
                label: qsTr("Wallpaper")
                description: qsTr("Personalize your desktop background.")
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("Choose Picture...")
                }
            }
        }
    }
}
