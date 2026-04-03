import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("System Log")
                color: "white"
                font.pixelSize: 18
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Refresh")
                onClicked: logText.text = RecoveryApp.logSummary()
            }
        }

        Text {
            text: qsTr("Recent greetd journal entries (last 50 lines).")
            color: "#aaa"
            font.pixelSize: 13
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: logText
                text: RecoveryApp.logSummary()
                readOnly: true
                wrapMode: TextEdit.NoWrap
                color: "#ccc"
                background: Rectangle { color: "#111" }
                font.family: "monospace"
                font.pixelSize: 12
                padding: 12
            }
        }
    }
}
