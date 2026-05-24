# Crown Popup Contract

## Scope
Kontrak ini berlaku untuk semua popup dari Crown (main menu, indicator applet, screenshot, dan applet baru).

## Tujuan
- Popup muncul di bawah trigger icon (default).
- Popup tampil di atas aplikasi (`Popup.Window`).
- State popup Crown terisolasi dari state Global Menu desktop.

## Jalur Resmi (Single Path)
1. Trigger applet memanggil `open()` pada `CrownAnchoredPopup` atau `CrownAnchoredMenu`.
2. `onAboutToShow` mengirim request ke `CrownPopupController`.
3. `PopupHostLayer` memproses request serial terbaru, menerapkan geometry, dan menjaga one-active-popup invariant.
4. `onAboutToHide/onClosed` menutup state aktif di `CrownPopupController`.

## API Contract Untuk Applet
- Wajib gunakan `CrownAnchoredPopup` atau `CrownAnchoredMenu`.
- Wajib isi `appletId` yang stabil dan unik. Format rekomendasi: `crown.<area>.<name>`.
- Wajib pasang `anchorItem` ke item trigger yang valid.
- Wajib menerima `popupHost` dari `Crown` (jangan hardcode host sendiri).
- Jangan akses `CrownPopupController` langsung selain lewat komponen anchored.

## Positioning Contract
- Default vertikal: `anchorY + anchorHeight + popupGap` (di bawah trigger).
- Fallback vertikal: jika overflow monitor, host boleh pindah ke atas anchor.
- Horizontal alignment: `left`, `right`, atau `center` sesuai komponen anchored.

## Layering Contract
- Semua popup Crown harus `popupType = Popup.Window`.
- Popup tidak boleh bergantung pada `z` item scene desktop untuk berada di atas aplikasi.
- `PopupHostLayer` hanya mediator lifecycle + geometry; bukan source of truth global state.

## State Isolation Contract
- Crown popup state dikelola di `CrownPopupController`.
- Global Menu state dikelola di `CrownBrandSection` (`menuBarOpenId`) + `GlobalMenuManager`.
- Membuka/menutup popup crown tidak boleh mengubah `menuBarOpenId` secara implisit.
- Popup Crown tidak boleh memanggil API mutasi global menu kecuali aksi menu global memang dipicu user di komponen global menu.

## Safety Rules
- Jika popup user/app rusak, fallback hanya pada popup/controller crown, bukan memodifikasi state global menu.
- Saat applet baru ditambahkan, wajib lolos checklist:
  - `CrownAnchored*` dipakai.
  - `appletId` unik.
  - `popupHost` berasal dari `Crown`.
  - Tidak ada mutasi ke `GlobalMenuManager` di handler open/close popup.
