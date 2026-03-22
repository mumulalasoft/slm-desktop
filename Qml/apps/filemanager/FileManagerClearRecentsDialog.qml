import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Desktop_Shell

AppDialog {
    id: root

    required property var hostRoot

    title: "Clear Recent Files"
    standardButtons: Dialog.NoButton
    dialogWidth: 420
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)

    bodyComponent: Component {
        ColumnLayout {
            spacing: 10
            implicitHeight: 108

            Label {
                Layout.fillWidth: true
                text: "This will remove all entries from Recent Files."
                wrapMode: Text.Wrap
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                lineHeightMode: Text.ProportionalHeight
                lineHeight: Theme.lineHeight("normal")
            }

            Label {
                Layout.fillWidth: true
                text: "Files are not deleted from disk."
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.Wrap
            }
        }
    }

    footerComponent: Component {
        RowLayout {
            spacing: 8
            implicitHeight: Math.max(32, Theme.metric("controlHeightRegular"))

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Cancel"
                onClicked: root.close()
            }

            Button {
                text: "Clear"
                onClicked: {
                    root.close()
                    hostRoot.clearRecentFiles()
                }
            }
        }
    }
}
