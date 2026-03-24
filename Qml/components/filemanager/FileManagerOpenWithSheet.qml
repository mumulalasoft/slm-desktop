// FileManagerOpenWithSheet.qml
//
// Sheet untuk memilih aplikasi yang akan membuka sebuah file.
// Menggunakan FileManagerApi.startOpenWithApplications() untuk mendapatkan
// daftar aplikasi yang bisa menangani tipe file tersebut.
//
// Cara pakai:
//   FileManagerOpenWithSheet {
//       id: openWithSheet
//   }
//   // Tampilkan:
//   openWithSheet.openFor("/home/user/document.pdf")

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Drawer {
    id: root

    edge: Qt.BottomEdge
    height: Math.min(implicitHeight, parent ? parent.height * 0.6 : 400)
    width: parent ? parent.width : 400
    modal: true

    // ── State ─────────────────────────────────────────────────────────────

    property string filePath: ""
    property var applications: []
    property bool loading: false
    property string _requestId: ""

    // ── API ───────────────────────────────────────────────────────────────

    function openFor(path) {
        root.filePath = path
        root.applications = []
        root.loading = true
        root.open()

        if (typeof FileManagerApi === "undefined" || !FileManagerApi) {
            root.loading = false
            return
        }

        root._requestId = "openwith-" + Date.now()
        FileManagerApi.startOpenWithApplications(path, 24, root._requestId)
    }

    // ── Wiring ────────────────────────────────────────────────────────────

    Connections {
        target: typeof FileManagerApi !== "undefined" ? FileManagerApi : null
        enabled: target !== null

        function onOpenWithApplicationsReady(requestId, path, rows, error) {
            if (requestId !== root._requestId) return
            root.loading = false
            if (error) {
                console.warn("[OpenWithSheet] error:", error)
                return
            }
            root.applications = rows || []
        }
    }

    // ── UI ────────────────────────────────────────────────────────────────

    contentItem: Item {
        implicitHeight: sheetColumn.implicitHeight + 32

        ColumnLayout {
            id: sheetColumn
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
            spacing: 12

            // Header
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Buka dengan")
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Text {
                    text: root.filePath.split("/").pop()
                    color: "#888"
                    font.pixelSize: 12
                    elide: Text.ElideLeft
                    Layout.maximumWidth: 200
                }
            }

            // Loading indicator
            Item {
                Layout.fillWidth: true
                height: 40
                visible: root.loading
                BusyIndicator { anchors.centerIn: parent; running: root.loading }
            }

            // App list
            Repeater {
                model: root.applications

                delegate: ItemDelegate {
                    Layout.fillWidth: true
                    height: 52

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Buka dengan %1").arg(modelData.name || "")

                    contentItem: RowLayout {
                        spacing: 12
                        Image {
                            width: 32; height: 32
                            source: modelData.icon
                                ? "image://themeicon/" + modelData.icon
                                : ""
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: modelData.name || ""
                                font.pixelSize: 14
                            }
                            Text {
                                text: modelData.isDefault
                                    ? qsTr("Aplikasi default")
                                    : (modelData.comment || "")
                                color: "#888"
                                font.pixelSize: 11
                                visible: text.length > 0
                            }
                        }
                    }

                    onClicked: {
                        if (!FileManagerApi) { root.close(); return }
                        FileManagerApi.startOpenPathWithApp(
                            root.filePath, modelData.appId || "", root._requestId + "-open")
                        root.close()
                    }
                }
            }

            // Set default button (bila ada aplikasi)
            Button {
                Layout.fillWidth: true
                text: qsTr("Jadikan default untuk tipe file ini")
                flat: true
                visible: root.applications.length > 0 && !root.loading

                Accessible.role: Accessible.Button
                Accessible.name: text

                onClicked: {
                    // Buka sub-picker untuk pilih default — atau gunakan yang pertama
                    // Implementasi lengkap di Tahap 4b
                    root.close()
                }
            }

            Item { height: 8 }
        }
    }
}
