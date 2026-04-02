# Architecture: Environment Variables Service (`slm-envd`)

> Settings > Developer > Environment Variables
> Status: Design — not yet implemented

---

## Component Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                          User Session                                │
│                                                                      │
│  ┌─────────────────┐  ┌──────────────────────────┐  ┌────────────┐  │
│  │  Settings App   │  │   Launcher Abstraction   │  │  Terminal  │  │
│  │  (D-Bus client) │  │  dock · grid · cmd-pal   │  │  (client)  │  │
│  │                 │  │  search · open-with      │  │            │  │
│  └────────┬────────┘  └────────────┬─────────────┘  └─────┬──────┘  │
│           │                        │                       │         │
│           └────────────────────────┼───────────────────────┘         │
│                                    │ D-Bus (session bus)             │
│                   ┌────────────────▼──────────────────────────────┐  │
│                   │           slm-envd (user session service)      │  │
│                   │                                               │  │
│                   │  ┌─────────────┐   ┌──────────────────────┐  │  │
│                   │  │  EnvStore   │   │    MergeResolver     │  │  │
│                   │  │  (JSON I/O) │   │  (priority stack)    │  │  │
│                   │  └─────────────┘   └──────────────────────┘  │  │
│                   │  ┌─────────────┐   ┌──────────────────────┐  │  │
│                   │  │EnvValidator │   │    DBusAdaptor       │  │  │
│                   │  │(risky vars) │   │  org.slm.Env1        │  │  │
│                   │  └─────────────┘   └──────────────────────┘  │  │
│                   │  ┌─────────────┐   ┌──────────────────────┐  │  │
│                   │  │ SessionEnv  │   │     AuditLog         │  │  │
│                   │  │  (in-mem)   │   │   (append-only)      │  │  │
│                   │  └─────────────┘   └──────────────────────┘  │  │
│                   └───────────────────────────────────────────────┘  │
│                        │                      │                      │
│              ~/.config/slm/environment.d/     │ polkit               │
│                                               │                      │
└───────────────────────────────────────────────┼──────────────────────┘
                                                │
┌───────────────────────────────────────────────▼──────────────────────┐
│               slm-envd-helper  (privileged, system bus)              │
│               Writes: /etc/slm/environment.d/system.json            │
│               Guarded by: org.slm.environment.write-system (polkit) │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Component Responsibilities

### `slm-envd` — User Session Service

The central service. Runs as the user, registered on the session D-Bus.

| Sub-component | Responsibility |
|---|---|
| **EnvStore** | Read/write JSON files in `~/.config/slm/environment.d/`. Watch for external changes via `QFileSystemWatcher`. Owns user-persistent and per-app scopes. |
| **SessionEnvStore** | In-memory `QHash` for session-scoped variables. Not persisted. Cleared on logout or explicit `ResetSessionOverrides()`. |
| **MergeResolver** | Pure-logic class. Given all scope layers, produces a final `QHash<QString,QString>` for a given `app_id`. |
| **EnvValidator** | Validates key format, value safety for known-risky variables. Returns `ValidationResult` with severity + message. |
| **DBusAdaptor** | Exposes `org.slm.Environment1` on `/org/slm/Environment`. Generated from XML + hand-extended. |
| **AuditLog** | Append-only structured log for system-scope changes. Writes to `/var/log/slm/envd-audit.log`. |

### `slm-envd-helper` — Privileged Helper

Minimal binary. Registers on the **system** D-Bus. Activated on demand.

- Only three methods: `WriteSystemEntry`, `DeleteSystemEntry`, `ReadSystemEntries`
- No shell execution, no dynamic code
- Validates all inputs independently of the caller (defense in depth)
- Writes to `/etc/slm/environment.d/system.json`
- Every successful write emits an audit log entry

### Launcher Abstraction

All app-launch paths (dock, app grid, command palette, global search, open-with from file manager) **must** call a single `AppLauncher::launch(appId, desktopEntry, extraEnv)` function.

- Calls `GetLaunchEnvironment(app_id, extra)` from slm-envd
- Falls back to current process environment if service is unavailable (with log warning)
- Cache last resolved env per app-id with 500 ms TTL to reduce latency

### Terminal

On startup, calls `GetEffectiveEnvironment("")` (empty app_id = generic user env).
Sets the returned map as the baseline environment for child shells.
Retries up to 3× with 1 s delay if service is not yet available.

### Settings App Client

QML + C++ controller. Uses `EnvServiceClient` (D-Bus proxy). Does not hold its own copy of data — all reads/writes go through D-Bus.

---

## Data Schema

### Entry Object

```json
{
  "version": 1,
  "scope": "user-persistent",
  "entries": [
    {
      "key": "MY_VAR",
      "value": "/usr/local/bin",
      "enabled": true,
      "comment": "Custom tool path",
      "merge_mode": "replace",
      "created_at": "2026-03-25T10:00:00Z",
      "modified_at": "2026-03-25T10:00:00Z"
    }
  ]
}
```

**`merge_mode`** options:
- `replace` (default) — higher priority replaces lower
- `prepend` — prepend to lower-priority value with `:` separator (useful for PATH)
- `append` — append to lower-priority value with `:` separator

### File Locations

| Scope | File |
|---|---|
| User persistent | `~/.config/slm/environment.d/user.json` |
| Per-app | `~/.config/slm/environment.d/apps/<app_id>.json` |
| System | `/etc/slm/environment.d/system.json` (root-owned) |
| Session | In-memory only, not persisted |

### D-Bus Entry Map (`a{sv}`)

```
key:         s  — variable name
value:       s  — variable value
enabled:     b  — active or disabled
scope:       s  — "session" | "user" | "system" | "per-app"
comment:     s  — optional description
merge_mode:  s  — "replace" | "prepend" | "append"
modified_at: s  — ISO 8601
```

---

## D-Bus API

**Interface:** `org.slm.Environment1`
**Object path:** `/org/slm/Environment`
**Bus:** Session bus (user service)

### Methods

```xml
<method name="GetEffectiveEnvironment">
  <arg name="app_id"   type="s"     direction="in"/>
  <arg name="env"      type="a{ss}" direction="out"/>
  <arg name="warnings" type="aa{sv}" direction="out"/>
  <!-- warnings entries: { key, severity, message, source_scope } -->
</method>

<method name="GetLaunchEnvironment">
  <arg name="app_id"   type="s"     direction="in"/>
  <arg name="extra"    type="a{ss}" direction="in"/>
  <arg name="env"      type="a{ss}" direction="out"/>
</method>

<method name="ListEntries">
  <arg name="scope"   type="s"     direction="in"/>
  <arg name="entries" type="aa{sv}" direction="out"/>
</method>

<method name="SetEntry">
  <arg name="scope"   type="s"    direction="in"/>
  <arg name="key"     type="s"    direction="in"/>
  <arg name="value"   type="s"    direction="in"/>
  <arg name="options" type="a{sv}" direction="in"/>
  <!-- options: { enabled, comment, merge_mode } -->
  <arg name="result"  type="a{sv}" direction="out"/>
  <!-- result: { success, error_code, error_message } -->
</method>

<method name="DeleteEntry">
  <arg name="scope" type="s" direction="in"/>
  <arg name="key"   type="s" direction="in"/>
</method>

<method name="SetEntryEnabled">
  <arg name="scope"   type="s" direction="in"/>
  <arg name="key"     type="s" direction="in"/>
  <arg name="enabled" type="b" direction="in"/>
</method>

<method name="SetAppOverride">
  <arg name="app_id" type="s" direction="in"/>
  <arg name="key"    type="s" direction="in"/>
  <arg name="value"  type="s" direction="in"/>
  <arg name="result" type="a{sv}" direction="out"/>
</method>

<method name="DeleteAppOverride">
  <arg name="app_id" type="s" direction="in"/>
  <arg name="key"    type="s" direction="in"/>
</method>

<method name="ListAppOverrides">
  <arg name="app_id"  type="s"     direction="in"/>
  <arg name="entries" type="aa{sv}" direction="out"/>
</method>

<method name="ListAppsWithOverrides">
  <arg name="app_ids" type="as" direction="out"/>
</method>

<method name="ResetSessionOverrides"/>

<method name="ValidateEntry">
  <arg name="key"    type="s"    direction="in"/>
  <arg name="value"  type="s"    direction="in"/>
  <arg name="result" type="a{sv}" direction="out"/>
  <!-- result: { valid, severity, message } -->
</method>
```

### Signals

```xml
<signal name="EntryChanged">
  <arg name="scope"   type="s"/>
  <arg name="key"     type="s"/>
  <arg name="value"   type="s"/>
  <arg name="enabled" type="b"/>
</signal>

<signal name="EntryDeleted">
  <arg name="scope" type="s"/>
  <arg name="key"   type="s"/>
</signal>

<signal name="SessionReset"/>
```

### Properties

```xml
<property name="Version"        type="u"     access="read"/>
<property name="SessionEntries" type="aa{sv}" access="read"/>
<!-- SessionEntries fires PropertiesChanged on mutation -->
```

---

## Merge Strategy & Priority

Priority stack (lowest → highest):

```
1. default          — /etc/environment + systemd environment generators
2. system           — /etc/slm/environment.d/system.json
3. user-persistent  — ~/.config/slm/environment.d/user.json
4. session          — in-memory (current login session only)
5. per-app          — ~/.config/slm/environment.d/apps/<app_id>.json
6. launch-time      — extra{} map passed to GetLaunchEnvironment()
```

**Conflict resolution:** Higher priority wins for `merge_mode=replace`.
For `merge_mode=prepend/append`: the value is prepended/appended to whatever the next lower-priority layer produced, using `:` as separator.

**Example — PATH with prepend:**
```
default:   PATH=/usr/bin:/bin
system:    PATH=/opt/company/bin  [merge_mode=prepend]
user:      PATH=/home/user/.local/bin  [merge_mode=prepend]

Result:    PATH=/home/user/.local/bin:/opt/company/bin:/usr/bin:/bin
```

**Disabled entries** are excluded from the merge entirely (as if they do not exist).

---

## Launch Application GUI Flow

```
User clicks app (dock / grid / search / open-with)
        │
        ▼
AppLauncher::launch(appId, desktopEntry, extraEnv={})
        │
        ├─ Check cache: last resolved env for this appId < 500ms ago?
        │  └─ Yes → use cached env
        │
        ├─ No → D-Bus: slm-envd.GetLaunchEnvironment(appId, extraEnv)
        │        │
        │        ├─ slm-envd: MergeResolver.resolve(appId)
        │        ├─ slm-envd: EnvValidator.check(result)  [add warnings to log]
        │        └─ returns: a{ss} effective env
        │
        ├─ If D-Bus fails: use current process env, log warning
        │
        └─ QProcess::startDetached(executable, args, workingDir, effectiveEnv)
```

---

## Terminal Integration Flow

```
Terminal app starts
        │
        ▼
TerminalEnvBridge::fetchBaseline()
        │
        ├─ D-Bus: slm-envd.GetEffectiveEnvironment("")
        │        [empty app_id = no per-app scope, generic user env]
        │
        ├─ Retry up to 3× (1s interval) if service not yet available
        │
        ├─ On success: set returned map as QProcess env for shell child
        │
        └─ On failure (3 retries exhausted): inherit current process env, warn in terminal banner
```

Terminal internal env (for its own process) remains unchanged.
Only the **child shell** inherits the resolved baseline.

---

## Privileged Write Flow (System Scope)

```
Settings UI: user edits System scope variable
        │
        ▼
EnvVariableController::setSystemEntry(key, value)
        │
        ▼
EnvValidator::validate(key, value)
  ├─ severity=critical → SensitiveVarWarningDialog (must confirm)
  └─ severity=high     → SensitiveVarWarningDialog (can confirm)
        │
        ▼ (user confirms)
        │
        ▼
PolkitBridge::authorize("org.slm.environment.write-system")
  ├─ polkit agent shows password prompt
  └─ returns: authorized | not-authorized | error
        │
        ├─ not-authorized → show inline error, abort
        │
        ▼ (authorized)
        │
slm-envd.SetEntry(scope="system", key, value, options)
        │
        ▼
slm-envd → system bus: slm-envd-helper.WriteSystemEntry(key, value, enabled)
        │
        ├─ helper validates inputs independently
        ├─ writes /etc/slm/environment.d/system.json
        ├─ appends audit log entry
        └─ returns result
        │
        ▼
slm-envd reloads system entries
slm-envd emits EntryChanged(scope="system", key, value, enabled)
        │
        ▼
Settings UI updates via signal
```

---

## Logging & Debugging

### Log Categories

| Category | Purpose |
|---|---|
| `slm.envd` | General service lifecycle |
| `slm.envd.merge` | Merge resolution steps (verbose) |
| `slm.envd.validate` | Validation results |
| `slm.envd.dbus` | D-Bus method calls in/out |
| `slm.envd.store` | File read/write events |
| `slm.envd.helper` | Privileged helper operations |

Enable verbose merge trace:
```
SLM_ENVD_DEBUG=1 slm-envd
# or
QT_LOGGING_RULES="slm.envd.*=true" slm-envd
```

### GetEffectiveEnvironment Debug Output

Each entry in the returned env includes a `source` field in `warnings`:
```
{ key: "PATH", value: "...", source: "user-persistent", merge_mode: "prepend" }
```

The Settings UI "Debug" button shows this full merge trace for any selected app.

### Audit Log Format

`/var/log/slm/envd-audit.log` — append-only, one JSON object per line:
```json
{"ts":"2026-03-25T10:00:00Z","scope":"system","op":"write","key":"MY_VAR","uid":1000,"polkit":"authorized"}
{"ts":"2026-03-25T10:01:00Z","scope":"system","op":"delete","key":"OLD_VAR","uid":1000,"polkit":"authorized"}
```

Rotation: max 10 MB, 5 rotations (logrotate config provided).

---

## Project Folder Structure

```
src/services/envd/
├── main.cpp
├── envservice.cpp / .h            # QObject root; D-Bus registration; lifecycle
├── envstore.cpp / .h              # JSON file I/O; QFileSystemWatcher
├── sessionenvstore.cpp / .h       # In-memory session scope
├── perappenvstore.cpp / .h        # Per-app JSON files
├── mergeresolver.cpp / .h         # Priority merge; pure logic; no Qt deps except types
├── envvalidator.cpp / .h          # Validation rules; RiskyVarPolicy
├── dbusadaptor.cpp / .h           # D-Bus adaptor (generated from XML + extended)
└── auditlog.cpp / .h              # Append-only structured audit logger

src/services/envd-helper/
├── main.cpp
├── helperservice.cpp / .h         # System bus registration; polkit check
├── helperdbusadaptor.cpp / .h     # org.slm.EnvironmentHelper1 adaptor
└── systemenvstore.cpp / .h        # Write /etc/slm/environment.d/system.json

src/core/launcher/
├── applauncher.cpp / .h           # AppLauncher::launch() — one function for all paths
└── launchenvresolver.cpp / .h     # Cache + D-Bus call to slm-envd

src/apps/settings/modules/developer/
├── EnvVariablesPage.qml
├── EnvVarRow.qml
├── AddEditEnvVarDialog.qml
├── SensitiveVarWarningDialog.qml
├── PerAppOverridesView.qml
├── EffectiveEnvPreviewPanel.qml
├── envvariablemodel.cpp / .h      # QAbstractListModel; fed via D-Bus proxy
├── envvariablecontroller.cpp / .h # QML-exposed C++ controller
├── envserviceclient.cpp / .h      # D-Bus proxy wrapper
└── polkitbridge.cpp / .h          # Triggers polkit agent from settings

src/apps/terminal/
└── terminalenvbridge.cpp / .h     # Fetches baseline env from slm-envd on startup

scripts/
├── install-envd-user-service.sh
└── systemd/
    ├── slm-envd.service           # User session service
    └── slm-envd-helper.service    # System service (D-Bus activated)

scripts/dbus/
├── org.slm.Environment1.xml       # D-Bus introspection XML
└── org.slm.EnvironmentHelper1.xml

scripts/polkit/
└── org.slm.environment.policy     # polkit action definitions
```

---

## Design Risks & Mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| App launch path bypasses `AppLauncher` → inconsistent env | High | Single `AppLauncher::launch()` function; code review policy; static analysis rule for direct `QProcess::start` of `.desktop` apps |
| `slm-envd` not running when launcher needs it | High | Launcher falls back to current process env; logs warning; 500ms timeout max |
| Service startup race: launcher calls before service ready | High | `AppLauncher` retries with exponential backoff (max 2s); terminal retries 3× |
| Circular dep: env var needed before `slm-envd` starts | Medium | systemd ordering: `After=graphical-session-pre.target`; minimal bootstrap env from `/etc/environment` is always used as base |
| User sets `PATH=""` → apps broken | High | `EnvValidator` blocks empty PATH; requires explicit confirmation dialog with diff view |
| Session scope lost on crash | Low | Session scope is intentionally ephemeral; document this in UI clearly |
| System scope conflicts with distro packages | Medium | System scope is above distro defaults but below per-app; document merge order |
| Per-app overrides stale after app rename/removal | Low | Garbage-collect on app uninstall hook; periodic scan at service startup |
| Helper binary exploitable | High | Minimize API surface (3 methods); strict input validation; no shell exec; CAP_DAC_WRITE only (not full root if possible) |
| Audit log fills disk | Medium | Log rotation: 10 MB × 5 files via logrotate |
| `merge_mode=prepend` creates duplicate PATH entries | Low | MergeResolver deduplicates `:` separated values after merge |
