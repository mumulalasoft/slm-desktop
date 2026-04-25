# Greeter To Desktop Test Architecture

Last updated: 2026-04-24

## Goal

Menyiapkan arsitektur pengujian end-to-end untuk flow:

```text
greetd -> slm-greeter -> slm-session-broker -> compositor -> slm-shell -> watchdog -> desktop healthy
```

Target dokumen ini bukan hanya "ada smoke test", tetapi lane test yang:

- bisa dijalankan bertahap dari lokal sampai QEMU
- jelas membedakan kegagalan wiring, login, session launch, dan desktop health
- menghasilkan artefak yang cukup untuk triage
- tidak memaksa semua validasi bergantung pada satu test UI yang rapuh

## Existing Assets

Komponen runtime yang sudah ada:

- `slm-greeter`
- `slm-session-broker`
- `slm-watchdog`
- `desktopd`
- `appSlm_Desktop` / `slm-shell`
- install/verify scripts di `scripts/login/*`

Tes/guard yang sudah relevan:

- `tests/sessionbroker_recovery_rollback_test.cpp`
- `tests/slmsessionstate_io_test.cpp`
- `scripts/login/verify-greetd-slm.sh`
- `scripts/login/verify-slm-desktop-runtime.sh`

Gap saat ini:

- belum ada lane yang benar-benar memverifikasi transisi "greeter sampai desktop healthy"
- belum ada harness terstandar untuk mengumpulkan bukti bahwa watchdog menandai session healthy
- belum ada pembagian lane yang memisahkan:
  - wiring install/runtime
  - greeter auth/session handoff
  - compositor/shell startup
  - desktop ready/healthy state

## Test Pyramid

Arsitektur yang disarankan dibagi menjadi 5 lapisan.

### L0. Pure Unit / State Logic

Tujuan:
- memverifikasi state machine dan rollback logic tanpa UI atau proses eksternal

Cakupan:
- `SessionState`
- `ConfigManager`
- `evaluateMode()`
- delayed commit / `configPending`
- watchdog promotion ke `last_good_snapshot`

Status:
- sebagian sudah ada

Kriteria:
- tidak butuh greetd
- tidak butuh Wayland
- jalan di CI default

### L1. Process Contract Tests

Tujuan:
- memverifikasi kontrak antarkomponen login stack tanpa boot desktop penuh

Harness:
- fake greetd socket server
- fake compositor executable
- fake shell executable
- fake watchdog executable

Cakupan:
- greeter kirim `create_session` / auth / `start_session`
- broker memilih mode yang benar
- broker mengekspor env yang benar
- broker menunggu `wayland-0`
- broker gagal dengan reason yang bisa diobservasi

Output wajib:
- stdout/stderr per proses
- file state/config hasil transisi
- exit code terstruktur

### L2. Runtime Wiring Smoke

Tujuan:
- memverifikasi artefak runtime sistem terpasang dengan benar

Sumber:
- `scripts/login/verify-greetd-slm.sh`
- `scripts/login/verify-slm-desktop-runtime.sh`

Peran:
- gate sebelum mencoba E2E QEMU
- menangkap failure murah seperti:
  - binary tidak terpasang
  - `slm.desktop` salah
  - greetd belum aktif
  - service desktop pendukung belum ada

Kriteria:
- tidak menyatakan desktop healthy
- hanya menyatakan runtime "wired"

### L3. QEMU Session Smoke

Tujuan:
- memverifikasi satu boot guest menghasilkan desktop session yang benar-benar hidup

Environment:
- QEMU Ubuntu guest
- boot ke installed disk, tanpa ISO
- runtime SLM sudah terpasang di guest

Assertion minimum:
- greetd aktif
- greeter process muncul
- login memulai `slm-session-broker`
- compositor dan shell start
- watchdog menulis `lastBootStatus=healthy`
- `configPending=false`

Oracle utama:
- `~/.config/slm-desktop/state.json`
- journal `greetd`
- journal user service / shell logs
- keberadaan socket Wayland

Catatan:
- lane ini tidak perlu otomatisasi klik UI dulu
- cukup boleh memakai "autologin test mode" atau fake auth path untuk membuktikan wiring session

### L4. Full Greeting To Desktop Acceptance

Tujuan:
- memverifikasi alur pengguna sesungguhnya: greeter tampil, user login, desktop siap

Environment:
- QEMU guest
- input automation untuk greeter
- screenshot/log capture

Assertion:
- greeter tampil
- username/password terisi
- login success
- desktop shell window muncul
- health marker tercapai dalam SLA waktu tertentu

Lane ini paling mahal dan paling flake-prone, jadi harus jadi lane nightly/manual-promote, bukan gate setiap commit.

## Recommended Lane Split

### Lane A: `login-state`

Isi:
- unit test login state/config/watchdog

Blokir:
- regression logic session/recovery

Frekuensi:
- per commit

### Lane B: `login-contract`

Isi:
- fake greetd + fake compositor/shell contract tests

Blokir:
- perubahan broker/greeter contract

Frekuensi:
- per commit atau pre-merge

### Lane C: `login-runtime-smoke`

Isi:
- verify script install/runtime

Blokir:
- packaging/runtime wiring

Frekuensi:
- pre-merge, release candidate

### Lane D: `login-qemu-session-smoke`

Isi:
- boot QEMU, login test mode / scripted auth, cek state healthy

Blokir:
- nightly
- release gate

### Lane E: `login-qemu-acceptance`

Isi:
- UI interaction greeter sampai desktop

Blokir:
- nightly atau manual validation

## Core Design Decision

Jangan membuat satu test UI besar sebagai satu-satunya bukti keberhasilan.

Sebaliknya, gunakan tiga oracle paralel:

1. Process oracle
   - proses `slm-greeter`, `slm-session-broker`, compositor, shell, watchdog benar-benar berjalan
2. State oracle
   - `state.json` dan file config menunjukkan transisi expected
3. UX oracle
   - screenshot/window evidence bahwa greeter dan desktop benar-benar tampil

Kalau salah satu gagal, triage jadi jauh lebih cepat.

## Proposed QEMU Harness

Untuk lane QEMU, harness dibagi menjadi empat tahap.

### Stage 1. Provision

Host:
- `dev/qemu-run.sh`
- `dev/qemu-install-deps-remote.sh`
- `dev/qemu-build-remote.sh`

Guest:
- runtime SLM terpasang
- greetd dikonfigurasi
- user test tersedia

### Stage 2. Trigger

Pilihan bertahap:

1. Session smoke mode
   - bypass auth manual
   - broker dijalankan dengan test mode / scripted command
2. Greeter automation mode
   - kirim keyboard input ke guest
   - login dari greeter sungguhan

Rekomendasi:
- implementasikan session smoke dulu
- baru acceptance UI setelah oracle state/process stabil

### Stage 3. Observe

Artefak yang harus dikumpulkan:

- `journalctl -b -u greetd --no-pager`
- `journalctl --user -b --no-pager`
- `~/.config/slm-desktop/state.json`
- screenshot greeter
- screenshot desktop
- daftar proses:
  - `slm-greeter`
  - `slm-session-broker`
  - compositor
  - `slm-shell`
  - `slm-watchdog`

### Stage 4. Decide

Pass minimal untuk lane smoke:

- broker start
- compositor ready
- shell start
- watchdog marks healthy

Pass minimal untuk acceptance:

- greeter visible
- login interaction success
- desktop visible
- healthy within timeout

## Data Contracts To Assert

### `state.json`

Field yang wajib dicek:

- `last_mode`
- `crash_count`
- `config_pending`
- `last_boot_status`
- `last_good_snapshot`

Expected success path:

```json
{
  "last_mode": "normal",
  "config_pending": false,
  "last_boot_status": "healthy"
}
```

### Runtime files

Wajib ada:

- `/usr/share/wayland-sessions/slm.desktop`
- `/etc/greetd/config.toml`
- `slm-session-broker` di path install

### Process contracts

Urutan minimum:

1. `greetd`
2. `slm-greeter`
3. `slm-session-broker`
4. compositor
5. `slm-shell`
6. `slm-watchdog`

## Failure Taxonomy

Agar hasil test langsung actionable, klasifikasikan error ke kategori berikut:

- `install_wiring_failure`
  - binary, session file, atau greetd config salah
- `greeter_connect_failure`
  - `GREETD_SOCK` / IPC greetd gagal
- `auth_failure`
  - kredensial atau alur auth salah
- `broker_mode_failure`
  - broker memilih safe/recovery tak sesuai ekspektasi
- `compositor_launch_failure`
  - compositor tidak start atau `wayland-0` tidak muncul
- `shell_launch_failure`
  - shell tidak start
- `watchdog_health_failure`
  - desktop start tapi tidak pernah healthy
- `desktop_visible_failure`
  - session jalan tapi UI acceptance gagal

## Implementation Plan

### Phase 1

Tambahkan lane contract-level tanpa QEMU:

- fake greetd server test
- fake compositor/shell/watchdog
- assert `state.json` transisi healthy/crash

Deliverable:
- test binary baru untuk `greeter + broker contract`

### Phase 2

Tambahkan QEMU smoke runner:

- deploy build ke guest
- install runtime
- jalankan session smoke
- collect logs + state

Deliverable:
- script `scripts/login/qemu-session-smoke.sh`

### Phase 3

Tambahkan acceptance lane:

- greeter input automation
- screenshot before/after login
- desktop ready proof

Deliverable:
- script `scripts/login/qemu-greeter-to-desktop.sh`

## Suggested Repository Structure

```text
scripts/login/
  qemu-session-smoke.sh
  qemu-greeter-to-desktop.sh
  collect-login-artifacts.sh

tests/login/
  fake_greetd_server.h/.cpp
  greeter_broker_contract_test.cpp
  broker_startup_contract_test.cpp

docs/
  GREETER_TO_DESKTOP_TEST_ARCHITECTURE.md
```

## Success Criteria

Arsitektur ini dianggap siap bila:

- commit-level lane bisa menangkap regression broker/greeter tanpa QEMU
- nightly lane bisa membuktikan satu boot QEMU mencapai `healthy`
- acceptance lane bisa menunjukkan login dari greeter ke desktop
- setiap kegagalan menghasilkan kategori failure + artefak yang cukup untuk triage

## Immediate Next Step

Implementasi pertama yang paling tepat:

1. tambahkan contract test untuk broker startup dengan fake compositor/shell/watchdog
2. tambahkan script QEMU smoke yang hanya mengecek `state.json -> healthy`
3. baru lanjut ke automation greeter UI

Urutan ini menjaga rasio signal-to-flake tetap tinggi.
