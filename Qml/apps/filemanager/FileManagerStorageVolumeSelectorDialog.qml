import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

AppDialog {
    id: root

    required property var hostRoot

    title: qsTr("Open Drive")
    standardButtons: Dialog.Cancel
    width: 420
    height: 320
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: String(hostRoot.storageSelectorDeviceLabel || qsTr("External Drive"))
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("body")
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusControl
            color: Theme.color("windowCard")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("windowCardBorder")

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                model: hostRoot.storageSelectorVolumes || []
                spacing: 4
                focus: true

                onCountChanged: {
                    if (count > 0 && currentIndex < 0) {
                        currentIndex = 0
                    }
                }

                Keys.onReturnPressed: {
                    if (currentIndex < 0 || currentIndex >= count) {
                        return
                    }
                    var row = model.get ? model.get(currentIndex) : model[currentIndex]
                    root.hostRoot.openStorageVolumeChoice(row)
                    root.close()
                }

                Keys.onEnterPressed: {
                    if (currentIndex < 0 || currentIndex >= count) {
                        return
                    }
                    var row = model.get ? model.get(currentIndex) : model[currentIndex]
                    root.hostRoot.openStorageVolumeChoice(row)
                    root.close()
                }

                delegate: Rectangle {
                    required property var modelData
                    readonly property string volumeName: String((modelData && modelData.name) ? modelData.name : "")
                    readonly property string volumePath: String((modelData && modelData.path) ? modelData.path : "")
                    readonly property bool volumeMounted: !!(modelData && modelData.mounted)
                    width: listView.width
                    height: 38
                    radius: Theme.radiusSm
                    color: pickMouse.containsMouse ? Theme.color("hoverOverlay") : "transparent"
                    border.width: listView.currentIndex === index ? Theme.borderWidthThin : 0
                    border.color: listView.currentIndex === index ? Theme.color("focusRing") : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Image {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            source: "image://themeicon/drive-harddisk-symbolic"
                        }

                        Text {
                            Layout.fillWidth: true
                            text: volumeName.length > 0 ? volumeName : (volumePath.length > 0 ? volumePath : qsTr("Drive"))
                            color: Theme.color("textPrimary")
                            elide: Text.ElideRight
                            font.pixelSize: Theme.fontSize("smallPlus")
                        }

                        Text {
                            text: volumeMounted ? qsTr("Mounted") : qsTr("Not mounted")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("tiny")
                        }
                    }

                    MouseArea {
                        id: pickMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: listView.currentIndex = index
                        onClicked: {
                            root.hostRoot.openStorageVolumeChoice(modelData)
                            root.close()
                        }
                    }
                }
            }
        }
    }
}
