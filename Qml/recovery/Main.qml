import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root
    visibility: Window.FullScreen
    color: "#1a1a1a"
    title: qsTr("SLM Recovery")
    property bool installBusy: false
    property string installStatusText: ""

    // ── Header ────────────────────────────────────────────────────────────────

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: 64
            color: "#c0392b"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24

                Text {
                    text: qsTr("Desktop Recovery")
                    color: "white"
                    font.pixelSize: 22
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: qsTr("SLM Desktop")
                    color: "#ffcdd2"
                    font.pixelSize: 13
                }
            }
        }

        // ── Crash notice ──────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            height: noticeColumn.implicitHeight + 24
            color: "#2c2c2c"
            visible: RecoveryApp.recoveryReason.length > 0 || RecoveryApp.crashCount > 0

            ColumnLayout {
                id: noticeColumn
                anchors { left: parent.left; right: parent.right; top: parent.top }
                anchors.margins: 16
                spacing: 4

                Text {
                    text: qsTr("Recovery reason: ") + (RecoveryApp.recoveryReason || qsTr("unknown"))
                    color: "#ef9a9a"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Text {
                    visible: RecoveryApp.crashCount > 0
                    text: qsTr("Crash count: %1").arg(RecoveryApp.crashCount)
                    color: "#ffcc80"
                    font.pixelSize: 12
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: (RecoveryApp.missingComponents || []).length > 0
            color: "#2c2c2c"
            implicitHeight: missingColumn.implicitHeight + 24

            ColumnLayout {
                id: missingColumn
                anchors { left: parent.left; right: parent.right; top: parent.top }
                anchors.margins: 16
                spacing: 8

                Text {
                    text: qsTr("Komponen penting hilang. Beberapa fitur SLM mungkin tidak berjalan.")
                    color: "#ffcc80"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Repeater {
                    model: RecoveryApp.missingComponents || []
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        color: "#1f1f1f"
                        radius: 6
                        border.width: 1
                        border.color: "#3b3b3b"
                        implicitHeight: compCol.implicitHeight + 12

                        ColumnLayout {
                            id: compCol
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 4

                            Text {
                                text: String((modelData || {}).title || (modelData || {}).componentId || "Unknown")
                                color: "white"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Text {
                                text: String((modelData || {}).description || "")
                                color: "#cccccc"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: qsTr("Paket: ") + String((modelData || {}).packageName || "-")
                                    color: "#aaaaaa"
                                    font.pixelSize: 11
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    enabled: !root.installBusy
                                    text: root.installBusy ? qsTr("Installing...") : qsTr("Install")
                                    onClicked: {
                                        root.installBusy = true
                                        var res = RecoveryApp.installMissingComponent(String((modelData || {}).componentId || ""))
                                        root.installBusy = false
                                        if (!!res && !!res.ok) {
                                            root.installStatusText = qsTr("Instalasi komponen berhasil.")
                                            RecoveryApp.refreshMissingComponents()
                                        } else {
                                            root.installStatusText = qsTr("Gagal instal komponen: ")
                                                    + String((res && res.error) ? res.error : "unknown")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: root.installStatusText.length > 0
                    text: root.installStatusText
                    color: "#ffcdd2"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // ── Tab bar ───────────────────────────────────────────────────────────

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            background: Rectangle { color: "#222" }

            TabButton {
                text: qsTr("Reset")
                width: implicitWidth
                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "white" : "#aaa"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.checked ? "#333" : "transparent"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width; height: 2
                        color: "#c0392b"
                        visible: parent.parent.checked
                    }
                }
            }

            TabButton {
                text: qsTr("Snapshots")
                width: implicitWidth
                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "white" : "#aaa"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.checked ? "#333" : "transparent"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width; height: 2
                        color: "#c0392b"
                        visible: parent.parent.checked
                    }
                }
            }

            TabButton {
                text: qsTr("Log")
                width: implicitWidth
                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "white" : "#aaa"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.checked ? "#333" : "transparent"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width; height: 2
                        color: "#c0392b"
                        visible: parent.parent.checked
                    }
                }
            }
        }

        // ── Pages ─────────────────────────────────────────────────────────────

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            ResetPage {}
            SnapshotPage {}
            LogPage {}
        }

        // ── Footer ────────────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: "#222"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                Text {
                    text: qsTr("After applying changes, click Exit to restart the desktop session.")
                    color: "#888"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                Button {
                    text: qsTr("Exit to Desktop")
                    palette.button: "#c0392b"
                    palette.buttonText: "white"
                    font.pixelSize: 13
                    onClicked: RecoveryApp.exitToDesktop()
                }
            }
        }
    }
}
