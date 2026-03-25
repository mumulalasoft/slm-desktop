// FileManagerSidebarIntegration.qml
//
// Komponen non-visual yang menghubungkan FileManagerShellBridge ke model
// yang dibutuhkan shell sidebar (panel kiri, spotlight places, dll).
//
// Cara pakai dari shell:
//   FileManagerSidebarIntegration {
//       id: fmSidebar
//       onMountAdded: (path, label, icon) => sidebar.addDevice(path, label, icon)
//       onMountRemoved: (path) => sidebar.removeDevice(path)
//   }
//
//   // Bind places:
//   ListView { model: fmSidebar.places }

import QtQuick

Item {
    id: root

    // ── Public properties ─────────────────────────────────────────────────

    // Daftar sidebar places (bookmarks + drives). Diperbarui otomatis.
    // Tiap entry: { label, path, uri, icon, section, ejectable }
    property var places: []

    // Jumlah item di trash
    property int trashItemCount: 0

    // Apakah filemanager app tersedia via DBus activation
    property bool fileManagerAvailable: false

    // Apakah fileopsd tersedia
    property bool fileOpsAvailable: false

    // ── Signals dari sidebar ke shell ─────────────────────────────────────

    // Mount baru masuk (e.g. USB tancap)
    signal mountAdded(string mountPath, string label, string iconName, bool ejectable)

    // Mount dicabut
    signal mountRemoved(string mountPath)

    // File operation progress
    signal operationProgress(string operationId, real fraction, string statusText)
    signal operationFinished(string operationId, bool success, string error)

    // ── Wiring ke FileManagerShellBridge ──────────────────────────────────

    Connections {
        target: typeof FileManagerShellBridge !== "undefined"
                ? FileManagerShellBridge : null
        enabled: target !== null

        function onSidebarPlacesChanged() {
            root.places = FileManagerShellBridge.sidebarPlaces()
        }

        function onTrashCountChanged(count) {
            root.trashItemCount = count
        }

        function onFileManagerAvailableChanged() {
            root.fileManagerAvailable = FileManagerShellBridge.fileManagerAvailable
        }

        function onFileOpsAvailableChanged() {
            root.fileOpsAvailable = FileManagerShellBridge.fileOpsAvailable
        }

        function onMountAdded(mountPath, label, iconName, ejectable) {
            root.mountAdded(mountPath, label, iconName, ejectable)
            root.places = FileManagerShellBridge.sidebarPlaces()
        }

        function onMountRemoved(mountPath) {
            root.mountRemoved(mountPath)
            root.places = FileManagerShellBridge.sidebarPlaces()
        }

        function onOperationProgress(operationId, fraction, statusText) {
            root.operationProgress(operationId, fraction, statusText)
        }

        function onOperationFinished(operationId, success, error) {
            root.operationFinished(operationId, success, error)
        }
    }

    // ── Init ──────────────────────────────────────────────────────────────

    Component.onCompleted: {
        if (typeof FileManagerShellBridge === "undefined" || !FileManagerShellBridge) {
            console.warn("[FileManagerSidebarIntegration] FileManagerShellBridge tidak tersedia")
            return
        }

        // Load initial state
        root.places           = FileManagerShellBridge.sidebarPlaces()
        root.trashItemCount   = FileManagerShellBridge.trashItemCount
        root.fileManagerAvailable = FileManagerShellBridge.fileManagerAvailable
        root.fileOpsAvailable = FileManagerShellBridge.fileOpsAvailable
    }

    // ── Convenience methods (delegasi ke bridge) ──────────────────────────

    function openPath(path) {
        if (FileManagerShellBridge) FileManagerShellBridge.openPath(path)
    }

    function revealItem(path) {
        if (FileManagerShellBridge) FileManagerShellBridge.revealItem(path)
    }

    function mountVolume(deviceUri) {
        if (FileManagerShellBridge) FileManagerShellBridge.mountVolume(deviceUri)
    }

    function unmountVolume(mountPath) {
        if (FileManagerShellBridge) FileManagerShellBridge.unmountVolume(mountPath)
    }

    function ejectVolume(deviceUri) {
        if (FileManagerShellBridge) FileManagerShellBridge.ejectVolume(deviceUri)
    }

    function emptyTrash() {
        if (FileManagerShellBridge) FileManagerShellBridge.emptyTrash()
    }

    function startCopy(sources, dest) {
        if (!FileManagerShellBridge) return ""
        return FileManagerShellBridge.startCopy(sources, dest)
    }

    function startMove(sources, dest) {
        if (!FileManagerShellBridge) return ""
        return FileManagerShellBridge.startMove(sources, dest)
    }

    function cancelOperation(operationId) {
        if (FileManagerShellBridge) FileManagerShellBridge.cancelOperation(operationId)
    }
}
