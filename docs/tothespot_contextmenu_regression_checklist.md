# SLM Desktop: Tothespot Context Menu Regression Checklist

Tanggal baseline: 2026-03-04

Tujuan:
- memastikan konteks menu Tothespot stabil (posisi, aksi, keyboard),
- mencegah regresi “labil”, salah posisi, atau menu stale setelah hasil berubah.

## Quick smoke (semi-otomatis)
Jalankan:

```bash
./scripts/smoke-tothespot-contextmenu.sh
```

Catatan:
- Script menjalankan runtime smoke (`offscreen` default) dan validasi guard QML utama.
- Ini bukan pengganti uji manual A-E, tapi gate cepat sebelum commit.

## One-command gate (smoke + focused tests)
Jalankan:

```bash
./scripts/test-tothespot.sh
```

Mode opsional:
- `--smoke-only`: hanya smoke runtime + guard QML.
- `--full`: tambah full `ctest` setelah focused tests.

## Prasyarat
- Jalankan SLM Desktop di sesi yang biasa dipakai (kwin-wayland host atau mode pengujian tim).
- Buka Tothespot dengan data campuran:
  - ada item aplikasi,
  - ada item file,
  - ada item folder.

## A. Posisi dan stabilitas mouse
1. Klik kanan item pertama, tengah, dan terakhir daftar.
- Hasil: menu muncul di dekat titik klik, tidak lompat ke posisi lain.

2. Scroll daftar lalu klik kanan item lain.
- Hasil: menu tetap muncul di titik klik terbaru, tidak offset dari posisi lama.

3. Saat menu terbuka di item A, klik kanan item B.
- Hasil: menu langsung pindah ke item B dalam 1 klik (tanpa perlu klik kedua).

4. Klik kiri area luar menu.
- Hasil: menu menutup.

## B. Aksi menu sesuai jenis item
1. Item aplikasi:
- Entry yang muncul: `Launch`, `Properties`.
- Tidak boleh ada `Open Containing Folder`.

2. Item file:
- Entry yang muncul: `Open`, `Open Containing Folder`, `Properties`.

3. Item folder:
- Entry yang muncul: `Open`, `Properties`.
- Tidak boleh ada `Open Containing Folder`.

## C. Keyboard behavior
1. Buka menu konteks (klik kanan atau `Shift+F10`/Menu key).
- Hasil: item pertama ter-highlight.

2. Tekan `Down`/`Up`.
- Hasil: highlight berpindah dan wrap (akhir -> awal, awal -> akhir).

3. Tekan `Enter`.
- Hasil: aksi item ter-highlight dieksekusi, menu menutup.

4. Tekan `Esc`.
- Hasil: menu menutup tanpa eksekusi aksi.

## D. Konsistensi saat hasil berubah
1. Buka menu lalu ketik di kolom pencarian.
- Hasil: menu langsung menutup.

2. Buka menu lalu ubah query sampai daftar hasil berubah.
- Hasil: menu tidak tertinggal di posisi lama.

3. Buka menu lalu pindah seleksi hasil pakai `Up/Down`.
- Hasil: menu menutup otomatis.

## E. Kriteria lulus
- Semua langkah A-D lolos.
- Tidak ada gejala:
  - menu muncul jauh dari titik klik,
  - menu berkedip/menutup-ulang sendiri,
  - aksi salah item,
  - menu menggantung saat hasil berubah.

## Catatan bug (jika gagal)
- Catat:
  - langkah gagal,
  - jenis item (app/file/folder),
  - posisi item (atas/tengah/bawah),
  - setelah scroll atau tidak,
  - screenshot + log singkat.
