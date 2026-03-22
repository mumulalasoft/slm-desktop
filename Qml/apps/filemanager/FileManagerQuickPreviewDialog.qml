import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Desktop_Shell

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

        Label {
            Layout.fillWidth: true
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("subtitle")
            font.weight: Theme.fontWeight("semibold")
            elide: Text.ElideMiddle
            text: String(hostRoot.quickPreviewTitleText || "")
        }

        Label {
            Layout.fillWidth: true
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideMiddle
            text: String(hostRoot.quickPreviewMetaText || "")
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
                visible: status === Image.Ready && String(source || "").length > 0
            }

            Image {
                anchors.centerIn: parent
                width: 96
                height: 96
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                source: String(hostRoot.quickPreviewFallbackIconSource || "")
                visible: !quickPreviewImage.visible
            }
        }
    }

    onClosed: {
        hostRoot.quickPreviewImageSource = ""
    }
}
