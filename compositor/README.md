# compositor launcher (kwin-only)

Folder ini sekarang hanya menyimpan launcher untuk menjalankan `appSlm_Desktop`
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
- binary `appSlm_Desktop` sudah dibuild

## Catatan

- `--backend` hanya menerima `kwin-wayland`.
- `--host` dan `--skip-build` dipertahankan sebagai compatibility flag (no-op).
- Jalur nested wlroots compositor sudah dihapus dari launcher resmi.
