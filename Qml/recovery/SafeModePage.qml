import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var openSnapshotsPage: null
    property string statusMessage: ""
    property bool statusOk: true

    function setStatus(ok, message) {
        statusOk = !!ok
        statusMessage = String(message || "")
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(parent.width, 680)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14
            anchors.top: parent.top
            anchors.topMargin: 24

            Text {
                text: qsTr("Safe Mode Actions")
                color: "white"
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Simple recovery actions for restoring desktop usability. "
                           + "Changes are applied safely and can be undone via snapshots.")
                color: "#aaaaaa"
                font.pixelSize: 13
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#3f3f3f" }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: qsTr("Restart Desktop")
                    onClicked: {
                        const ok = RecoveryApp.restartDesktop()
                        root.setStatus(ok, ok ? qsTr("Restart requested.")
                                              : qsTr("Failed to request restart."))
                    }
                }

                Button {
                    text: qsTr("Reset Desktop Settings")
                    onClicked: {
                        const ok = RecoveryApp.resetToSafeDefaults()
                        root.setStatus(ok, ok ? qsTr("Safe defaults applied.")
                                              : qsTr("Failed to reset settings."))
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: qsTr("Disable Extensions")
                    onClicked: {
                        const ok = RecoveryApp.disableExtensions()
                        root.setStatus(ok, ok ? qsTr("Extensions will be disabled on next session.")
                                              : qsTr("Failed to disable extensions."))
                    }
                }

                Button {
                    text: qsTr("Reset Graphics Stack")
                    onClicked: {
                        const ok = RecoveryApp.resetGraphicsStack()
                        root.setStatus(ok, ok ? qsTr("Software rendering fallback prepared.")
                                              : qsTr("Failed to reset graphics stack."))
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: qsTr("Rollback Snapshot")
                    onClicked: {
                        if (root.openSnapshotsPage) {
                            root.openSnapshotsPage()
                            root.setStatus(true, qsTr("Open Snapshots tab to select rollback target."))
                        } else {
                            root.setStatus(false, qsTr("Snapshot page handler unavailable."))
                        }
                    }
                }

                Button {
                    text: qsTr("Open Terminal")
                    onClicked: {
                        const ok = RecoveryApp.openTerminal()
                        root.setStatus(ok, ok ? qsTr("Terminal opened.")
                                              : qsTr("No terminal available."))
                    }
                }

                Button {
                    text: qsTr("Network Repair")
                    onClicked: {
                        const ok = RecoveryApp.networkRepair()
                        root.setStatus(ok, ok ? qsTr("Network repair command executed.")
                                              : qsTr("Network repair could not be applied."))
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: qsTr("Repair System")
                    onClicked: {
                        const ok = RecoveryApp.repairSystem()
                        root.setStatus(ok, ok ? qsTr("Repair command executed.")
                                              : qsTr("Repair failed. Run from Recovery Partition or with elevated privileges."))
                    }
                }

                Button {
                    text: qsTr("Reinstall System")
                    onClicked: {
                        const ok = RecoveryApp.reinstallSystem()
                        root.setStatus(ok, ok ? qsTr("Reinstall request registered.")
                                              : qsTr("Failed to register reinstall request."))
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 6
                color: root.statusOk ? "#1f4b27" : "#5b1f1f"
                visible: root.statusMessage.length > 0
                implicitHeight: messageText.implicitHeight + 14

                Text {
                    id: messageText
                    anchors.fill: parent
                    anchors.margins: 7
                    text: root.statusMessage
                    wrapMode: Text.WordWrap
                    color: "white"
                    font.pixelSize: 12
                }
            }

            Item { height: 8 }
        }
    }
}
