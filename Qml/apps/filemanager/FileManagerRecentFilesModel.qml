// FileManagerRecentFilesModel.qml
//
// Model non-visual untuk recent files. Memanggil FileManagerApi.recentFiles()
// dan meng-expose hasilnya sebagai property QML list.
//
// Cara pakai:
//   FileManagerRecentFilesModel {
//       id: recentModel
//       limit: 20
//       onLoaded: myListView.model = recentModel.files
//   }

import QtQuick

Item {
    id: root

    // Jumlah maksimal recent files yang dimuat
    property int limit: 50

    // Hasil — list of { path, name, mimeType, size, modified, iconName }
    property var files: []

    // true selama loading
    property bool loading: false

    // Signal saat data sudah tersedia
    signal loaded()

    // Auto-load saat komponen selesai dibuat
    Component.onCompleted: reload()

    // ── API ───────────────────────────────────────────────────────────────

    /** Muat ulang recent files dari FileManagerApi */
    function reload() {
        if (typeof FileManagerApi === "undefined" || !FileManagerApi) return
        root.loading = true
        const raw = FileManagerApi.recentFiles(root.limit) || []
        root.files = raw
        root.loading = false
        root.loaded()
    }

    /** Hapus semua recent files */
    function clear() {
        if (typeof FileManagerApi === "undefined" || !FileManagerApi) return
        FileManagerApi.clearRecentFiles()
        root.files = []
        root.loaded()
    }
}
