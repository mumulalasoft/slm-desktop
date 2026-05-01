# AppDeck Unified Terminology & State System

Dokumen ini adalah referensi utama untuk AppDeck sebagai satu-satunya surface aplikasi utama di SLM Desktop.
Semua implementasi, diskusi, dan binding QML harus mengikuti kontrak ini.

## 1. Single Source of Truth

Hanya ada satu surface utama:

```text
AppDeck
```

Tidak ada surface terpisah untuk launcher, hub, atau surface discovery lain.
Jika ada perilaku baru, itu harus dimodelkan sebagai `state` atau `mode` dari AppDeck.

## 2. Pemisahan Konsep

AppDeck dipisahkan menjadi dua lapisan konsep:

### State = bentuk / ukuran

State mengatur layout, ukuran, posisi, dan transformasi spatial.

```text
dock
grid
```

### Mode = fungsi / konten

Mode mengatur isi, interaksi, dan behavior konten.

```text
apps
pulse
context
```

## 3. Kontrak QML

Semua UI binding harus berbasis dua properti ini:

```qml
property string state: "dock"   // dock | grid
property string mode: "apps"    // apps | pulse | context
```

State adalah pengendali bentuk. Mode adalah pengendali isi.
Jangan mencampur keduanya dalam satu flag atau satu string gabungan.

## 4. Kombinasi Valid

Hanya kombinasi berikut yang sah:

```text
AppDeck[state=dock, mode=apps]
AppDeck[state=grid, mode=apps]
AppDeck[state=grid, mode=pulse]
AppDeck[state=grid, mode=context]
```

## 5. Kombinasi Terlarang

Kombinasi berikut tidak boleh dipakai:

```text
AppDeck[state=dock, mode=pulse]
AppDeck[state=dock, mode=context]
```

Dock hanya untuk `apps`. Dock adalah anchor UI, bukan surface hasil atau command mode.

## 6. Terminologi Resmi

Gunakan alias berikut saat berdiskusi atau menulis code:

| Alias   | Representasi                    |
|---------|---------------------------------|
| dock    | `AppDeck[state=dock, mode=apps]` |
| grid    | `AppDeck[state=grid, mode=apps]` |
| pulse   | `AppDeck[state=grid, mode=pulse]` |
| context | `AppDeck[state=grid, mode=context]` |

## 7. Transisi Resmi

```text
dock -> grid
grid -> dock

grid -> pulse
pulse -> grid

grid -> context
context -> grid
```

Tidak ada transisi lain yang dianggap valid.

## 8. Trigger Dasar

- Klik icon atau gesture up:
  `dock -> grid`
- `Cmd + Space` atau search trigger:
  `grid -> pulse`
- `ESC`:
  `pulse -> grid`
- Right click atau selection:
  `grid -> context`

Jika implementasi baru perlu membuka AppDeck, pilih transisi yang paling dekat dengan intent pengguna dan tetap menjaga kombinasi valid di atas.

## 9. Aturan Animasi

- Perubahan `state` menghasilkan animasi spatial: scale, position, depth.
- Perubahan `mode` menghasilkan animasi content: fade, swap, blur, transition.
- Jangan mencampur animasi spatial dan animasi content dalam satu layer jika bisa dipecah.

Prinsipnya:

- `state` mengubah bentuk surface
- `mode` mengubah isi surface

## 10. Arsitektur Implementasi

AppDeck harus diperlakukan sebagai satu surface dengan beberapa zone internal:

- dock zone
- grid zone
- pulse zone
- context zone

Zone boleh muncul atau hilang, tetapi state dan mode tetap menjadi sumber keputusan utama.

Jangan hardcode layout di luar state machine AppDeck.

## 11. Hubungan Dengan Pulse

Pulse adalah engine intent dan search, bukan surface terpisah untuk hasil.

Flow yang benar:

```text
User input -> Pulse -> hasil / intent -> AppDeck(mode=context)
```

Pulse boleh mengubah query, seed, dan hasil, tetapi rendering tetap berada di AppDeck.

## 12. Hubungan Dengan Navigasi Aplikasi

`apps` adalah mode default untuk akses aplikasi.
`pulse` dan `context` hanya aktif pada `state=grid`.

Dock hanya boleh menampilkan `apps`.
Konten selain `apps` tidak boleh dipaksa masuk ke dock.

## 13. Prinsip Motion Controller

Semua motion harus mengikuti pemisahan ini:

- `state change` -> motion controller untuk perubahan spatial
- `mode change` -> motion controller untuk perubahan konten

Jangan mengikat keduanya pada satu layer visual jika itu membuat animasi menjadi ambigu.

## 14. Istilah Yang Tidak Boleh Dipakai

Hindari istilah lama. Semua diskusi harus kembali ke `state` dan `mode`.

## 15. Checklist Anti-Gagal

Sebelum implementasi dianggap selesai, pastikan:

- Tidak ada mode di dock selain `apps`.
- Semua logic UI berbasis `state` dan `mode`.
- Semua transisi mengikuti daftar resmi.
- Tidak ada hardcode layout di luar state.
- Dock tidak pernah dipakai untuk `pulse` atau `context`.

## 16. Tujuan Akhir

Target sistem ini:

- satu surface yang konsisten
- state machine yang jelas
- motion yang unified
- arsitektur yang scalable
- UX yang smooth, predictable, dan premium

Kalimat pegangan:

```text
AppDeck adalah satu surface utama.
State mengatur bentuk.
Mode mengatur isi.
Dock hanya untuk apps.
```
