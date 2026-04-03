# SLM Regression Checklist

Checklist ini dipakai sebelum merge perubahan UI/windowing/filemanager.

## File Manager (Core)

- Buka File Manager (`Alt+Shift+E`) tanpa blank/crash.
- Navigasi folder (`Home`, `Up`, `Back`, `Next`) berjalan normal.
- `Recent` menampilkan data host (GIO recent).
- Sort by time: item terbaru tampil di atas, section `Today/Yesterday/Older` tidak lompat.
- Search instant responsif, cancel query lama saat mengetik.
- Thumbnail muncul stabil saat scroll (tidak fallback ikon mendadak).
- Context menu:
  - file vs folder vs multi-selection sesuai.
  - `Open in New Tab` menambah tab benar.
- DnD copy/move (content -> sidebar) jalan.
- Storage sidebar:
  - mount/unmount state stabil (urutan tidak labil).
  - entry click memindahkan content view.

## Window CSD (Frameless)

- Rounded corner terlihat konsisten.
- Drag window dari area title kosong berjalan.
- Double click title area toggle maximize/restore.
- Resize dari 8 sisi/corner berjalan.
- Focus in/out tidak memunculkan gap/artefak besar.

## Topbar/Popup

- Popup topbar toggle stabil (klik kedua menutup, bukan reopen).
- Contract lint popup topbar lulus (`scripts/check-topbar-popup-contract.sh`).
- Main menu:
  - submenu `Recent Applications` dan `Recent Files` muncul saat ada data.
  - entry submenu menampilkan icon (recent app icon berwarna; recent files lewat fallback `iconName -> mimeType -> extension`).
  - guard test lulus: `topbar_mainmenu_recent_icons_guard_test`.
- Screenshot dialog tampil, tidak freeze input.
- Save screenshot dialog -> choose folder -> select folder berjalan.
- Global menu diagnostics valid (`dump` dan `healthcheck`) di sesi runtime aktif.

## Workspace (Mission Control Hybrid)

- `Super+S` toggle Workspace Overlay stabil (open/close tanpa flicker).
- Saat overlay terbuka, klik tab workspace di strip hanya pindah workspace (overlay tetap terbuka).
- Thumbnail scene hanya menampilkan window workspace aktif.
- Drag thumbnail ke tab workspace memindahkan window ke workspace target.
- Drag thumbnail ke tab placeholder membuat workspace baru lalu memindahkan window.
- `Super+Left/Right` pindah workspace normal mode berjalan.
- `Ctrl+Super+Left/Right` memindah focused window antar workspace berjalan.
- Klik app di Dock:
  - fokus window di workspace aktif jika ada,
  - jika tidak, pindah ke workspace window app lalu fokus/present.
- Invariant dinamis valid:
  - selalu ada trailing empty workspace,
  - workspace kosong non-last otomatis dibersihkan.

## Daemon / DBus

- `org.slm.Desktop.*` services start normal.
- FileOperations:
  - `Copy/Move/Trash/Delete` emit `Progress`, `Finished`, `Error`.
- Devices:
  - `Mount/Eject/Unlock/Format` reply sesuai.

## Quick Commands

```bash
scripts/test.sh
scripts/check-topbar-popup-contract.sh
ctest --test-dir build/Desktop_Qt_6_10_2-Debug -R topbar_mainmenu_recent_icons_guard_test --output-on-failure
scripts/test.sh secret-consent build/toppanel-Debug
scripts/smoke-runtime.sh
scripts/test-globalmenu.sh
```

Nightly soak (opsional):

```bash
SLM_TEST_ENABLE_SOAK=1 SLM_TEST_SOAK_MINUTES=5 scripts/test.sh
```

Nightly secret-consent lane knobs:

```bash
SLM_TEST_NIGHTLY_SECRET_CONSENT_MODE=required \
SLM_TEST_NIGHTLY_SECRET_CONSENT_SKIP_BUILD=1 \
scripts/test.sh nightly build
```
