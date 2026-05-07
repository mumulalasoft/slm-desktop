# SLM Session State

Last updated: 2026-05-08, Asia/Jakarta

## Current Git State

Current branch head:

- `5110aaa Implement secure LayerShell lockscreen flow`
- Previous architecture commit: `d608379 Implement deterministic LayerShell shell policy`

Known uncommitted local state that is intentionally not part of the last commits:

- `third_party/slm-style` is dirty as a submodule/worktree entry.
- `docs/dep.txt` is untracked/local.
- `log/` contains local runtime logs.
- `scripts/dev/kwin_only_debug.sh`, `scripts/dev/qemu-mount-hostshare.sh`, `scripts/dev/qemu-quick.sh`, and `scripts/dev/wayland_debug.sh` are untracked local helper/debug scripts.

## Stable State

### LayerShell System Surfaces

Critical shell surfaces have been moved toward a deterministic LayerShellQt architecture:

- Crown uses a dedicated OverlayLayer surface.
- AppDeck uses a persistent TopLayer surface.
- Notifications use a LayerShell overlay surface with compact input region behavior.
- Lockscreen uses a SecurityLayerShell OverlayLayer surface with exclusive keyboard input.

Runtime source scans are clean for these forbidden patterns:

- `WindowStaysOnTopHint`
- `requestActivate(`
- `.raise(`
- local unlock fallback
- `SessionStateClient.setLocked`

### Shell Policy

`ShellPolicyController` is implemented and exposed to QML.

Policy states:

- `Normal`
- `AppFullscreen`
- `ImmersiveFullscreen`
- `Presentation`
- `Locked`

Behavior now separates stacking from visibility policy:

- Layer assignments stay stable.
- Fullscreen policy hides/reveals Crown/AppDeck without recreating surfaces.
- Immersive fullscreen suppresses shell surfaces and notifications.
- Locked policy hides/suspends shell surfaces.

### Lockscreen

The lockscreen is no longer allowed to unlock locally from QML.

Current secure flow:

- UI calls `SessionStateClient.requestUnlock(password)`.
- `SessionStateClient` calls `org.slm.SessionState.Unlock`.
- `SessionStateService` owns unlock authority.
- `SessionAuthenticator` uses PAM when `SLM_HAVE_PAM=1`.
- QML cannot call `setLocked(false)` anymore.

Daemon lock state exists in `SessionStateService` and client state:

- `Active`
- `Locking`
- `Locked`
- `Unlocking`

Structured logs added or expected in current code path:

- `[LOCKD]`
- `[LOCK_STATE]`
- `[AUTH]`
- `[PAM]`
- `[INPUT_BLOCK]`
- `[LOCKSCREEN]`
- `[MONITOR]`

Multi-monitor coverage is improved:

- `LockScreenWindow` accepts `targetScreen`.
- Main lockscreen auth surface targets the primary/root screen.
- Secondary screens get additional `LockScreenWindow` instances via `Instantiator`.
- `AppDeckLayerShellController` now tracks LayerShell state per `QWindow`, so multiple lockscreen windows can be prepared by one controller.

### qemu-smoke SSH

`scripts/dev/qemu-smoke.sh` was fixed so SSH auth is clearer and less brittle:

- Uses `sshpass -e` when `SLM_QEMU_SSH_PASSWORD` is set and `sshpass` exists.
- Falls back to interactive TTY prompt when possible.
- Emits a specific error when password is set but `sshpass` is missing.

## Verification Already Run

Successful builds:

- `cmake --build build/icu78 -j4 --target desktopd slm-desktop sessionstateservice_dbus_test appdecksystem_render_contract_test`
- Follow-up `cmake --build build/icu78 -j4 --target slm-desktop appdecksystem_render_contract_test`
- `cmake --build build/icu78 -j4 --target desktopd`

Successful tests:

- `./build/icu78/appdecksystem_render_contract_test`
- `./build/icu78/sessionstateservice_dbus_test`

Notes:

- `sessionstateservice_dbus_test` binary runs, but individual DBus tests skip in the sandbox because no session bus is available.
- `dbus-run-session -- ./build/icu78/sessionstateservice_dbus_test` failed in this sandbox because `dbus-daemon` could not bind `/tmp/dbus-*`.
- QEMU smoke was not fully validated after lockscreen work because QEMU/SSH availability was inconsistent in this environment.

## Important Remaining Work

### 1. Runtime QEMU Validation

Run real QEMU/KWin smoke after guest SSH is fixed:

```bash
bash scripts/dev/qemu-smoke.sh
```

Validate in guest:

- Crown appears on OverlayLayer and does not vanish.
- AppDeck remains correctly positioned and above apps.
- Lock shortcut shows lockscreen immediately.
- Locked session blocks app input.
- Unlock only succeeds with valid PAM credentials.
- Failed unlock does not dismiss lockscreen.
- App fullscreen and immersive fullscreen do not break lockscreen.

### 2. Multi-Monitor Lockscreen Test

Add or run a QEMU/nested KWin multi-monitor scenario.

Expected:

- Every output has a lockscreen layer surface.
- No uncovered monitor.
- Keyboard focus remains owned by lockscreen.
- Secondary monitor surface does not create normal xdg_toplevel fallback.

### 3. Session Bus Integration Test Outside Sandbox

Run DBus tests in an environment where `dbus-run-session` can create a bus:

```bash
dbus-run-session -- ./build/icu78/sessionstateservice_dbus_test
```

Expected:

- `Ping` returns `lock_state=Active`.
- `GetCapabilities` includes `lock-state`.
- `LockStateChanged` transitions to `Locked` and back to `Active`.
- Failed unlock throttling still works.

### 4. PAM Runtime Validation

Run:

```bash
scripts/test-session-unlock.sh
```

or set:

```bash
SLM_TEST_UNLOCK_PASSWORD='<current-user-password>' ./build/icu78/sessionstateservice_dbus_test
```

Expected:

- Valid password unlocks.
- Invalid password stays locked.
- Five failures trigger rate limit.
- PAM unavailable returns a failure state, never unlock.

### 5. Screenshot/Input Policy While Locked

Current lockscreen blocks shell shortcuts through `ShellInputRouter`, but screenshot/app preview sanitization needs a dedicated runtime pass.

Next work:

- Block or sanitize screenshot requests while locked.
- Hide/blur workspace thumbnails and app previews while locked.
- Add tests that `ShellInputRouter` blocks screenshot actions during `LockScreen`.

### 6. UI Crash Failsafe

The current implementation prevents QML from unlocking without PAM, but a full `slm-lockd` watchdog/restart path is not fully separated as a standalone daemon.

Next work:

- Split/alias `desktopd` lock authority into explicit `slm-lockd` service or target.
- Add lockscreen UI crash detection.
- Restart lockscreen UI while keeping daemon state `Locked`.
- Add an emergency minimal lock UI path.

### 7. logind/Idle/Resume Integration

Next work:

- Validate suspend/resume flow.
- Ensure wake shows lockscreen before desktop content flashes.
- Integrate logind session lock state more explicitly.
- Add logs for resume and monitor re-map.

## Do Not Regress

Do not reintroduce:

- `Qt.WindowStaysOnTopHint` for shell-critical UI.
- `raise()` or `requestActivate()` restacking.
- normal QML top-level lockscreen as security path.
- local QML unlock fallback.
- `SessionStateClient.setLocked()` as a QML invokable.
- dynamic layer switching for Crown/AppDeck/Lockscreen.
- click-through lockscreen regions.

## Useful Commands

Build core targets:

```bash
cmake --build build/icu78 -j4 --target desktopd slm-desktop appdecksystem_render_contract_test sessionstateservice_dbus_test
```

Run contract tests:

```bash
./build/icu78/appdecksystem_render_contract_test
./build/icu78/sessionstateservice_dbus_test
```

Scan forbidden runtime patterns:

```bash
rg -n "_canUseLocalUnlockFallback|local unlock fallback|SessionStateClient\\.setLocked|Q_INVOKABLE void setLocked|WindowStaysOnTopHint|requestActivate\\(|\\.raise\\(" Main.qml Qml src -S
```

Check patch hygiene:

```bash
git diff --check
```
