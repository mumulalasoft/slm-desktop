# SLM Desktop TODO

> Canonical session summary: `docs/SESSION_STATE.md`

## Backlog (Tothespot)
- [ ] Add optional preview pane for non-compact popup mode.
- [x] Add provider health indicators with timeout/fallback status (exported via `tothespot.telemetryMeta().providerHealth` for UI/debug panel).
- [x] Add cold-start cache for empty-query popular results.

## Backlog (Theme)
- [ ] Elevation/shadow tokens:
  - unify popup/window shadow strength (`elevationLow/Medium/High`).
- [ ] Motion tokens:
  - replace ad-hoc durations/easing with shared animation tokens.
- [ ] Semantic control states:
  - define `controlBgHover/Pressed/Disabled`, `textDisabled`, `focusRing`.
- [ ] FileManager density pass:
  - align toolbar/search/tab/status heights to compact token grid.
- [ ] Add high-contrast mode toggle in `UIPreferences` + runtime Theme switch.
- [ ] Add reduced-motion mode toggle:
  - scale/disable non-essential transitions globally.
- [ ] Add optional dynamic wallpaper sampling tint with contrast guardrail.

## Backlog (FileManager)
- [ ] Continue shrinking `FileManagerWindow.qml` by moving remaining dialog/menu glue into `Qml/apps/filemanager/`.
- [x] Folder Sharing UX (non-teknis, task-oriented):
  - [x] Entry point: context menu `Bagikan…` (folder), Properties tab `Sharing`, info status.
  - [x] Dialog sederhana: toggle ON/OFF, nama folder jaringan, akses, izin, guest access.
  - [x] Success state + aksi cepat: salin alamat, cara akses, hentikan berbagi.
  - [x] Badge/status folder dibagikan di tampilan file manager.
  - [x] Backend bridge SMB otomatis (best-effort), tanpa expose istilah teknis di UI.

## Backlog (Global Menu)
- [ ] Runtime integration check global menu end-to-end on active GTK/Qt/KDE apps (verify active binding/top-level changes by focus switching).
  - helper available: `scripts/smoke-globalmenu-focus.sh` (`--strict --min-unique 2`).

## Backlog (Clipboard)
- [x] Integrate native `wl-data-control` low-level path for Wayland clipboard watching (event-driven primary path, keep Qt/X11 fallback, with backend-mode observability + fallback test coverage).

## Program (Solid & Unbreakable)
- [x] Unify missing dependency API across apps via global `MissingComponents` controller.
- [x] Migrate greeter/recovery QML to use global `MissingComponents` endpoint (domain-scoped install).
- [x] Remove legacy duplicate missing-component APIs from `GreeterApp` and `RecoveryApp`.
- [x] Add dependency severity metadata (`required` / `recommended`) in component checks.
- [x] Add blocking gate APIs:
  - `MissingComponents.blockingMissingComponentsForDomain(domain)`
  - `MissingComponents.hasBlockingMissingForDomain(domain)`
- [x] Add contract test baseline for missing-component controller domain guard + blocking API consistency (`missingcomponentcontroller_test`).
- [x] Enforce startup degraded-mode gate by severity:
  - [x] Greeter: block login submit while `required` session components missing.
  - [x] FileManager Sharing: only `required` issues block readiness; `recommended` stays non-blocking.
  - [x] Expand gate coverage to all fragile flows (print/network/bluetooth/portal actions).
- [~] Add end-to-end regression tests:
  - [x] Core baseline: missing dependency -> install pipeline (mocked pkexec/pkg manager) -> recheck -> feature restored (`missingcomponentcontroller_test`).
  - [ ] UI flow end-to-end: missing dependency -> warning UI -> install via polkit -> recheck -> feature restored.
- [ ] Add desktop health daemon + structured reason codes and persistent timeline.
- [ ] Add automatic config rollback on crash-loop (last-known-good snapshot).

## Next (Portal Adapter hardening)
- [ ] Phase-5 InputCapture:
  - [ ] Bind provider to real compositor pointer-capture/barrier primitives (currently stateful contract + optional command hook only).
- [ ] Phase-6 Privacy/data portals:
  - [ ] Camera
  - [ ] Location
  - [ ] PIM metadata/resolution portals (contacts/calendar/mail metadata first, body last)
- [ ] Phase-7 Hardening & interop:
  - [ ] DBus contract/regression suite GTK/Qt/Electron/Flatpak
  - [x] Race/cancellation/multi-session stress tests (`portal_adapter_race_cancellation_test.cpp` — 15 cases: double-close/revoke no-op, rapid create/destroy N=20, cancellation ordering, operation on terminal state, revocation mid-stream, multi-session isolation, manager signals)

## Next (Permission Foundation)
- [x] Integrate `DBusSecurityGuard` into live service endpoints: clipboard, desktopd, inputcapture, screencast, globalmenu, sessionstate, slmcapability, foldersharing.
- [x] Add consent mediation contract for `AskUser` decisions: `ConsentMediator` (async requestConsent/fulfillConsent, timeout auto-deny, AllowAlways/DenyAlways auto-persist to PermissionStore).
- [x] Add tests:
  - [x] trust resolver classification (`trustresolver_test.cpp` — 12 test cases)
  - [x] policy defaults per trust level (`policyengine_test.cpp`)
  - [x] gesture-gated capability checks (`policyengine_test.cpp`)
  - [x] permission persistence/audit writes (`policy_integration_test.cpp` — stored override, audit round-trip, ConsentMediator fulfill/persist/timeout/cancel)

---

## Inisiatif: Polkit Authentication Agent (SLM Desktop)

> **Konteks:** Kembangkan polkit authentication agent milik desktop sendiri — bukan bergantung pada agent desktop lain. Fokus: stabil, aman, ringan, mudah diuji, terintegrasi rapi dengan session desktop. Core agent dipisah dari shell utama agar crash agent tidak menjatuhkan desktop shell. Target runtime Linux modern dengan systemd user session.

### Tujuan

1. Membuat Polkit authentication agent resmi milik desktop.
2. Agent berjalan otomatis saat user login ke session grafis.
3. Agent mendaftarkan diri sebagai session authentication agent Polkit.
4. Saat ada request autentikasi, agent menampilkan dialog native desktop.
5. Dialog mendukung: pesan autentikasi, identity/admin target, input password, tombol cancel, state loading, state password salah.
6. Agent aman terhadap: multi request, cancel, timeout, request source mati di tengah proses, restart agent, session lock.
7. Gunakan systemd user service untuk lifecycle.
8. Rancang agar nantinya bisa diintegrasikan dengan lockscreen dan shell desktop.

### Kebutuhan teknis

- Gunakan Polkit API resmi: `libpolkit-agent-1` + `libpolkit-gobject-1`.
- Authority tetap dikelola `polkitd`; agent hanya menangani registration, authentication flow orchestration, dan UI.
- Jangan memindahkan responsibility authorization decision ke agent.
- C++ untuk core agent, QML untuk UI.
- Signal/slot atau pola event-driven yang rapi.
- Hindari ketergantungan yang tidak perlu.
- Single-instance per session.
- Keamanan password: jangan log password, segera bersihkan buffer, minimalkan jejak data sensitif.

### Output yang dibutuhkan

1. Ringkasan arsitektur tingkat tinggi
2. Tahapan implementasi per fase
3. Struktur direktori proyek
4. Daftar class/module utama
5. State machine autentikasi
6. Integrasi systemd user service
7. Checklist security hardening
8. Checklist testing
9. Risiko implementasi yang perlu diwaspadai
10. Saran urutan pengerjaan paling efisien

### Preferensi desain

- Dialog autentikasi harus terasa seperti komponen sistem, bukan popup aplikasi biasa.
- Mudah di-theme sesuai desktop.
- Siapkan jalur untuk multi-monitor, HiDPI, accessibility, dark mode, localization.
- Siapkan kemungkinan fallback minimal jika UI shell belum siap.
- Agent memberi tahu shell bahwa dialog autentikasi sedang aktif, tanpa coupling berlebihan.

### Roadmap implementasi

#### Fase 1 — MVP: Agent registration + dialog minimal

- [x] Buat binary `slm-polkit-agent` (C++ Qt, bootstrap single-instance daemon tanpa QML dulu)
- [x] Integrasikan `libpolkit-agent-1`: register listener ke session (custom `PolkitAgentListener` aktif)
- [x] Implementasi `AuthSession` wrapper: mulai, cancel, respond password
- [x] Dialog QML minimal: label aksi, label identity, password field, Cancel, OK
- [x] Wiring C++ ↔ QML via `AuthDialogController` QObject
- [x] Systemd user service: `slm-polkit-agent.service`
- [x] Test manual: `pkexec ls /root` dari terminal → dialog muncul

#### Fase 2 — Stabilisasi: multi-request, error handling, state machine

- [ ] State machine lengkap: `Idle → Pending → Authenticating → Success/Failed/Cancelled`
- [ ] Queue multi-request: satu dialog aktif, sisanya antri
- [ ] Handle: cancel dari user, cancel dari caller (request source mati), timeout
- [ ] State `PasswordWrong`: shake animation + pesan error, tanpa clear password
- [ ] State `Loading`: spinner, disable input
- [ ] Auto-cancel jika caller process tidak ada lagi (poll/DBus watch)
- [ ] Restart handling: agent mati → systemd restart → re-register ke polkit

#### Fase 3 — Integrasi shell + hardening

- [ ] Notifikasi ke shell: emit DBus signal `AuthDialogActive(bool)` → shell bisa dim/blur atau lock input
- [ ] Integrasi dengan lockscreen: jika session terkunci saat auth request masuk → queue sampai unlock
- [ ] Security hardening: clear password buffer setelah AuthSession selesai, no-logging guard, input sanitization
- [ ] Accessibility: label ARIA, keyboard navigation, focus management
- [ ] Multi-monitor: dialog muncul di monitor aktif/focused
- [ ] HiDPI: image provider icon scaled, layout fluid

#### Fase 4 — Polish + production hardening

- [ ] Unit test: AuthSessionManager queue, state machine transitions, cancel/timeout paths
- [ ] Integration test: mock polkit authority, verify agent registration + response
- [ ] Localization: string extraction, i18n-ready QML
- [ ] Dark mode: dialog ikut tema desktop
- [ ] Audit log: catat request masuk/keluar (tanpa password), untuk observability
- [ ] Fallback mode: jika QML engine gagal load, tampilkan dialog via QWidget minimal

### Struktur direktori yang diusulkan

```
src/
  polkit-agent/
    main.cpp                      # entry point, single-instance guard
    polkitagent.cpp/h             # PolkitAgentListener subclass (GLib bridge)
    authsessionmanager.cpp/h      # queue, lifecycle, timeout management
    authsession.cpp/h             # satu authentication session
    authdialogcontroller.cpp/h    # QObject jembatan C++ ↔ QML
    dbus/
      agentnotifier.cpp/h         # emit DBus signal AuthDialogActive ke shell
Qml/
  polkit-agent/
    AuthDialog.qml                # dialog utama
    PasswordField.qml             # field password + shake animation
    AuthIdentityView.qml          # tampilan identity (ikon, nama, realm)
systemd/
  slm-polkit-agent.service        # user service unit
```

### Class utama C++

| Class | Tanggung jawab |
|---|---|
| `SlmPolkitAgent` | Subclass `PolkitAgentListener`, register/unregister ke polkit, terima `InitiateAuthentication` callback |
| `AuthSessionManager` | Queue request, buat/destroy `AuthSession`, enforce single-dialog-at-a-time |
| `AuthSession` | Wrap `PolkitAgentSession` GLib object, expose signal Qt: `showError`, `showInfo`, `completed` |
| `AuthDialogController` | QObject yang di-expose ke QML: property `message`, `identity`, `actionId`; invokable `authenticate(password)`, `cancel()` |
| `AgentNotifier` | Publish `org.slm.desktop.AuthAgent` DBus interface: signal `AuthDialogActive(bool)` |

### State machine autentikasi

```
[Idle]
  │ InitiateAuthentication received
  ▼
[Pending]           ← request masuk queue jika dialog sudah aktif
  │ dialog ready, user siap
  ▼
[Authenticating]    ← password di-submit ke PolkitAgentSession
  │
  ├─ Success   → emit Completed(true)  → [Idle]
  ├─ Failed    → [PasswordWrong]       → kembali ke [Authenticating] atau [Cancelled]
  ├─ Cancelled → emit Completed(false) → [Idle]
  └─ Timeout   → emit Completed(false) → [Idle]

[PasswordWrong]
  │ retry atau cancel
  ├─ retry  → [Authenticating]
  └─ cancel → [Cancelled] → [Idle]
```

### Systemd user service

```ini
# ~/.config/systemd/user/slm-polkit-agent.service
[Unit]
Description=SLM Desktop Polkit Authentication Agent
PartOf=graphical-session.target
After=graphical-session.target
ConditionEnvironment=WAYLAND_DISPLAY

[Service]
ExecStart=/usr/libexec/slm-polkit-agent
Restart=on-failure
RestartSec=2s
TimeoutStopSec=5s

[Install]
WantedBy=graphical-session.target
```

### Security hardening checklist

- [ ] Password tidak pernah masuk log (journal, qDebug, stderr)
- [ ] Buffer password di-zero setelah `AuthSession` selesai (ZeroMemory / memset_s / `SecureZeroMemory`)
- [ ] Tidak ada core dump yang berisi password (set `RLIMIT_CORE=0` di main)
- [ ] Single-instance guard via `QLocalServer` atau lock file
- [ ] Verifikasi caller: pastikan `PolkitSubject` valid sebelum tampilkan dialog
- [ ] Tidak expose detail internal error ke user (tampilkan pesan generik)
- [ ] Dialog tidak bisa di-dismiss tanpa explicit cancel (tidak bisa klik luar dialog)
- [ ] Timeout auto-cancel jika tidak ada respons user dalam N detik

### Testing checklist

- [ ] Unit: `AuthSessionManager` queue — request masuk saat dialog aktif masuk antri dengan benar
- [ ] Unit: state machine — semua transisi valid, transisi invalid di-reject
- [ ] Unit: timeout — session auto-cancel setelah timeout
- [ ] Unit: cancel dari caller — process mati → session cleanup tanpa crash
- [ ] Integration: `pkexec ls /root` → dialog muncul → password benar → sukses
- [ ] Integration: password salah → state `PasswordWrong` → retry → sukses
- [ ] Integration: cancel → proses `pkexec` exit dengan kode error
- [ ] Integration: dua request berurutan → keduanya selesai dengan benar
- [ ] Integration: agent restart di tengah session → request berikutnya OK
- [ ] Manual: dialog muncul di monitor yang sedang digunakan (multi-monitor)
- [ ] Manual: HiDPI — icon dan font tidak blur
- [ ] Manual: dark mode — dialog mengikuti tema desktop

### Risiko implementasi

| Risiko | Mitigasi |
|---|---|
| GLib event loop bentrok dengan Qt event loop | Gunakan `QEventLoop` + GLib integration atau run GLib di thread terpisah |
| `PolkitAgentListener` adalah GObject — interop C++ ribet | Buat thin C wrapper atau gunakan `gio-qt` bridge |
| Agent crash → semua `pkexec` hang | Systemd restart cepat (2s), polkit timeout sendiri setelah ~30s |
| Race condition multi-request | Queue dengan mutex, satu dialog aktif |
| Memory leak buffer password | RAII wrapper + explicit zero di destructor |
| Dialog muncul di belakang fullscreen app | Pastikan dialog memiliki `Qt::WindowStaysOnTopHint` atau protocol Wayland `xdg_activation` |

### Saran urutan pengerjaan

1. Buat `slm-polkit-agent` binary minimal: register ke polkit, print ke stdout saat ada request (belum ada dialog)
2. Tambah `AuthSession` + state machine sederhana
3. Buat `AuthDialogController` + dialog QML minimal (tanpa animasi)
4. Wire end-to-end: `pkexec` → dialog → password → sukses/gagal
5. Tambah queue multi-request + timeout
6. Tambah `AgentNotifier` DBus signal ke shell
7. Systemd user service + autostart
8. Polish: animasi, dark mode, accessibility, localization
9. Testing suite lengkap
10. Security hardening pass final

---

## Inisiatif: Login/Session Stack (SLM Desktop Foundation)

> **Konteks:** Anda adalah software architect dan engineer senior Linux desktop stack. Tugas Anda adalah merancang dan mengimplementasikan fondasi login/session stack untuk desktop Linux modern bernama **SLM Desktop**.

### Tujuan utama

Bangun arsitektur desktop yang **solid, opinionated, recoverable, dan self-healing**. Desktop tidak boleh mudah rusak hanya karena user mengganti konfigurasi. Jika konfigurasi baru menyebabkan crash loop, desktop harus bisa **mendeteksi, rollback, lalu masuk ke safe mode atau recovery mode**.

Target desain:

* desktop menentukan stack inti secara jelas
* login flow dikendalikan platform
* recovery dimulai sejak sebelum shell utama dijalankan
* konfigurasi desktop bersifat transactional
* crash loop harus bisa dideteksi dan dipulihkan otomatis
* user tetap punya jalur masuk aman meskipun sesi normal rusak

---

### Keputusan arsitektur yang wajib diikuti

#### 1. Jangan membuat display manager penuh sendiri

Gunakan **greetd** sebagai backend login daemon.

#### 2. SLM memiliki komponen sendiri di atas greetd

Implementasikan komponen berikut:

* `slm-greeter` → greeter grafis Qt/QML
* `slm-session-broker` → broker sesi resmi
* `slm-watchdog` → pemantau health sesi user
* `slm-recovery-app` → aplikasi pemulihan minimal
* `slm-configd` atau `ConfigManager` → pengelola konfigurasi transactional
* session desktop resmi SLM

#### 3. greetd tidak boleh langsung menjalankan shell desktop

Alur wajib:

* `greetd` → `slm-greeter` → autentikasi user → pilih mode startup
* → `slm-session-broker` → validasi state → tentukan mode final
* → compositor + shell + daemon inti

#### 4. Desktop harus opinionated

Komponen inti dianggap **required components**:

* greetd backend login
* slm-greeter
* slm-session-broker
* compositor resmi atau daftar compositor resmi
* slm shell
* settings daemon
* policy agent
* portal backend
* notification daemon
* recovery/watchdog service

Komponen inti ini tidak dianggap bebas tukar. Sistem harus bisa mendeteksi bila state platform tidak sesuai.

---

### Mode sesi yang wajib ada

#### A. Normal Session

Mode biasa: tema aktif, plugin resmi aktif, layout user aktif, animasi aktif.

#### B. Safe Session

Dipakai jika crash loop atau dipilih user:

* tema custom dimatikan
* plugin/extension non-esensial dimatikan
* panel/layout default, wallpaper default, animasi berat dimatikan
* konfigurasi berisiko diabaikan

#### C. Recovery Session

Dipakai untuk diagnosis:

* shell minimal atau recovery shell
* hanya UI pemulihan dasar: reset settings, disable plugin, restore snapshot, log viewer dasar

#### D. Factory Reset Action

Bukan menghapus data user — hanya reset konfigurasi desktop ke default aman.

---

### Prinsip recovery yang wajib

#### 1. Transactional config

Model wajib:

* current config, previous config, safe/default config, optional versioned snapshots

#### 2. Delayed commit

* config baru diterapkan → sistem menunggu satu periode health check
* jika sesi tetap sehat → config ditandai `good`
* jika crash sebelum stabil → rollback ke snapshot sebelumnya

#### 3. Atomic write

* tulis ke file sementara → fsync → rename

#### 4. Crash loop detection

* increment `crash_count` setiap crash pada startup awal
* jika melebihi threshold → rollback ke last known good → safe/recovery mode

#### 5. Recovery sejak login

Greeter harus menyediakan opsi: Start Desktop / Start in Safe Mode / Start Recovery / Power actions.

---

### State dan penyimpanan

Direktori: `~/.config/slm-desktop/`

File:

* `config.json` — konfigurasi aktif
* `config.prev.json` — snapshot sebelumnya
* `config.safe.json` — baseline aman / default
* `state.json` — runtime state: `crash_count`, `last_mode`, `last_good_snapshot`, `safe_mode_forced`, `recovery_reason`, `last_boot_status`
* `snapshots/` — versi tambahan

Broker membaca `state.json` pada setiap startup.

---

### Tanggung jawab tiap komponen

#### slm-greeter (Qt/QML)

* daftar user / input password
* pilih mode: normal / safe / recovery
* tombol power + branding SLM
* tampilkan pesan: "desktop dipulihkan dari crash", "konfigurasi terakhir dibatalkan", "safe mode aktif"

#### slm-session-broker (C++ Qt Core atau Rust)

* terima mode startup dari greeter
* baca `state.json`
* periksa integritas platform
* validasi config
* putuskan normal/safe/recovery
* lakukan rollback jika perlu
* siapkan environment sesi → launch compositor + shell
* catat status ke journal/log

#### slm-watchdog (C++ atau Rust)

* pantau apakah shell/compositor berhasil stabil
* jika crash sebelum healthy → catat startup failure + tulis state crash
* tandai startup sukses jika sesi bertahan cukup lama

#### slm-recovery-app (Qt/QML)

* reset theme, panel, layout monitor
* disable plugin
* restore default / restore snapshot
* tampilkan ringkasan error/log

#### ConfigManager (shared library)

* get/set/validate config (schema-aware)
* snapshot current → promote → rollback ke previous / safe / default
* atomic save

---

### Platform integrity check

Yang harus dicek:

* session yang dijalankan memang SLM
* broker dijalankan melalui session resmi
* compositor termasuk daftar resmi
* file config valid, schema version cocok
* komponen inti tersedia

Jika gagal: jangan jalankan sesi normal → masuk safe/recovery mode dengan alasan yang jelas.

---

### Struktur proyek yang diusulkan

```
src/
  login/
    greeter/          # slm-greeter (Qt/QML)
    session-broker/   # slm-session-broker
    watchdog/         # slm-watchdog
    recovery-app/     # slm-recovery-app
    libslmconfig/     # shared ConfigManager + state lib
sessions/
  slm.desktop         # session resmi → Exec=slm-session-broker
scripts/
  install-session.sh
```

---

### Integrasi greetd

* `greetd.toml` → `command = "slm-greeter"`
* Session resmi: `slm.desktop` → `Exec=/usr/libexec/slm-session-broker`
* Mode dipilih greeter → dikirim ke broker via argumen atau environment flag
* **Satu session resmi**, mode ditentukan greeter + broker — bukan banyak session terpisah

---

### Preferensi teknologi

* `slm-greeter`, `slm-recovery-app` → Qt/QML
* `slm-session-broker` → C++ Qt Core (atau Rust)
* `slm-watchdog` → C++ atau Rust
* Linux Wayland-first
* systemd/journald untuk logging dan watchdog integration

---

### Roadmap implementasi

#### Fase 1 — MVP

- [x] Rancang kontrak data: `state.json`, `config.json`, snapshot metadata, schema version
- [x] Implementasi `ConfigManager` (atomic write, rollback, snapshot)
- [x] Skeleton `slm-session-broker`: baca state, putuskan mode, launch compositor+shell
- [x] Session file `slm.desktop` → broker
- [x] Integrasi greetd dasar dengan greeter placeholder

#### Fase 2 — Stabilisasi

- [x] `slm-greeter` Qt/QML: login UI, pilih mode, pesan recovery
- [x] `slm-watchdog`: deteksi crash loop, tulis `crash_count`, mark healthy
- [x] Crash loop threshold → auto safe mode
- [x] Rollback otomatis ke `last_good_snapshot`

#### Fase 3 — Recovery hardening

- [x] `slm-recovery-app`: reset to safe defaults, rollback to previous, restore snapshot, log viewer
- [x] Platform integrity check hardening: required service binaries + XDG_RUNTIME_DIR
- [x] Delayed commit + health confirmation (`configPending` flag, broker + watchdog)
- [x] Recovery mode shell minimal (broker launches `slm-recovery-app` in Recovery mode)

#### Fase 4 — UX polish

- [x] Greeter: animasi minimal (fade-in 600ms, shake on login error), pesan kontekstual (configPending banner, safeModeForced banner, crash detail + count)
- [x] Factory reset config action (`ConfigManager::factoryReset()` + UI confirmation di recovery app)
- [x] Dokumentasi arsitektur final (`docs/architecture-login-stack.md`)
