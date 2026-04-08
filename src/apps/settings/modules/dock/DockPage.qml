import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property string highlightSettingId: ""

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("Dock")
            Layout.fillWidth: true

            SettingCard {
                objectName: "icon-size"
                highlighted: root.highlightSettingId === "icon-size"
                label: qsTr("Icon Size")
                description: qsTr("Set the size of dock icons.")
                Layout.fillWidth: true

                ComboBox {
                    id: iconSizeCombo
                    model: [qsTr("Small"), qsTr("Medium"), qsTr("Large")]
                    readonly property var sizeKeys: ["small", "medium", "large"]
                    currentIndex: {
                        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return 1
                        var sz = DesktopSettings.dockIconSize || "medium"
                        var idx = sizeKeys.indexOf(sz)
                        return idx >= 0 ? idx : 1
                    }
                    onActivated: function(index) {
                        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setDockIconSize) {
                            DesktopSettings.setDockIconSize(sizeKeys[index])
                        }
                    }
                    Layout.preferredWidth: 160
                }
            }

            SettingCard {
                objectName: "magnification"
                highlighted: root.highlightSettingId === "magnification"
                label: qsTr("Magnification")
                description: qsTr("Enlarge icons when hovering over the dock.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: {
                        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return true
                        return DesktopSettings.dockMagnificationEnabled !== false
                    }
                    onToggled: {
                        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setDockMagnificationEnabled) {
                            DesktopSettings.setDockMagnificationEnabled(checked)
                        }
                    }
                }
            }
        }
    }
}
