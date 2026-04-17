# TopBar Popup QA Checklist

Dokumen ini memverifikasi kontrak di `docs/TOPBAR_POPUP_CONTRACT.md`.

## Preconditions
- Build shell terbaru sudah terpasang.
- Session desktop berjalan normal (bukan safe mode).
- Tidak ada patch lokal lain yang mengubah topbar popup flow saat test.

## Test Matrix

### 1) Popup tampil di bawah trigger icon
Initial condition:
- Desktop idle.
- Semua popup topbar tertutup.

Steps:
1. Klik icon main menu pada topbar.
2. Klik icon indicator lain (network/sound/battery/datetime).
3. Ulangi di monitor utama dan monitor sekunder (jika multi-monitor).

Expected:
- Popup muncul di bawah icon trigger sebagai posisi default.
- Jika ruang bawah tidak cukup, popup boleh pindah ke atas trigger (fallback overflow), bukan keluar monitor.
- Tidak ada popup yang muncul jauh dari trigger.

### 2) Popup topbar selalu di atas aplikasi
Initial condition:
- Buka 2-3 aplikasi (normal + maximized).

Steps:
1. Fokus aplikasi fullscreen/maximized.
2. Buka popup topbar dari beberapa applet.
3. Pindahkan fokus antar aplikasi saat popup terbuka.

Expected:
- Popup topbar tetap terlihat di atas surface aplikasi.
- Popup tidak tertutup window app.
- Tidak ada flicker layering saat fokus app berubah.

### 3) Popup topbar tidak mempengaruhi state global menu
Initial condition:
- Ada app aktif yang mengekspor global menu.

Steps:
1. Buka salah satu kategori global menu.
2. Tutup global menu.
3. Buka popup topbar (main menu/indicator).
4. Tutup popup topbar.
5. Buka lagi kategori global menu.

Expected:
- Buka/tutup popup topbar tidak otomatis membuka kategori global menu.
- State kategori global menu tidak berubah implisit saat popup topbar dibuka/ditutup.
- Aksi global menu hanya berubah saat user interaksi langsung di area global menu.

### 4) One-active-popup invariant
Initial condition:
- Desktop idle.

Steps:
1. Buka popup A (mis. network).
2. Tanpa menutup manual, buka popup B (mis. sound).
3. Lanjut buka popup C (mis. datetime).

Expected:
- Hanya 1 popup topbar aktif pada satu waktu.
- Popup sebelumnya otomatis close saat popup baru dibuka.
- Tidak ada popup ganda saling overlap.

### 5) Close behavior konsisten
Initial condition:
- Popup topbar dalam kondisi terbuka.

Steps:
1. Tekan `Esc`.
2. Klik area di luar popup.
3. Klik icon trigger yang sama lagi (toggle).

Expected:
- Popup tertutup bersih di semua metode close.
- Tidak ada stuck state (controller menganggap open padahal popup sudah tertutup).

### 6) Multi-monitor anchor integrity
Initial condition:
- Dua monitor aktif.

Steps:
1. Letakkan shell/topbar pada monitor A lalu buka popup.
2. Pindah pointer/fokus ke monitor B lalu buka popup dari topbar monitor B.
3. Ulangi untuk applet berbeda.

Expected:
- Popup muncul pada monitor anchor masing-masing.
- Clamp geometry mengikuti monitor rect yang benar.

## Regression Checks
- Tidak ada crash loop saat popup dibuka/ditutup cepat berulang.
- Tidak ada warning/error QML baru terkait popup controller/host layer.
- Shortcut global menu tetap bekerja normal setelah stress open/close popup topbar.

## Optional Stress Script (Manual)
1. Spam klik cepat pada 3 icon popup berbeda selama 15-30 detik.
2. Ulangi sambil aplikasi berat sedang redraw.
3. Verifikasi:
   - tetap 1 popup aktif
   - tidak freeze
   - tidak ghost popup

## Pass Criteria
- Semua expected behavior di atas terpenuhi.
- Tidak ada coupling baru ke state global menu.
- Tidak ada regresi visual/z-order dibanding baseline.
