# Branding Glossary

Dokumen ini mendefinisikan istilah branding UI terbaru agar konsisten di seluruh shell.

## Istilah Utama

1. `AppHub`
- Definisi: launcher aplikasi penuh layar (rebrand dari Launchpad).
- Fungsi: menampilkan grid aplikasi, pencarian aplikasi, dan aksi cepat seperti pin ke appdeck/desktop.
- Komponen utama: `Qml/components/appdeck/AppDeckExpandedView.qml`, `Qml/components/apphub/AppHub.qml`.

2. `AppDeck`
- Definisi: bar aplikasi di bagian bawah desktop (rebrand dari Dock).
- Fungsi: akses cepat aplikasi favorit/berjalan, indikator aktif, drag reorder, dan drop target.
- Komponen utama: `Qml/components/appdeck/AppDeck.qml`, `Qml/components/overlay/AppDeckWindow.qml`.

3. `Pulse`
- Definisi: command surface dan discovery overlay (rebrand dari Tothespot).
- Fungsi: mencari aplikasi/file/folder, command mode, quick actions, pratinjau hasil, dan aktivasi aksi konteks.
- Komponen utama: `Qml/components/appdeck/AppDeckContextView.qml`, `Qml/components/pulse/PulseOverlay.qml`.

4. `Crown`
- Definisi: panel/menu bar atas desktop (rebrand dari Topbar).
- Fungsi: menu global, status indikator, profil pencarian, screenshot control, dan akses applet.
- Komponen utama: `Qml/components/crown/Crown.qml`, `Qml/components/overlay/CrownWindow.qml`.

## Aturan Penamaan

- Gunakan nama branding baru (`AppHub`, `AppDeck`, `Pulse`, `Crown`) untuk teks yang terlihat user:
- label, judul window, tooltip, deskripsi settings, dan dokumentasi UX.
- Pertahankan nama internal teknis lama jika dibutuhkan kompatibilitas:
- key settings, id modul, nama sinyal/properti, namespace kode, dan scope aksi (mis. `pulse`).
- Saat menambah fitur baru, gunakan branding baru di UI sejak awal, tanpa mengubah kontrak internal yang masih aktif.
