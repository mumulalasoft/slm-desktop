import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Item {
    id: root

    property var issues: []
    property bool busy: false
    property string statusText: ""
    property string summaryText: "Required runtime components are missing."
    property string installText: "Install"
    property string installingText: "Installing..."
    property string unknownText: "Unknown"
    property bool showPackageName: false
    property real scale: 1.0

    property color cardColor: Theme.color("surface")
    property color cardBorderColor: Theme.color("panelBorder")
    property color titleColor: Theme.color("textPrimary")
    property color detailColor: Theme.color("textSecondary")
    property color statusColor: Theme.color("warning")

    signal installRequested(string componentId)

    visible: (root.issues || []).length > 0
    implicitHeight: visible ? card.implicitHeight : 0

    Rectangle {
        id: card
        anchors.left: parent.left
        anchors.right: parent.right
        color: root.cardColor
        border.color: root.cardBorderColor
        border.width: Theme.borderWidthThin
        radius: Math.round(8 * root.scale)
        implicitHeight: column.implicitHeight + Math.round(16 * root.scale)

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: Math.round(10 * root.scale)
            spacing: Math.round(6 * root.scale)

            Label {
                Layout.fillWidth: true
                text: root.summaryText
                color: root.titleColor
                font.pixelSize: Math.round(13 * root.scale)
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.issues || []
                delegate: Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    color: Theme.color("controlBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    radius: Math.round(6 * root.scale)
                    implicitHeight: row.implicitHeight + Math.round(8 * root.scale)

                    RowLayout {
                        id: row
                        anchors.fill: parent
                        anchors.margins: Math.round(6 * root.scale)
                        spacing: Math.round(8 * root.scale)

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Math.round(2 * root.scale)

                            Label {
                                Layout.fillWidth: true
                                text: String((modelData || {}).title
                                             || (modelData || {}).componentId
                                             || (modelData || {}).code
                                             || root.unknownText)
                                color: root.titleColor
                                font.pixelSize: Math.round(12 * root.scale)
                                font.weight: Theme.fontWeight("bold")
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                text: String((modelData || {}).guidance
                                             || (modelData || {}).description
                                             || (modelData || {}).message
                                             || "")
                                color: root.detailColor
                                font.pixelSize: Math.round(11 * root.scale)
                                wrapMode: Text.WordWrap
                                visible: text.length > 0
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Package: " + String((modelData || {}).packageName || "-")
                                color: root.detailColor
                                font.pixelSize: Math.round(11 * root.scale)
                                wrapMode: Text.WordWrap
                                visible: root.showPackageName
                            }
                        }

                        Button {
                            visible: !!(modelData || {}).autoInstallable
                            enabled: !root.busy
                            text: root.busy ? root.installingText : root.installText
                            onClicked: root.installRequested(String((modelData || {}).componentId || ""))
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.statusText
                color: root.statusColor
                font.pixelSize: Math.round(11 * root.scale)
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }
    }
}
