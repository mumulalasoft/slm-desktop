# SLM Desktop — Login & Session Stack Architecture

## Overview

The SLM login/session stack is a layered set of components that sit between **greetd** (the display manager backend) and the desktop shell. Its goals are:

- A controlled, platform-owned startup flow
- Crash loop detection and automatic recovery
- Transactional, rollback-capable configuration
- A meaningful recovery path that users can navigate without shell access

```
greetd
  └── slm-greeter          (Qt/QML login UI)
        └── slm-session-broker  (session orchestrator)
              ├── kwin_wayland / sway / ...  (compositor)
              ├── slm-shell                   (normal/safe mode)
              │   └── slm-watchdog            (health timer)
              └── slm-recovery-app            (recovery mode shell)
```

---

## Components

### `slm-greeter`

A full-screen Qt/QML application launched by greetd.

**Responsibilities:**
- Enumerate local users from `/etc/passwd`
- Authenticate via the greetd IPC protocol (`$GREETD_SOCK`)
- Let the user choose startup mode: Normal / Safe Mode / Recovery
- Display contextual notices: crash history, `configPending` warning, `safeModeForced` banner
- Power actions: suspend, reboot, poweroff via `systemctl`

**Animations:**
- 600 ms cubic fade-in on startup
- Shake animation on login failure

**Key files:**
- `src/login/greeter/greetdclient.h/.cpp` — greetd IPC (4-byte length-prefix + JSON over `QLocalSocket`)
- `src/login/greeter/greeterapp.h/.cpp` — C++ bridge exposed as `GreeterApp` context property
- `Qml/greeter/Main.qml` — window + fade-in
- `Qml/greeter/LoginPage.qml` — login form, avatars, notices, power row
- `Qml/greeter/ModeSelector.qml` — mode popup (Normal / Safe / Recovery)
- `Qml/greeter/PowerRow.qml` — individual power action button

---

### `slm-session-broker`

A C++/Qt Core process launched by greetd after successful authentication, as the session entry point (`sessions/slm.desktop`).

**Startup sequence:**
1. Load `state.json` — check `configPending` flag
2. If `configPending == true` and last session ended without health confirmation → rollback config to previous
3. Increment `crash_count`, write `lastBootStatus = "started"`
4. Load `config.json` → read compositor and shell paths
5. Run `PlatformChecker::checkAll()` — validate compositor, SLM_OFFICIAL_SESSION, required service binaries, XDG_RUNTIME_DIR
6. `evaluateMode()` — apply priority: forced > crash loop > platform failure > requested > normal
7. `performRollback()` — Safe → rollback to safe baseline; Recovery → rollback to previous
8. `prepareEnvironment()` — set `SLM_SESSION_MODE`, `SLM_OFFICIAL_SESSION`, `XDG_SESSION_TYPE`, `XDG_CURRENT_DESKTOP`; set `configPending = true` for Normal mode
9. Launch compositor → wait for `$XDG_RUNTIME_DIR/wayland-0` (10 s timeout)
10. Launch shell (normal/safe) **or** `slm-recovery-app` (recovery mode)
11. Launch `slm-watchdog`
12. Wait for compositor exit → write `lastBootStatus = "ended"` or `"crashed"`

**Key files:**
- `src/login/session-broker/sessionbroker.h/.cpp`
- `src/login/session-broker/main.cpp`

---

### `slm-watchdog`

A short-lived daemon launched by the broker after the shell starts.

**Responsibilities:**
- Single-shot timer: 30 seconds after launch
- On timer: reset `crash_count = 0`, set `configPending = false`, set `lastBootStatus = "healthy"`, promote active config to safe baseline (`config.safe.json`)
- Self-terminates after marking healthy

If the compositor crashes before the watchdog fires, `crash_count` stays elevated for the next boot.

**Key files:**
- `src/login/watchdog/sessionwatchdog.h/.cpp`
- `src/login/watchdog/main.cpp`

---

### `slm-recovery-app`

A full-screen Qt/QML application launched by the broker in Recovery mode, acting as the recovery shell.

**Capabilities:**
- **Reset tab:** Reset to safe defaults (`config.safe.json → config.json`), rollback to previous (`config.prev.json → config.json`), factory reset (delete all config files, write minimal defaults)
- **Snapshots tab:** List available snapshots (timestamp-named), restore any snapshot as active config
- **Log tab:** Show recent `greetd` journal entries via `journalctl`

All config operations are atomic (temp file + rename). After applying changes, the user exits and the broker re-launches the desktop session.

**Key files:**
- `src/login/recovery-app/recoveryapp.h/.cpp`
- `src/login/recovery-app/main.cpp`
- `Qml/recovery/Main.qml`, `ResetPage.qml`, `SnapshotPage.qml`, `LogPage.qml`

---

### `libslmlogin` (static library)

Shared library used by all login stack binaries.

| Class | Purpose |
|---|---|
| `SessionState` / `SessionStateIO` | Persistent runtime state (`state.json`) |
| `ConfigManager` | Transactional config: load, save, rollback, snapshot, promoteToLastGood, factoryReset |
| `PlatformChecker` | Integrity checks: compositor allow-list, `SLM_OFFICIAL_SESSION`, required binaries, `XDG_RUNTIME_DIR` |

---

## State Machine

### Startup mode selection (`evaluateMode`)

```
safeModeForced == true          → Safe
crash_count >= 3                → Recovery
platform check failed           → Safe   (only if requested Normal)
otherwise                       → requested mode (Normal / Safe / Recovery)
```

### `state.json` fields

| Field | Type | Description |
|---|---|---|
| `crash_count` | int | Incremented on each startup, reset to 0 by watchdog |
| `last_mode` | string | `"normal"` / `"safe"` / `"recovery"` |
| `last_good_snapshot` | string | Snapshot id last promoted by watchdog |
| `safe_mode_forced` | bool | Forces Safe mode regardless of user selection |
| `config_pending` | bool | True until watchdog confirms session healthy |
| `recovery_reason` | string | Human-readable reason for non-normal mode |
| `last_boot_status` | string | `"started"` / `"healthy"` / `"ended"` / `"crashed"` |
| `last_updated` | ISO 8601 | Last write timestamp |

---

## Configuration Files (`~/.config/slm-desktop/`)

| File | Purpose |
|---|---|
| `config.json` | Active configuration |
| `config.prev.json` | Previous configuration (before last save) |
| `config.safe.json` | Last known-good baseline (promoted by watchdog) |
| `state.json` | Runtime state (crash_count, mode, pending flags) |
| `snapshots/*.json` | Versioned snapshots (timestamp ids) |

### `config.json` schema (minimal)

```json
{
  "compositor": "kwin_wayland",
  "compositorArgs": [],
  "shell": "slm-shell",
  "shellArgs": []
}
```

---

## Crash Recovery Flow

```
Boot N:   crash_count++ → config marked pending → compositor launched
          compositor crashes before watchdog fires
          → lastBootStatus = "crashed", crash_count stays at N

Boot N+1: configPending == true + lastBootStatus != "healthy"
          → rollback config to previous before incrementing crash_count
          → crash_count++

Boot N+2 (if crash_count >= 3): evaluateMode → Recovery
          → broker launches slm-recovery-app
          → user resets config
          → exits → new session → watchdog fires → crash_count = 0
```

---

## Delayed Commit (configPending)

The `configPending` flag tracks whether the active configuration has been confirmed stable by the watchdog:

- **Broker** sets `configPending = true` when launching a Normal session
- **Watchdog** clears `configPending = false` after 30 s (healthy)
- **Next boot**: if `configPending == true` and last session did not reach healthy → config rolled back to previous *before* crash counter is processed

This means a single session crash after a config change triggers a config rollback without requiring the 3-crash threshold.

---

## Platform Integrity Checks (`PlatformChecker::checkAll`)

| Check | Failure consequence |
|---|---|
| Compositor in allowed list | Warning → Safe mode if Normal requested |
| `SLM_OFFICIAL_SESSION == 1` | Warning |
| Compositor + shell binaries exist on PATH/filesystem | Warning |
| Required service binaries on PATH (`desktopd`, `slm-svcmgrd`, `slm-loggerd`) | Warning |
| `XDG_RUNTIME_DIR` set and directory exists | Warning |

Platform check failures are non-fatal by default but downgrade to Safe mode when a Normal session is requested.

---

## Install Paths

| Binary | Destination |
|---|---|
| `slm-session-broker` | `${LIBEXECDIR}` (e.g. `/usr/libexec/`) |
| `slm-watchdog` | `${BINDIR}` |
| `slm-greeter` | `${BINDIR}` |
| `slm-recovery-app` | `${BINDIR}` |
| `sessions/slm.desktop` | `/usr/share/wayland-sessions/` |

**greetd configuration (`/etc/greetd/config.toml`):**
```toml
[terminal]
vt = 1

[default_session]
command = "slm-greeter"
user = "greeter"
```
