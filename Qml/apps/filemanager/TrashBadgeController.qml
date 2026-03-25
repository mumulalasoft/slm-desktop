// TrashBadgeController.qml
//
// Controller non-visual yang meng-expose trash item count ke panel/applet.
// Menghubungkan FileManagerShellBridge.trashCountChanged ke badge property.
//
// Cara pakai di Dock atau panel:
//   TrashBadgeController {
//       id: trashBadge
//   }
//   Text {
//       text: trashBadge.count > 0 ? trashBadge.count.toString() : ""
//       visible: trashBadge.count > 0
//   }
//   // Atau pakai property binding:
//   badgeCount: trashBadge.count

import QtQuick

Item {
    id: root

    // Jumlah item di trash. 0 = kosong.
    property int count: 0

    // true jika trash tidak kosong
    property bool hasItems: count > 0

    // Emit saat user klik trash icon
    signal emptyTrashRequested()

    function emptyTrash() {
        if (FileManagerShellBridge) FileManagerShellBridge.emptyTrash()
    }

    // ── Wiring ────────────────────────────────────────────────────────────

    Connections {
        target: typeof FileManagerShellBridge !== "undefined"
                ? FileManagerShellBridge : null
        enabled: target !== null

        function onTrashCountChanged(newCount) {
            root.count = newCount
        }
    }

    Component.onCompleted: {
        if (typeof FileManagerShellBridge !== "undefined" && FileManagerShellBridge)
            root.count = FileManagerShellBridge.trashItemCount
    }
}
