import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Item {
    id: root

    property var issues: []
    property string message: qsTr("Required runtime components are missing.")
    property var installHandler: null
    property bool autoVisible: true
    property string installStatus: ""
    property bool installBusy: false

    signal refreshRequested()
    signal postInstall()

    visible: !autoVisible || (issues || []).length > 0
    implicitHeight: visible ? card.implicitHeight : 0

    Rectangle {
        id: card
        anchors.left: parent.left
        anchors.right: parent.right
        radius: 8
        color: Theme.color("warningBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("warning")
        implicitHeight: cardCol.implicitHeight + 16

        ColumnLayout {
            id: cardCol
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: root.message
                color: Theme.color("textPrimary")
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.issues || []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    required property var modelData

                    Text {
                        Layout.fillWidth: true
                        text: String((modelData || {}).title || (modelData || {}).componentId || "Unknown")
                              + " — "
                              + String((modelData || {}).guidance || (modelData || {}).description || "")
                        color: Theme.color("textSecondary")
                        wrapMode: Text.WordWrap
                    }

                    DSStyle.Button {
                        visible: !!(modelData || {}).autoInstallable
                        enabled: !root.installBusy
                        text: root.installBusy ? qsTr("Installing...") : qsTr("Install")
                        onClicked: {
                            if (typeof root.installHandler !== "function") {
                                return
                            }
                            root.installBusy = true
                            var res = root.installHandler(String((modelData || {}).componentId || ""))
                            root.installBusy = false
                            root.installStatus = (!!res && !!res.ok)
                                    ? qsTr("Install completed. Rechecking...")
                                    : (qsTr("Install failed: ") + String((res && res.error) ? res.error : "unknown"))
                            root.refreshRequested()
                            if (!!res && !!res.ok) {
                                root.postInstall()
                            }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.installStatus.length > 0
                text: root.installStatus
                color: Theme.color("textSecondary")
                wrapMode: Text.WordWrap
            }
        }
    }
}
