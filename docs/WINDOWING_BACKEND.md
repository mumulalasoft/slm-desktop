# Windowing Backend

Slm Desktop runtime saat ini:
- Windowing backend command/state: `kwin-wayland` (IPC)
- Window decoration jalur utama: **CSD (client-side, frameless QML)** agar lintas compositor (tidak bergantung Plasma/KWin).

## Ringkasan

- `WindowingBackendManager` canonical ke `kwin-wayland`.
- Jalur backend lama (`slm-desktop` / `wlroots`) tidak dipakai lagi pada runtime app.
- `CompositorStateModel` tetap tersedia untuk kompatibilitas QML.
- Dekorasi jendela aplikasi SLM tidak mengandalkan server-side decoration.

## Startup Selection

Urutan konfigurasi backend:
1. Env var `DS_WINDOWING_BACKEND`
2. Preference `windowing.backend`
3. Default: `kwin-wayland`

Semua nilai input akan dinormalisasi ke `kwin-wayland`.

## Capability Matrix

| Capability | kwin-wayland |
|---|---|
| `window-list` | yes |
| `spaces` | yes |
| `switcher` | yes |
| `overview` | no |
| `progress-overlay` | no |
| `command.switcher-next` | yes |
| `command.switcher-prev` | yes |
| `command.progress-hide` | no |

## CLI

- `windowingctl` sekarang menampilkan matrix/watch untuk backend kwin-wayland saja.

## Catatan Migrasi

- Jika masih butuh eksperimen wlroots compositor, jalankan terpisah dari launcher resmi.
- Jalur launcher resmi proyek ini adalah kwin-only.
