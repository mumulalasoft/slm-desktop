# SLM Login Foundation Status (greetd + broker + recovery)

This document maps implementation status against the platform checklist.
Legend:
- `DONE (code)` implemented in repository and buildable.
- `DONE (ops)` requires root/system deployment.
- `PENDING` not completed yet.

## 1. Foundation
- `DONE (ops)` Install/activate greetd as DM: `scripts/login/install-greetd-slm.sh`.
- `DONE (ops)` Disable competing DM (gdm/sddm/lightdm): same script.
- `DONE (code+ops)` Session entry `/usr/share/wayland-sessions/slm.desktop` with `Exec=/usr/libexec/slm-session-broker`.
- `DONE (code)` `slm-session-broker` startup logging + mode arg parse + exit handling.
- `DONE (code)` Broker launches compositor + shell and validates `WAYLAND_DISPLAY` / `XDG_RUNTIME_DIR`.

## 2. Config System
- `DONE (code)` `ConfigManager::load/save/currentConfig`.
- `DONE (code)` snapshot files: `config.json`, `config.prev.json`, `config.safe.json`.
- `DONE (code)` atomic write upgraded to `QSaveFile` commit.
- `DONE (code)` schema validation + fallback chain: active -> safe -> prev -> default.

## 3. Recovery Engine
- `DONE (code)` `state.json` fields include `crash_count`, `last_mode`, `last_good_snapshot`.
- `DONE (code)` crash counter increment at startup, reset by watchdog when healthy.
- `DONE (code)` delayed commit (`configPending`) + rollback on unconfirmed previous session.
- `DONE (code)` rollback logic: previous/safe/snapshot/factory reset.
- `DONE (code)` crash loop detection threshold `>= 3`, forces recovery path.

## 4. Watchdog
- `DONE (code)` `slm-watchdog` process implemented.
- `DONE (code)` health criteria: wayland socket + shell process existence.
- `DONE (code)` writes healthy state and promotes last-known-good config.

## 5. Greeter
- `DONE (code)` `slm-greeter` Qt/QML UI with user/password.
- `DONE (code)` mode selector: Normal / Safe / Recovery.
- `DONE (code)` status feedback: crash/recovery/safe notices.
- `DONE (code)` greetd protocol integration via `GreetdClient`.

## 6. Session Modes
- `DONE (code)` normal mode.
- `DONE (code)` safe mode: broker sets mode and rollback to safe baseline.
- `DONE (code)` recovery mode: launches `slm-recovery-app`.
- `DONE (code)` factory reset action in recovery app.

## 7. Platform Integrity
- `DONE (code)` checks for official session, allowed compositor, required services, runtime dir.
- `DONE (code)` degraded fallback to safe/recovery from broker decision logic.

## 8. Packaging
- `PARTIAL` install rules for binaries/session file exist in `CMakeLists.txt`.
- `PENDING` distro meta-packages (`slm-desktop-base/session/greeter`) are packaging-layer work.

## 9. Testing
- `DONE (ops)` ops verification script: `scripts/login/verify-greetd-slm.sh`.
- `PARTIAL` existing runtime smoke tests, but dedicated login/recovery CI tests still need expansion.

## 10. Advanced
- `PENDING` plugin isolation, crash fingerprint, auto disable plugin.

## Quick Runbook
1. Build targets:
   - `cmake --build build/toppanel-Debug --target slm-session-broker slm-watchdog slm-greeter slm-recovery-app -j8`
2. Install full SLM desktop runtime (root):
   - `sudo bash scripts/login/install-slm-desktop-runtime.sh /path/to/build`
   - default build path: `build/toppanel-Debug`
3. Verify runtime install:
   - `sudo SLM_TARGET_USER=<user> bash scripts/login/verify-slm-desktop-runtime.sh`
4. Install/activate greetd stack (root):
   - `sudo bash scripts/login/install-greetd-slm.sh`
5. Verify deployment:
   - `sudo bash scripts/login/verify-greetd-slm.sh`
