# compositor launcher (kwin-only)

Folder ini sekarang hanya menyimpan launcher untuk menjalankan `slm-desktop`
di host Wayland dengan backend `kwin-wayland`.

## Jalankan

```bash
cd compositor
./scripts/run-nested.sh
```

Alias:

```bash
cd compositor
./scripts/nested-run.sh
```

## Syarat

- `XDG_SESSION_TYPE=wayland`
- `WAYLAND_DISPLAY` tersedia
- binary `slm-desktop` sudah dibuild

## Catatan

- `--backend` hanya menerima `kwin-wayland`.
- `--host` dan `--skip-build` dipertahankan sebagai compatibility flag (no-op).
- Jalur nested wlroots compositor sudah dihapus dari launcher resmi.
