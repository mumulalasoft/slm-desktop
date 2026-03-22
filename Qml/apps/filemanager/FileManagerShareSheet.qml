import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Desktop_Shell
import Style as DSStyle

DSStyle.AppDialog {
    id: shareSheet

    required property var hostRoot

    title: "Share"
    dialogWidth: 440
    showDefaultCloseFooter: true

    property var shareActions: hostRoot ? hostRoot.contextSlmShareActions : []

    bodyComponent: Component {
        Item {
            implicitHeight: gridLayout.contentHeight

            GridView {
                id: gridLayout
                anchors.fill: parent
                model: shareSheet.shareActions
                cellWidth: 100
                cellHeight: 110
                interactive: false
                clip: true

                delegate: Item {
                    width: gridLayout.cellWidth
                    height: gridLayout.cellHeight

                    Rectangle {
                        id: bg
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: Theme.radiusControl
                        color: mouseArea.containsMouse ? Theme.color("accentSoft") : "transparent"

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        width: parent.width - 16

                        Image {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            fillMode: Image.PreserveAspectFit
                            source: {
                                var iconSource = String(modelData.iconSource || "")
                                if (iconSource.length > 0) return iconSource
                                var iconName = String(modelData.iconName || (modelData.icon || ""))
                                if (iconName.length > 0) return "image://themeicon/" + iconName
                                return ""
                            }
                            visible: source.toString().length > 0
                        }

                        DSStyle.Label {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            text: String(modelData.name || modelData.id || "")
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            font.pixelSize: Theme.fontSize("caption")
                            color: Theme.color("textPrimary")
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (modelData.id) {
                                hostRoot.invokeSlmCapabilityAction("Share", modelData.id)
                                shareSheet.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
