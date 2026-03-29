import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: root

    property string appName: ""
    property string appIcon: ""
    property int notificationId: -1
    property string title: ""
    property string body: ""
    property string timestamp: ""
    property string priority: "normal"
    property bool compact: false
    property var actionsModel: []

    signal actionTriggered(string actionKey)
    signal dismissRequested()
    signal clicked()

    radius: 14
    color: Theme.darkMode ? Qt.rgba(30 / 255, 30 / 255, 30 / 255, 0.70)
                          : Qt.rgba(1, 1, 1, 0.70)
    border.width: Theme.borderWidthThin
    border.color: Theme.color("panelBorder")

    implicitWidth: compact ? 340 : 380
    implicitHeight: contentColumn.implicitHeight + 24

    RowLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignTop
            width: 28
            height: 28
            radius: 8
            color: Theme.color("controlBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            Image {
                anchors.fill: parent
                anchors.margins: 5
                source: root.appIcon
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: source.length > 0
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: root.title.length > 0 ? root.title : root.appName
                color: Theme.color("textPrimary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("semiBold")
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.body
                color: Theme.color("textSecondary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                maximumLineCount: compact ? 2 : 3
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: actionsRepeater.count > 0

                Repeater {
                    id: actionsRepeater
                    model: root.actionsModel
                    delegate: Button {
                        required property var modelData
                        text: String(modelData || "")
                        flat: true
                        onClicked: root.actionTriggered(text)
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.timestamp
                visible: text.length > 0 && !compact
                color: Theme.color("textMuted")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("micro")
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
