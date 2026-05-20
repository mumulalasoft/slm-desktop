import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

DSStyle.AppDialog {
    id: shareSheet

    required property var hostRoot
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)

    title: "Share"
    dialogWidth: 468
    bodyPadding: 18
    footerPadding: 14
    showDefaultCloseFooter: true
    closeButtonText: "Done"

    property var shareActions: hostRoot ? hostRoot.contextSlmShareActions : []

    function panelColor() {
        return Theme.darkMode ? Qt.rgba(0.16, 0.16, 0.18, 1.0) : Qt.rgba(0.965, 0.97, 0.98, 1.0)
    }

    function rowColor(hovered) {
        if (hovered) {
            return Theme.darkMode ? Qt.rgba(0.22, 0.24, 0.26, 1.0) : Qt.rgba(1, 1, 1, 1.0)
        }
        return Theme.darkMode ? Qt.rgba(0.13, 0.13, 0.15, 1.0) : Qt.rgba(0.985, 0.987, 0.992, 1.0)
    }

    background: DSStyle.PopupSurface {
        implicitWidth: shareSheet.dialogWidth
        implicitHeight: 164
        popupRadius: Theme.radiusWindowAlt
        popupColor: Theme.color("surface")
        popupBorderColor: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.11)
        popupOpacity: 1.0
        elevation: "high"
    }

    bodyComponent: Component {
        Item {
            implicitHeight: Math.max(160, sheetColumn.implicitHeight)

            ColumnLayout {
                id: sheetColumn
                anchors.fill: parent
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: shareSheet.panelColor()
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.09)
                    implicitHeight: headerRow.implicitHeight + 24

                    RowLayout {
                        id: headerRow
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                            radius: 9
                            color: Theme.darkMode ? Qt.rgba(0.24, 0.28, 0.32, 1.0) : Qt.rgba(0.90, 0.94, 0.98, 1.0)
                            border.width: Theme.borderWidthThin
                            border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08)

                            Image {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                sourceSize.width: 24
                                sourceSize.height: 24
                                fillMode: Image.PreserveAspectFit
                                source: "image://themeicon/emblem-shared-symbolic?v=" + shareSheet.iconRevision
                                opacity: Theme.darkMode ? 0.9 : 0.72
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: "Share selected item"
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("title")
                                font.weight: Theme.fontWeight("semibold")
                                elide: Text.ElideRight
                            }

                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: "Choose a destination or service."
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("caption")
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                GridView {
                    id: gridLayout
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(124, Math.ceil(count / 3) * cellHeight)
                    model: shareSheet.shareActions
                    cellWidth: Math.floor(width / 3)
                    cellHeight: 112
                    interactive: contentHeight > height
                    clip: true

                    delegate: Item {
                        width: gridLayout.cellWidth
                        height: gridLayout.cellHeight

                        Rectangle {
                            id: bg
                            anchors.fill: parent
                            anchors.margins: 5
                            radius: 10
                            color: shareSheet.rowColor(mouseArea.containsMouse)
                            border.width: Theme.borderWidthThin
                            border.color: mouseArea.containsMouse
                                          ? Theme.color("accent")
                                          : (Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.08))

                            Behavior on color { ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                            Behavior on border.color { ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault } }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            width: parent.width - 20

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 46
                                Layout.preferredHeight: 46
                                radius: 10
                                color: Theme.darkMode ? Qt.rgba(0.20, 0.24, 0.28, 1.0) : Qt.rgba(0.92, 0.95, 0.98, 1.0)
                                border.width: Theme.borderWidthThin
                                border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.07)

                                Image {
                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    sourceSize.width: 28
                                    sourceSize.height: 28
                                    fillMode: Image.PreserveAspectFit
                                    source: {
                                        var iconSource = String(modelData.iconSource || "")
                                        if (iconSource.length > 0) return iconSource
                                        var iconName = String(modelData.iconName || (modelData.icon || ""))
                                        if (iconName.length > 0) return "image://themeicon/" + iconName + "?v=" + shareSheet.iconRevision
                                        return ""
                                    }
                                    visible: source.toString().length > 0
                                }
                            }

                            DSStyle.Label {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: true
                                text: String(modelData.name || modelData.id || "")
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                font.pixelSize: Theme.fontSize("caption")
                                font.weight: Theme.fontWeight("medium")
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
}
