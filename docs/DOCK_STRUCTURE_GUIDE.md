# PROMPT ANTI-GAGAL: Satu Dock Renderer (Layer-Shell) di atas Launchpad & Aplikasi

## Keputusan final (tidak boleh diubah)

* Hanya ada **1 Dock renderer**.
* Dock adalah **surface layer-shell** (bukan Item, bukan QWindow biasa).
* Launchpad **tidak punya Dock**.
* Tidak ada **dua host/renderer**.
* Tidak boleh pakai GTK.
* Tidak boleh pakai `Qt.WindowStaysOnTopHint` sebagai solusi utama.

Target stacking:

```text
DockSurface (TOP)
LaunchpadWindow
App Windows
```

---

## FASE 0 — Freeze & Audit (WAJIB)

**Tujuan:** Pastikan baseline bersih.

### Aksi:

* Tambahkan log unik saat Dock dibuat: `DOCK_CREATED <ptr>`
* Tambahkan log saat Launchpad toggle

### Verifikasi:

* [ ] Saat startup: hanya **1 log DOCK_CREATED**
* [ ] Toggle Launchpad: **tidak ada DOCK_CREATED baru**

Jika gagal → STOP.

---

## FASE 1 — Hapus semua Dock lama (WAJIB)

**Tujuan:** Pastikan tidak ada duplikasi renderer.

### Hapus:

* Semua `Dock` di `Main.qml` / ShellRoot
* Semua `Dock` di `LaunchpadWindow.qml`
* Semua `ShellDockHost`, `LaunchpadDockHost`
* Semua `shellDockState`, `launchpadDockState`

### Verifikasi (WAJIB):

* [ ] `grep -R "Dock.qml"` → hanya muncul di **DockWindow.qml**
* [ ] `LaunchpadWindow.qml` **tidak** meng-import/instantiate Dock
* [ ] `Main.qml` **tidak** membuat Dock

Jika masih ada 1 saja → STOP.

---

## FASE 2 — DockSystem (SSOT tunggal)

**Tujuan:** Semua state Dock di satu tempat.

### Buat `DockSystem.qml` (Singleton) dengan minimal:

```text
pinnedApps
runningApps
activeItemId
hoveredItemId
pressedItemId
dockRect
iconRects
dockVisible
```

### API minimal:

```text
setHoveredItem(id)
setPressedItem(id)
clearInteraction()
updateIconRects(rects)
```

### Aturan:

* Renderer **tidak boleh** punya state sendiri (hover/pressed/geometry)

### Verifikasi:

* [ ] Tidak ada `hoveredItem` lokal di Dock.qml
* [ ] Tidak ada mutation state di renderer

Jika ada → STOP.

---

## FASE 3 — DockController tunggal

**Tujuan:** Semua input lewat satu jalur.

### Buat `DockController.qml` (Singleton):

```text
onHover(id, pos)
onPress(id)
onRelease(id)
onActivate(id)
onDragStart(id)
onDragMove(pos)
onDragEnd()
onContextMenu(id)
```

### Aturan:

* Event dari UI → **DockController** → update **DockSystem**
* Renderer **tidak boleh** set state langsung

### Verifikasi:

* [ ] Semua `MouseArea` memanggil DockController
* [ ] Tidak ada `state = ...` di renderer

Jika gagal → STOP.

---

## FASE 4 — DockWindow (layer-shell) SATU-SATUNYA

**Tujuan:** Dock keluar dari scene biasa.

### Buat di C++:

* `DockLayerWindow : QQuickWindow`
* Binding ke **layer-shell** (mis. LayerShellQt atau binding yang kamu punya)

### Konfigurasi:

* layer: **TOP**
* anchor: **BOTTOM | LEFT | RIGHT**
* set **exclusive_zone = dockHeight**

### Lifecycle:

```text
onShellStart():
  create DockLayerWindow ONCE
  load DockWindow.qml
```

### DockWindow.qml:

```qml
// Hanya ini:
Dock {  // satu-satunya renderer
  bind to DockSystem
  send events to DockController
}
```

### Verifikasi:

* [ ] Hanya ada **1 DockLayerWindow**
* [ ] Dock muncul tanpa Launchpad
* [ ] Toggle Launchpad → **tidak membuat Dock baru**
* [ ] Log `DOCK_CREATED` tetap satu

Jika gagal → STOP.

---

## FASE 5 — Launchpad TANPA Dock

**Tujuan:** Putus total relasi Dock–Launchpad.

### Aksi:

* `LaunchpadWindow.qml` hanya konten Launchpad
* Tidak ada import/instance Dock

### Verifikasi:

* [ ] Buka Launchpad → Dock tetap terlihat
* [ ] Tutup Launchpad → Dock tetap instance yang sama
* [ ] Tidak ada Dock di Launchpad tree

Jika gagal → STOP.

---

## FASE 6 — Stacking VALIDASI (krusial)

**Tujuan:** Pastikan urutan layer benar di compositor.

### Cek:

* Dock **selalu di atas** Launchpad
* Launchpad di atas app

### Verifikasi:

* [ ] Buka Launchpad fullscreen → Dock **tidak tertutup**
* [ ] Buka app fullscreen → Dock tetap di atas (sesuai policy)
* [ ] Tidak ada flicker saat toggle

Jika Dock tertutup:

* Pastikan Dock benar layer-shell **TOP**
* Pastikan Launchpad **bukan** layer yang sama/lebih tinggi
* Jangan kembali ke `WindowStaysOnTopHint`

---

## FASE 7 — Input & Exclusive Zone

**Tujuan:** Interaksi benar dan tidak overlap.

### Aksi:

* `exclusive_zone = dockHeight`
* Tentukan policy input saat Launchpad:

  * tetap interaktif **atau** limited (pilih satu)

### Verifikasi:

* [ ] Klik Dock saat Launchpad aktif → sesuai policy
* [ ] Window tidak overlap area Dock

---

## Larangan keras (ulang):

* ❌ GTK Dock
* ❌ Dua renderer/host Dock
* ❌ Dock di Launchpad
* ❌ Dock di ShellRoot (Item biasa)
* ❌ Sinkronisasi dua Dock
* ❌ `WindowStaysOnTopHint` sebagai solusi utama

---

## Deliverable wajib per fase

Kirim:

1. Daftar file dibuat/diubah/dihapus
2. Cuplikan kode kunci (DockSystem, DockController, DockLayerWindow)
3. Checklist verifikasi fase (semua harus ✔)

Jika satu saja ❌ → fase belum selesai.

---

## Kalimat inti (pegangan):

> Dock adalah satu surface layer-shell milik shell.
> Launchpad tidak pernah memiliki Dock.
> Tidak ada duplikasi renderer.
> Semua state & input lewat DockSystem + DockController.

Kerjakan persis per fase. Jangan lompat.
