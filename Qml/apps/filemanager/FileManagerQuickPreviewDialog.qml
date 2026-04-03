import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

AppDialog {
    id: root

    required property var hostRoot

    modal: false
    title: "Quick Preview"
    standardButtons: Dialog.NoButton
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)
    width: Math.min(780, Math.max(420, hostRoot.width - 140))
    height: Math.min(620, Math.max(280, hostRoot.height - 140))

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        DSStyle.Label {
            Layout.fillWidth: true
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("subtitle")
            font.weight: Theme.fontWeight("semibold")
            elide: Text.ElideMiddle
            text: String(hostRoot.quickPreviewTitleText || "")
        }

        DSStyle.Label {
            Layout.fillWidth: true
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideMiddle
            text: String(hostRoot.quickPreviewMetaText || "")
        }

        RowLayout {
            Layout.fillWidth: true
            visible: hostRoot.quickPreviewArchiveMode
            spacing: 8

            DSStyle.Label {
                text: "Entries: " + String(Number(hostRoot.quickPreviewArchiveEntryCount || 0))
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }

            DSStyle.Label {
                text: String(hostRoot.quickPreviewArchiveLayout || "").length > 0
                      ? ("Layout: " + String(hostRoot.quickPreviewArchiveLayout || "").replace(/_/g, " "))
                      : ""
                visible: text.length > 0
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }

            Item { Layout.fillWidth: true }

            DSStyle.Label {
                text: "Showing first " + String((hostRoot.quickPreviewArchiveEntries || []).length)
                visible: hostRoot.quickPreviewArchiveTruncated
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusControl
            color: Theme.color("fileManagerSearchBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("fileManagerControlBorder")

            Image {
                id: quickPreviewImage
                anchors.fill: parent
                anchors.margins: 16
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                source: String(hostRoot.quickPreviewImageSource || "")
                visible: !hostRoot.quickPreviewArchiveMode
                         && status === Image.Ready
                         && String(source || "").length > 0
            }

            Image {
                anchors.centerIn: parent
                width: 96
                height: 96
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                source: String(hostRoot.quickPreviewFallbackIconSource || "")
                visible: !hostRoot.quickPreviewArchiveMode && !quickPreviewImage.visible
            }

            ScrollView {
                anchors.fill: parent
                anchors.margins: 10
                visible: hostRoot.quickPreviewArchiveMode
                clip: true

                TextArea {
                    readOnly: true
                    wrapMode: TextArea.NoWrap
                    selectByMouse: true
                    text: (hostRoot.quickPreviewArchiveEntries || []).join("\n")
                    font.family: "Monospace"
                    font.pixelSize: Math.max(11, Theme.fontSize("small"))
                    color: Theme.color("textPrimary")
                    background: null
                    leftPadding: 6
                    rightPadding: 6
                    topPadding: 6
                    bottomPadding: 6
                }
            }
        }
    }

    onClosed: {
        hostRoot.quickPreviewImageSource = ""
        hostRoot.quickPreviewArchiveMode = false
        hostRoot.quickPreviewArchiveEntries = []
        hostRoot.quickPreviewArchiveEntryCount = 0
        hostRoot.quickPreviewArchiveTruncated = false
        hostRoot.quickPreviewArchiveLayout = ""
    }
}
