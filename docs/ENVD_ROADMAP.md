# Implementation Roadmap: Environment Variables Feature

> Module: Settings > Developer > Environment Variables
> Backend: `slm-envd` + `slm-envd-helper`
> Stack: C++17 / Qt6 / QML / D-Bus / polkit
> Status: Not yet started — reference ENVD_ARCHITECTURE.md and ENVD_UXUI.md

---

## Phase Overview

| Phase | Theme | Risk | Value delivered |
|---|---|---|---|
| **1** | Local MVP | Low | Working UI; no service; proves UX model |
| **2** | Service & Merge | Medium | D-Bus service; merge; effective env preview |
| **3** | Launcher & Terminal | Medium-High | Consistent env for all app launches |
| **4** | System Scope & Security | High | Admin variables; audit log; risky-var policy |

Each phase is independently releasable. Later phases depend on earlier ones but do not require them to be complete before starting in a separate branch.

---

## Phase 1 — Local MVP

### Goal

Prove the UX model. Deliver a working Settings page that lets a developer add, edit, delete, and enable/disable user-persistent environment variables stored in a local JSON file. No D-Bus service. No background process.

### Deliverables

- `EnvEntry` data struct with full schema
- `LocalEnvStore` — reads/writes `~/.config/slm/environment.d/user.json`
- `EnvValidator` — key format + basic value rules
- `EnvVariableModel` — `QAbstractListModel` backed by LocalEnvStore
- `EnvVariableController` — QML-facing C++ controller
- QML: `EnvVariablesPage`, `EnvVarRow`, `AddEditEnvVarDialog`
- Wire into Settings module loader under Developer category
- Unit tests

### Classes to Create

```
src/apps/settings/modules/developer/
├── enventry.h                     # Struct: key, value, enabled, scope,
│                                  #   comment, merge_mode, created_at, modified_at
├── localenvstore.cpp / .h         # QObject; JSON read/write; QFileSystemWatcher
├── envvalidator.cpp / .h          # Static validate(key,value) → ValidationResult
├── envvariablemodel.cpp / .h      # QAbstractListModel; roles: Key, Value, Enabled,
│                                  #   Scope, Comment, Modified
├── envvariablecontroller.cpp / .h # Q_PROPERTY + Q_INVOKABLE for QML
├── EnvVariablesPage.qml
├── EnvVarRow.qml
└── AddEditEnvVarDialog.qml
```

### API Provided

```cpp
// EnvValidator
struct ValidationResult { bool valid; QString severity; QString message; };
ValidationResult EnvValidator::validateKey(const QString &key);
ValidationResult EnvValidator::validateValue(const QString &key, const QString &value);
bool             EnvValidator::isSensitiveKey(const QString &key);

// EnvVariableController (exposed to QML)
Q_INVOKABLE void   addEntry(QString key, QString value, QString scope, QString comment);
Q_INVOKABLE void   updateEntry(QString key, QString value, QString comment, QString mergeMode);
Q_INVOKABLE void   deleteEntry(QString key, QString scope);
Q_INVOKABLE void   setEnabled(QString key, QString scope, bool enabled);
Q_INVOKABLE QVariantMap validateKey(QString key);
Q_INVOKABLE QVariantMap validateValue(QString key, QString value);
Q_PROPERTY  bool   loading READ loading NOTIFY loadingChanged
Q_PROPERTY  QString lastError READ lastError NOTIFY lastErrorChanged
```

### Test Plan

| Test | Type | Pass criteria |
|---|---|---|
| Key format rules | Unit | Rejects lowercase, spaces, digits as first char; accepts A–Z, 0–9, `_` |
| Sensitive key detection | Unit | PATH, LD_LIBRARY_PATH etc. return `isSensitive=true` |
| JSON round-trip | Unit | Write then read produces identical `EnvEntry` list |
| Concurrent file watch | Unit | FileSystemWatcher triggers model reload without crash |
| Model CRUD | Unit | add/update/delete emit correct signals, row count correct |
| QML integration | Manual | Page loads, add a variable, confirm it appears in list |
| QML validation | Manual | Invalid key shows inline error; confirm button disabled |

### Risks & Fallback

| Risk | Fallback |
|---|---|
| JSON file corrupted by external write | `LocalEnvStore::load()` validates schema; skip invalid entries; log warning |
| QML binding loops in row delegate | Use explicit `Connections` signal-slot; avoid property bindings on writable state |
| Phase 1 scope creep (per-app, session) | Defer per-app scope to Phase 2; Phase 1 is User scope only |

### Safest Implementation Order

1. `EnvEntry` struct + JSON serialization / deserialization (no Qt deps)
2. `LocalEnvStore` — read, write, `QFileSystemWatcher` reload
3. `EnvValidator` — key format rules, sensitive key list, value checks
4. `EnvVariableModel` — CRUD, roles, signals
5. `EnvVariableController` — thin QML bridge
6. `EnvVarRow.qml` — delegate (static layout first, no actions)
7. `EnvVariablesPage.qml` — list + search field
8. `AddEditEnvVarDialog.qml` — add/edit modal
9. Register module in Developer settings section
10. Unit tests (write alongside each class, not after)

---

## Phase 2 — Service & Merge

### Goal

Extract data logic into a proper user-session D-Bus service (`slm-envd`). Add session scope, merge resolver, per-app scope, and effective environment preview. Settings app switches from `LocalEnvStore` to a D-Bus proxy.

### Deliverables

- `slm-envd` executable registered as systemd user service
- D-Bus interface `org.slm.Environment1` on `/org/slm/Environment`
- `MergeResolver` with 6-layer priority stack
- `SessionEnvStore` (in-memory, not persisted)
- `PerAppEnvStore` (per-app JSON files)
- `EnvServiceClient` — D-Bus proxy used by settings app (replaces `LocalEnvStore`)
- `EffectiveEnvPreviewPanel.qml` + `EffectiveEnvPreviewController`
- Integration tests (service ↔ client round-trip)

### Classes to Create

```
src/services/envd/
├── main.cpp                       # QCoreApplication; D-Bus registration
├── envservice.cpp / .h            # Service root; owns all sub-components
├── envstore.cpp / .h              # User-persistent scope; JSON I/O
├── sessionenvstore.cpp / .h       # In-memory session scope
├── perappenvstore.cpp / .h        # Per-app JSON files
├── mergeresolver.cpp / .h         # Pure logic; no Qt deps except QHash/QString
├── dbusadaptor.cpp / .h           # org.slm.Environment1 adaptor

src/apps/settings/modules/developer/
├── envserviceclient.cpp / .h      # QDBusInterface wrapper; async calls
├── EffectiveEnvPreviewPanel.qml
└── effectiveenvpreviewcontroller.cpp / .h
```

### API Provided (D-Bus)

See full XML in `ENVD_ARCHITECTURE.md`.  Key additions in Phase 2:

```
GetEffectiveEnvironment(app_id) → (a{ss} env, aa{sv} warnings)
GetLaunchEnvironment(app_id, extra) → a{ss}
ListEntries(scope)
SetEntry(scope, key, value, options)
DeleteEntry(scope, key)
SetEntryEnabled(scope, key, enabled)
SetAppOverride / DeleteAppOverride / ListAppOverrides
ResetSessionOverrides()
ValidateEntry(key, value)

Signals: EntryChanged, EntryDeleted, SessionReset
Properties: Version, SessionEntries
```

### Test Plan

| Test | Type | Pass criteria |
|---|---|---|
| MergeResolver — all 6 priority levels | Unit | Higher priority always wins |
| MergeResolver — prepend/append merge mode | Unit | PATH built correctly from multiple layers |
| MergeResolver — disabled entries excluded | Unit | Disabled entries do not appear in output |
| SessionEnvStore — add / remove / reset | Unit | Correct in-memory state; signals emitted |
| Service startup / D-Bus registration | Integration | `qdbus org.slm.Environment1 /org/slm/Environment` returns interface |
| SetEntry round-trip (user scope) | Integration | Set → ListEntries → contains entry; JSON file updated |
| EntryChanged signal fires | Integration | QDBusServiceWatcher receives signal after SetEntry |
| GetEffectiveEnvironment — merge result | Integration | user=A, session=B for same key → B wins |
| GetEffectiveEnvironment — source field | Integration | Each entry reports which scope it came from |
| Preview panel shows correct data | Manual | Select Firefox → see merged env including per-app override |

### Risks & Fallback

| Risk | Fallback |
|---|---|
| D-Bus activation race (settings app launches before `slm-envd`) | `EnvServiceClient` retries connection for 3 s; shows loading state; does not crash |
| `slm-envd` crashes mid-session | Settings app detects service disappearance via `QDBusServiceWatcher`; shows "service unavailable" state; re-enables when service returns |
| Schema migration when entry format changes | Version field in JSON + `EnvStore::migrate()` function called on load |
| D-Bus adaptor codegen complexity | Hand-write adaptor in Phase 2; switch to `qdbusxml2cpp` in Phase 3 when API is stable |

### Safest Implementation Order

1. D-Bus XML interface definition (`org.slm.Environment1.xml`)
2. `MergeResolver` — pure function, test-first, no D-Bus dependency
3. `SessionEnvStore`
4. `PerAppEnvStore`
5. `EnvService` — wires stores + resolver; no D-Bus yet
6. `DBusAdaptor` skeleton
7. `main.cpp` — QCoreApplication + D-Bus registration
8. systemd user service unit file + install script
9. `EnvServiceClient` in settings app (replace `LocalEnvStore`)
10. `EffectiveEnvPreviewController` + `EffectiveEnvPreviewPanel.qml`
11. Integration tests

---

## Phase 3 — Launcher & Terminal Integration

### Goal

All app-launch paths use `slm-envd` to obtain the effective environment before spawning a process. Terminal fetches baseline env from the service on startup. Per-app overrides are testable end-to-end.

### Deliverables

- `AppLauncher::launch()` — single function used by all launch paths
- `LaunchEnvResolver` — D-Bus call to `GetLaunchEnvironment` + 500 ms cache
- Dock, app grid, command palette, global search, open-with updated to use `AppLauncher`
- `TerminalEnvBridge` — terminal-side baseline env fetch
- Settings UI: Per-App Overrides tab (`PerAppOverridesView.qml`, `PerAppOverridesController`)

### Classes to Create

```
src/core/launcher/
├── applauncher.cpp / .h           # AppLauncher::launch(appId, desktopEntry, extraEnv)
└── launchenvresolver.cpp / .h     # D-Bus call + per-app-id cache (500 ms TTL)

src/apps/terminal/
└── terminalenvbridge.cpp / .h     # fetchBaseline() with retry; sets child process env

src/apps/settings/modules/developer/
├── PerAppOverridesView.qml
├── PerAppOverridesController.cpp / .h
└── AppPickerDialog.qml            # If not already shared component
```

### API Provided

```cpp
// AppLauncher
static bool launch(const QString &appId,
                   const QUrl &desktopEntry,
                   const QVariantMap &extraEnv = {});
// Returns false and logs if spawn fails; never throws.

// TerminalEnvBridge
void fetchBaseline(std::function<void(QProcessEnvironment)> callback);
// Async; calls callback with resolved env or falls back to current env.
```

D-Bus additions (slm-envd):
```
GetLaunchEnvironment(app_id, extra) — already in Phase 2
ListAppsWithOverrides() → as
```

### Test Plan

| Test | Type | Pass criteria |
|---|---|---|
| AppLauncher calls GetLaunchEnvironment | Unit | Mock D-Bus; verify call made with correct app_id |
| AppLauncher fallback when service down | Unit | QProcess spawned with current process env; warning logged |
| Cache TTL: second call within 500 ms hits cache | Unit | D-Bus mock called only once for two launches within TTL |
| Dock launch inherits user variable | Integration | Set MY_VAR=hello; launch test app from dock; app sees MY_VAR |
| Per-app override scoped correctly | Integration | Set MOZ_VAR=1 per-app for Firefox only; other apps do not see it |
| Terminal baseline env | Manual | `echo $MY_VAR` in new terminal → "hello" |
| Open-with uses AppLauncher | Manual | Open file from file manager → launched app sees correct env |

### Risks & Fallback

| Risk | Fallback |
|---|---|
| Launch paths that bypass AppLauncher | Establish code convention; add comment/lint guard on `QProcess::startDetached` for .desktop app launches |
| Launch latency increase (D-Bus round-trip before spawn) | 500 ms TTL cache per app-id; typical D-Bus call < 5 ms in same session |
| open-with runs in a different process context (file manager) | open-with IPC must use AppLauncher via D-Bus call to launcher process, not direct spawn |
| Terminal race: service not yet ready at terminal startup | Retry 3× with 1 s delay; if still unavailable, inherit current process env and show 1-line warning banner in terminal |

### Safest Implementation Order

1. `LaunchEnvResolver` (pure logic + D-Bus call + cache) — test-first
2. `AppLauncher::launch()` with resolver + fallback
3. Wire **dock** to AppLauncher (most visible; easy to verify)
4. Wire **app grid**
5. Wire **command palette**
6. Wire **global search**
7. Wire **open-with** (most complex — may involve IPC to launcher process)
8. `TerminalEnvBridge`
9. Per-App Overrides tab in settings UI
10. End-to-end integration tests

---

## Phase 4 — System Scope & Security

### Goal

Enable system-wide environment variables (all users on the machine) via a privileged D-Bus helper gated by polkit. Add audit logging and a warning policy for risky variables. Harden the entire feature.

### Deliverables

- `slm-envd-helper` — minimal privileged D-Bus service (system bus, D-Bus activated)
- polkit action: `org.slm.environment.write-system`
- `AuditLog` — append-only structured log at `/var/log/slm/envd-audit.log`
- `RiskyVarPolicy` — severity levels per variable; blocking vs soft-warning
- `SensitiveVarWarningDialog.qml` in settings app
- `PolkitBridge` — triggers polkit agent from settings app
- Settings UI: System scope tab unlocked
- logrotate config for audit log

### Classes to Create

```
src/services/envd-helper/
├── main.cpp
├── helperservice.cpp / .h         # QObject; system D-Bus registration
├── helperdbusadaptor.cpp / .h     # org.slm.EnvironmentHelper1 adaptor
└── systemenvstore.cpp / .h        # Writes /etc/slm/environment.d/system.json

src/services/envd/
├── auditlog.cpp / .h              # Append-only; structured JSON lines; rotation guard
└── riskyvar_policy.cpp / .h       # Static severity table; isCritical/isHigh/isMedium

src/apps/settings/modules/developer/
├── SensitiveVarWarningDialog.qml
└── polkitbridge.cpp / .h          # Triggers polkit agent; async result callback
```

### API Provided

**System bus — `org.slm.EnvironmentHelper1`:**
```xml
<method name="WriteSystemEntry">
  <arg name="key"     type="s"    direction="in"/>
  <arg name="value"   type="s"    direction="in"/>
  <arg name="enabled" type="b"    direction="in"/>
  <arg name="result"  type="a{sv}" direction="out"/>
</method>

<method name="DeleteSystemEntry">
  <arg name="key" type="s" direction="in"/>
</method>

<method name="ReadSystemEntries">
  <arg name="entries" type="aa{sv}" direction="out"/>
</method>
```

**polkit action:**
```
org.slm.environment.write-system
  — implicit-any: no
  — implicit-inactive: no
  — implicit-active: auth_admin_keep
```

**RiskyVarPolicy API:**
```cpp
enum class RiskSeverity { None, Medium, High, Critical };
RiskSeverity RiskyVarPolicy::severity(const QString &key);
QString      RiskyVarPolicy::description(const QString &key);
bool         RiskyVarPolicy::requiresDialog(const QString &key);
```

### Test Plan

| Test | Type | Pass criteria |
|---|---|---|
| RiskyVarPolicy — PATH is Critical | Unit | `severity("PATH") == Critical` |
| RiskyVarPolicy — unknown key is None | Unit | `severity("MY_CUSTOM") == None` |
| AuditLog — append does not truncate | Unit | Write 3 entries; file has exactly 3 JSON lines |
| AuditLog — rotation threshold | Unit | At max size, rotates without data loss |
| polkit gating — unauthorized rejected | Integration | Call WriteSystemEntry without auth → error returned |
| polkit gating — authorized accepted | Integration | Authorized call → /etc/slm/environment.d/system.json updated |
| Helper validates independently | Integration | Pass `PATH=""` to helper directly → rejected (not forwarded from envd) |
| Audit log entry written | Integration | After authorized write, log file has entry with uid + timestamp |
| SensitiveVarWarningDialog shown | Manual | Edit PATH in settings → warning dialog with diff appears |
| System scope tab shows polkit button | Manual | System tab → lock icon → click → password prompt |

### Risks & Fallback

| Risk | Severity | Fallback |
|---|---|---|
| polkit agent not available on custom desktops | High | Show "Run settings as administrator" fallback instruction |
| `slm-envd-helper` binary has a privilege escalation bug | Critical | Minimize API to 3 methods; no shell execution; no dynamic loading; input validation in both envd and helper (defense in depth); security review before merge |
| `PATH=""` slips through to helper | Critical | Double validation: `EnvValidator` in envd + independent check in helper; both must pass |
| Audit log fills disk | Medium | logrotate: max 10 MB × 5 files; helper checks remaining space before write |
| `/etc/slm/environment.d/` missing | Low | Helper creates directory `root:root 755` on first write |
| Per-app + system scope conflict creates unexpected behavior | Medium | Document merge order clearly in UI; per-app always overrides system |
| Phase 4 complexity delays Phase 3 | High | Deliver Phase 4a (read-only system scope display) first, then Phase 4b (write) |

### Safest Implementation Order

1. `RiskyVarPolicy` + wire into `EnvValidator` (settings app gets warning badges immediately)
2. `SensitiveVarWarningDialog.qml` — show for sensitive vars even before polkit exists
3. `AuditLog` class (can be tested standalone)
4. polkit action XML (`org.slm.environment.write-system.policy`)
5. `slm-envd-helper` — skeleton only, D-Bus registration on system bus
6. `HelperDBusAdaptor` + `SystemEnvStore`
7. Wire `slm-envd.SetEntry(scope=system)` → polkit check → helper call
8. `PolkitBridge` in settings app
9. Settings UI: unlock System scope tab with polkit integration
10. Audit log logrotate config + install script
11. Security review of helper binary surface area
12. Integration + security tests

---

## Cross-Phase Decisions

### D-Bus service name

Using `org.slm.Environment1` as provisional name. Rename to `org.slm.Desktop.Environment1` to align with existing service naming convention before Phase 2 lands. Update in XML, unit files, and client at the same time.

### JSON file format versioning

Every JSON config file carries `"version": 1`. When entry schema changes:
- Bump `"version"` in the file
- Add `EnvStore::migrate(int fromVersion)` called on load
- Never remove old fields before one full release cycle

### Testing infrastructure

- Phase 1–2: standard Qt unit test + `QSignalSpy`
- Phase 2: D-Bus integration tests using in-process `QDBusConnection::sessionBus()` with a test service
- Phase 3: launch tests using `QProcess` + a minimal "env reporter" test binary that prints its environment
- Phase 4: polkit tested via a mock polkit agent (same pattern used in existing portal tests)

### File watching strategy

`QFileSystemWatcher` watches:
- `~/.config/slm/environment.d/` (directory) → reload user.json + per-app files on change
- `/etc/slm/environment.d/` (directory) → reload system.json; accessible read-only by `slm-envd`

Debounce: 200 ms after last change event before reload (handles atomic file writes that produce multiple events).

### Backward compatibility

If `slm-envd` is not installed (Phase 1 era):
- `LocalEnvStore` still writes the same JSON file format
- When `slm-envd` is later installed, it reads the existing file without migration

If `slm-envd` is installed but not running:
- `AppLauncher` and `TerminalEnvBridge` fall back gracefully
- Settings app shows degraded state (read-only from LocalEnvStore)
