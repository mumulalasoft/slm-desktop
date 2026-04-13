# SLM Desktop TODO

> Canonical session summary: `docs/SESSION_STATE.md`

## Fokus Aktif (Prioritas Tinggi)

### 1) Storage & Automount
- [ ] QA cold reboot penuh (bukan hanya restart service) untuk persistence policy mount/automount.
- [ ] QA manual lintas kondisi media: locked, busy, unsupported FS, burst multi-partition.
- [ ] Verifikasi akhir UX FileManager untuk volume attach/detach cepat (no duplicate row, no stale mount state).

### 2) Startup Performance Desktop
- [ ] Turunkan waktu `qml.load.begin -> main.onCompleted` dengan profiling terarah per komponen.
- [x] Pertahankan target UX saat ini: shell cepat tampil, komponen non-kritis deferred.
- [x] Tambah guard regression sederhana (log phase + budget threshold) agar startup tidak melambat lagi.

### 3) Migrasi UI Preference -> settingsd (SSOT)
- [x] Fix regresi: perubahan tema dari Settings belum apply konsisten runtime.
- [x] Audit binding Theme/Appearance yang masih membaca sumber lama.
- [x] Tutup migrasi: pastikan seluruh preferensi UI utama membaca/menulis ke settingsd.

### 4) FileManager UX Stabilization
- [x] Pastikan aksi context menu `Open Drive` selalu mengganti current directory content view.
- [x] Final pass menu/submenu density + alignment consistency.
- [x] Tambah regression check untuk flow mount -> open -> browse.

---

## Program Network & Firewall (Roadmap Ringkas)

### Phase 1 (Wajib)
- [ ] Aktifkan incoming firewall default deny + loopback/established allow (nftables sebagai packet engine final).
- [ ] Implement block IP/subnet/list + temporary/permanent + hit counter + note.
- [ ] Integrasi identity mapping dasar via `desktop-appd` (PID -> app identity/context).
- [ ] UI Settings: Security -> Firewall (toggle, mode Home/Public/Custom, blocked IP/network, active connections).

### Phase 2
- [ ] Outgoing control (allow/block/prompt) berbasis app identity.
- [ ] Deteksi proses CLI/interpreter (python/node script target, parent/cwd/tty/cgroup).
- [ ] Popup policy yang kontekstual dan tidak spam.

### Phase 3
- [ ] Trust system (System/Trusted/Unknown/Suspicious) + policy adaptif.
- [ ] Behavior learning + auto suggestion.
- [ ] Quarantine mode untuk app mencurigakan.

---

## Program desktop-appd (Ringkas)
- [ ] Selesaikan skeleton service + DBus contract `org.desktop.Apps` minimal.
- [ ] Stabilkan lifecycle registry app-centric (bukan PID-centric).
- [ ] Implement exposure policy agar Dock/Switcher hanya menampilkan app yang valid UX.
- [ ] Tambah contract test untuk signal granular (`AppAdded/Removed/StateChanged/FocusChanged`).

## Program Notification (Ringkas)
- [ ] Finalisasi policy banner vs center + Do Not Disturb.
- [ ] Hardening multi-monitor behavior.
- [ ] Persistensi history + recovery state dasar.

## Program Polkit Agent (Ringkas)
- [ ] Finalisasi state machine multi-request + timeout/cancel path.
- [ ] Hardening security (no password logging, secure buffer wipe, restart-safe).
- [ ] Tambah integration test runtime session.

---

## Parking Lot (Tidak Aktif Sekarang)
- Struktur Dock: **dibekukan** pada state "last good" sampai siklus pengujian berikutnya.
- Rencana refactor arsitektur shell yang besar: lanjut setelah milestone stabilitas/firewall tercapai.
