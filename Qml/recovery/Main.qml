import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components/system"

Window {
    id: root
    visibility: Window.FullScreen
    color: "#1a1a1a"
    title: qsTr("SLM Recovery")
    property bool installBusy: false
    property string installStatusText: ""
    property var missingIssues: []

    function refreshMissingIssues() {
        if (typeof MissingComponents === "undefined" || !MissingComponents) {
            missingIssues = []
            return
        }
        missingIssues = MissingComponents.missingComponentsForDomain("recovery")
    }

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

        MissingComponentsCard {
            Layout.fillWidth: true
            issues: root.missingIssues || []
            summaryText: qsTr("Komponen penting hilang. Beberapa fitur SLM mungkin tidak berjalan.")
            showPackageName: true
            busy: root.installBusy
            statusText: root.installStatusText
            cardColor: "#2c2c2c"
            cardBorderColor: "#3b3b3b"
            titleColor: "white"
            detailColor: "#cccccc"
            statusColor: "#ffcdd2"
            onInstallRequested: function(componentId) {
                root.installBusy = true
                var res = MissingComponents.installComponentForDomain("recovery", componentId)
                root.installBusy = false
                if (!!res && !!res.ok) {
                    root.installStatusText = qsTr("Instalasi komponen berhasil.")
                    root.refreshMissingIssues()
                } else {
                    root.installStatusText = qsTr("Gagal instal komponen: ")
                            + String((res && res.error) ? res.error : "unknown")
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

    Component.onCompleted: refreshMissingIssues()
}
