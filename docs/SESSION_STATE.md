# SLM Session State

Last updated: 2026-05-08, Asia/Jakarta

## Current Git State

Current branch head: `wip/next-from-main`, on top of `5110aaa Implement secure LayerShell lockscreen flow`.

In-flight (uncommitted on this branch) work covers items 5, 6, and 7 from the
"Important Remaining Work" list. See "Recent Iteration" below.

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

## Recent Iteration (in-flight, uncommitted)

This iteration tightens the lockscreen security boundary against three leak
paths: screenshot capture while locked, UI process crash, and suspend/resume.

### Item 5 — Hard block screenshots while locked

- `ShellInputRouter::actionAllowedInLayer` rejects any `screenshot.*` action
  when the active layer is `LockScreen`.
- `ScreenshotManager` gained `setLockedProvider(std::function<bool()>)`. All
  five public capture entry points (`capture`, `captureFullscreen`,
  `captureWindow`, `captureArea`, `captureAreaRect`) early-return
  `{ok:false, error:"session-locked"}` when the provider reports true.
- `main.cpp` wires the provider as `[&sessionStateClient](){ return sessionStateClient.locked(); }`.
- New unit test `tests/screenshotmanager_lock_test.cpp` (9 tests) and an added
  `lockScreen_blocksScreenshots` case in `tests/shell_input_router_test.cpp`.
- Side fix: `tests/shell_input_router_test.cpp` had stale `AppHub` /
  `setAppHubVisible` / `apphubVisible` references that no longer matched
  `ShellStateController` (renamed to AppDeck). Renamed in-place; the test
  suite now compiles and runs (22 tests pass).

### Item 6 — `slm-lockd` supervisor + `ForceLocked`

- New binary `slm-lockd` (target in top-level `CMakeLists.txt`). Sources:
  `src/daemon/lockd/lockd_main.cpp`, `src/daemon/lockd/shellliveness.{h,cpp}`.
  Pure `QCoreApplication` daemon — no UI in this iteration.
- `ShellLiveness` watches `org.slm.Desktop` via `QDBusServiceWatcher`. On
  loss it emits `shellLost("shell-process-died")`; lockd then calls
  `org.slm.SessionState.ForceLocked(reason)` so desktopd's authoritative
  lock state remains `Locked` even though the UI is gone.
- `SessionStateService` gained a `ForceLocked(QString reason)` DBus method
  (idempotent; logs `[LOCKD] ForceLocked …`; routed through `setLockState`
  which dedups when already `Locked`).
- `slm-desktop` now registers the well-known DBus name `org.slm.Desktop`
  in `main.cpp` early in startup so the watcher has a stable target.
- New unit test `tests/lockd_shell_liveness_test.cpp` (6 tests).
- **Deferred to next iteration:** the in-process emergency LayerShell lock
  UI (full-screen raster window with password input) — current scope is the
  state-preservation failsafe. Without the visual emergency UI, the screen
  may briefly show the previous frame after a shell crash, but unlock
  always requires a fresh PAM round-trip via `desktopd.Unlock`.

### Item 7 — Logind `PrepareForSleep` integration

- `SessionStateManager` subscribes to
  `org.freedesktop.login1.Manager.PrepareForSleep` on the **system bus**.
  - On sleep (`true`): logs `[LOCKD] [SUSPEND] entering sleep, locking session`,
    calls `Lock()`. `Lock()` is now idempotent (early-returns when `m_idle`
    is already true) so repeated PrepareForSleep events do not re-emit
    `SessionLocked`.
  - On wake (`false`): logs `[LOCKD] [RESUME] woke from sleep`, emits new
    `Resumed()` signal. Wake never auto-unlocks.
- `SessionStateService` declares a new `Resumed()` DBus signal and forwards
  the manager's `Resumed` to it.
- `SessionStateService::Lock()` is now idempotent: returns early when the
  current state is `Locking` or `Locked` (logs the no-op).
- `SessionStateClient` subscribes to the `Resumed` DBus signal and re-emits
  it as the QML-visible Qt signal `resumed()`.
- `Qml/components/overlay/LockScreenWindow.qml` listens for
  `SessionStateClient.resumed()` and re-attaches the SecurityLayer overlay
  per output (matters when monitor topology changed during suspend).
- New unit test `tests/sessionstatemanager_suspend_test.cpp` (6 tests
  covering suspend/resume routing, idempotency, and signal counts).

### Test summary for this iteration

```
shell_input_router_test                22 passed, 0 failed
screenshotmanager_lock_test             9 passed, 0 failed
sessionstatemanager_suspend_test        6 passed, 0 failed
lockd_shell_liveness_test               6 passed, 0 failed
appdecksystem_render_contract_test     13 passed, 0 failed
sessionstateservice_dbus_test           8 passed, 0 failed, 1 skipped
```

The skipped test in `sessionstateservice_dbus_test` is the PAM-success path,
which requires `SLM_TEST_UNLOCK_PASSWORD` to be set in the env (unchanged).

Forbidden-pattern scan is clean.

## Important Remaining Work

### 0. Emergency LayerShell lock UI (was the second half of item 6)

The supervisor (slm-lockd) now ensures daemon state stays `Locked` when the
shell dies, but no emergency visual UI is shown in that window. Next:

- Add an in-process emergency LayerShell window (raster `QGuiApplication`
  inside `slm-lockd`, no QML) using LayerShellQt directly: OverlayLayer +
  Exclusive keyboard + per-output instances. Hard-coded look (black bg,
  centered password field). Unlock goes through `desktopd.Unlock` only;
  no local PAM. Hide once the shell's own SecurityLayer surface returns.

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

### 5. Screenshot/Input Policy While Locked  *(landed this iteration)*

Hard-blocked at both `ShellInputRouter` (input gate) and `ScreenshotManager`
(capture gate). Workspace thumbnail / app preview blur is still pending and
remains as separable follow-up work.

### 6. UI Crash Failsafe  *(supervisor landed this iteration; emergency UI deferred)*

`slm-lockd` supervisor watches `org.slm.Desktop` and forces `Locked` when
the shell vanishes. Authoritative state therefore survives a UI crash. The
in-process emergency LayerShell window (visual fallback while shell respawns)
is the remaining piece; see "Important Remaining Work — 0" above.

### 7. logind/Idle/Resume Integration  *(landed this iteration)*

Logind `PrepareForSleep` is wired through `SessionStateManager`. Auto-lock
fires before suspend; `[SUSPEND]` and `[RESUME]` log tags are emitted; the
`Resumed` signal propagates through the service → client → QML chain so
the security overlay re-attaches per output on wake. Wayland idle-protocol
auto-lock remains deferred.

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
cmake --build build/icu78 -j4 --target \
    desktopd slm-desktop slm-lockd \
    appdecksystem_render_contract_test \
    sessionstateservice_dbus_test \
    shell_input_router_test \
    screenshotmanager_lock_test \
    lockd_shell_liveness_test \
    sessionstatemanager_suspend_test
```

Run contract / unit tests:

```bash
./build/icu78/appdecksystem_render_contract_test
./build/icu78/sessionstateservice_dbus_test
QT_QPA_PLATFORM=offscreen ./build/icu78/shell_input_router_test
./build/icu78/screenshotmanager_lock_test
QT_QPA_PLATFORM=offscreen ./build/icu78/sessionstatemanager_suspend_test
QT_QPA_PLATFORM=offscreen ./build/icu78/lockd_shell_liveness_test
```

Smoke run slm-lockd (won't lock anything without a real session bus + desktopd):

```bash
./build/icu78/slm-lockd --shell-name org.slm.Desktop
```

Scan forbidden runtime patterns:

```bash
rg -n "_canUseLocalUnlockFallback|local unlock fallback|SessionStateClient\\.setLocked|Q_INVOKABLE void setLocked|WindowStaysOnTopHint|requestActivate\\(|\\.raise\\(" Main.qml Qml src -S
```

Check patch hygiene:

```bash
git diff --check
```
