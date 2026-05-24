# Privileged Access Backend — slm-fsd Design Document

**Version:** 0.1 (scaffold)
**Status:** Architecture locked; Phase-1 implementation in progress.
**Daemon:** `slm-fsd` — SLM File System Daemon

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│  User Session (uid=N)                                                   │
│                                                                         │
│  ┌──────────────────┐   ┌──────────────────────────────────────────┐   │
│  │  slm-filemanager │   │  Any other user app (future)             │   │
│  │  (runs as user)  │   │  (runs as user)                          │   │
│  └────────┬─────────┘   └──────────────┬───────────────────────────┘   │
│           │ D-Bus call                  │ D-Bus call                    │
│           └──────────────┬─────────────┘                               │
│                          │  org.slm.Desktop.Fsd1                        │
│                          │  /org/slm/Desktop/Fsd                        │
│                          ▼                                              │
│         ┌────────────────────────────────────┐                         │
│         │          SYSTEM D-Bus              │                         │
│         └────────────────────────────────────┘                         │
└─────────────────────────────┬───────────────────────────────────────────┘
                              │  (crosses privilege boundary)
┌─────────────────────────────▼───────────────────────────────────────────┐
│  root / System                                                          │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  slm-fsd  (runs as root, Type=dbus on system bus)               │   │
│  │                                                                  │   │
│  │  FsdService          ← D-Bus object, enforces token + polkit    │   │
│  │    ├── FsdTokenStore  ← in-memory, non-persistent tokens        │   │
│  │    ├── FsdPathPolicy  ← protected-path detection, validation    │   │
│  │    ├── FsdPolkit      ← polkit check per-verb                   │   │
│  │    ├── FsdSnapshot    ← BTRFS snapshot before destructive ops   │   │
│  │    └── FsdRecoveryTrash ← safe-delete: move to recovery trash   │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  slm-polkit-agent  (user session, already shipping)             │   │
│  │  Handles auth dialogs for all polkit challenges.                │   │
│  │  No new dialog code needed for fsd.                             │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  slm-recoveryd  (coordinates snapshot rollback UI)              │   │
│  │  RequestSafeMode / RequestRecoveryPartition remain here.        │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility |
|-----------|---------------|
| `FsdService` | D-Bus adaptor, verb dispatch, caller-identity binding |
| `FsdTokenStore` | Token issuance, TTL expiry, scope validation, connection-death revocation |
| `FsdPathPolicy` | Protected-path detection, canonicalization, `..` rejection, UTF-8 validation |
| `FsdPolkit` | Per-verb polkit check via `org.freedesktop.PolicyKit1` D-Bus API |
| `FsdSnapshot` | BTRFS subvolume detection, snapshot creation before destructive ops |
| `FsdRecoveryTrash` | Move-on-delete to `/var/lib/slm/recovery-trash/`, permanent purge verb |

---

## 2. Relationship with slm-fileopsd

`src/daemon/fileopsd/` (binary: `slm-fileopsd`) handles **unprivileged** long-running file
operations (copy, move, trash, empty-trash) for user-owned paths. It stays intact.

`slm-fsd` is a **separate** daemon that handles privileged access only. The two daemons
are complementary:

- `slm-fileopsd`: user-context ops, job queue, progress reporting for user paths.
- `slm-fsd`: root-privilege ops, token-gated, polkit-authorized, for protected paths.

The filemanager calls `slm-fileopsd` for normal operations; it calls `slm-fsd` only when
`IsProtected()` returns true for the target path.

---

## 3. D-Bus Interface

**Service:** `org.slm.Desktop.Fsd1`
**Object path:** `/org/slm/Desktop/Fsd`
**Bus:** system (crosses privilege boundary)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
  "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<!--
  org.slm.Desktop.Fsd1  — SLM File System Daemon privileged interface.

  Token dict (a{sv}):
    token_id   (s) : UUID of the token
    path       (s) : path prefix this token covers
    ops        (as): list of permitted op names
    expires_at (t) : Unix timestamp (UTC) when the token expires
    uid        (u) : UID of the owner session
    snapshot   (s) : BTRFS snapshot name taken at token issuance (empty if none)

  Result dict (a{sv}) — returned by all mutating verbs:
    ok           (b) : success flag
    error        (s) : error string ("" on success)
    error_code   (s) : machine-readable code ("NotImplemented", "PermissionDenied",
                       "PathNotAllowed", "TokenExpired", "TokenScopeViolation",
                       "SnapshotFailed", "ImmutableMount", ...)
    snapshot     (s) : snapshot name created (empty if none)

  IsProtected result:
    protected  (b) : true if path falls under a protected prefix
    scope      (s) : matched scope name (e.g. "system", "etc", "boot", "user-config")
-->
<node name="/org/slm/Desktop/Fsd">
  <interface name="org.slm.Desktop.Fsd1">

    <!-- ── Query ──────────────────────────────────────────────────── -->

    <method name="Ping">
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="IsProtected">
      <arg name="path"          type="s"     direction="in"/>
      <arg name="protected"     type="b"     direction="out"/>
      <arg name="scope"         type="s"     direction="out"/>
    </method>

    <method name="GetTokens">
      <arg name="tokens"        type="aa{sv}" direction="out"/>
    </method>

    <!-- ── Token lifecycle ────────────────────────────────────────── -->

    <method name="ReleaseToken">
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <!-- ── File operations (all require a valid token_id) ─────────── -->

    <method name="RequestRename">
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="source"        type="s"     direction="in"/>
      <arg name="destination"   type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestDelete">
      <!-- Moves to recovery trash; does NOT unlink immediately. -->
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="path"          type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestPurge">
      <!-- Permanently deletes from recovery trash. Requires separate polkit check. -->
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="path"          type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestMove">
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="source"        type="s"     direction="in"/>
      <arg name="destination"   type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestCopy">
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="source"        type="s"     direction="in"/>
      <arg name="destination"   type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestWrite">
      <!-- Write content to a file atomically (write-to-temp + rename). -->
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="path"          type="s"     direction="in"/>
      <arg name="content"       type="ay"    direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestCreate">
      <!-- Create an empty file or directory. -->
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="path"          type="s"     direction="in"/>
      <arg name="is_dir"        type="b"     direction="in"/>
      <arg name="mode"          type="u"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestChmod">
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="path"          type="s"     direction="in"/>
      <arg name="mode"          type="u"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestChown">
      <!-- Most destructive of the permission ops — requires auth_admin each time. -->
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="path"          type="s"     direction="in"/>
      <arg name="uid"           type="u"     direction="in"/>
      <arg name="gid"           type="u"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestMount">
      <!-- Requires auth_admin each time (no keep). -->
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="device"        type="s"     direction="in"/>
      <arg name="mountpoint"    type="s"     direction="in"/>
      <arg name="fstype"        type="s"     direction="in"/>
      <arg name="options"       type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <method name="RequestUnmount">
      <arg name="token_id"      type="s"     direction="in"/>
      <arg name="mountpoint"    type="s"     direction="in"/>
      <arg name="result"        type="a{sv}" direction="out"/>
    </method>

    <!-- ── Signals ─────────────────────────────────────────────────── -->

    <signal name="TokenIssued">
      <arg name="token"         type="a{sv}"/>
    </signal>

    <signal name="TokenExpired">
      <arg name="token_id"      type="s"/>
    </signal>

    <signal name="TokenRevoked">
      <arg name="token_id"      type="s"/>
      <arg name="reason"        type="s"/>
    </signal>

    <signal name="OperationCompleted">
      <arg name="token_id"      type="s"/>
      <arg name="verb"          type="s"/>
      <arg name="result"        type="a{sv}"/>
    </signal>

    <signal name="SnapshotCreated">
      <arg name="snapshot_name" type="s"/>
      <arg name="source_path"   type="s"/>
      <arg name="token_id"      type="s"/>
    </signal>

    <!-- ── Properties ──────────────────────────────────────────────── -->

    <property name="serviceRegistered"  type="b" access="read"/>
    <property name="apiVersion"         type="s" access="read"/>

  </interface>
</node>
```

---

## 4. Token Model

### Data Structure

```cpp
struct FsdToken {
    QString  id;           // UUID v4
    QString  pathPrefix;   // canonicalized, absolute
    QStringList opSet;     // e.g. {"rename", "delete", "move"}
    QDateTime expiresAt;   // UTC, default now + 10 min
    uint     ownerUid;     // from GetConnectionUnixUser at issuance
    QString  callerService;// D-Bus unique name (":1.NNN") — revoke on disconnect
    QString  snapshotName; // BTRFS snapshot taken at issuance (empty if none)
};
```

### TTL Math

- Default TTL: **600 seconds** (10 minutes) from issuance.
- polkit `auth_admin_keep` grants last 15 minutes; fsd tokens expire sooner — do not rely on keep for ongoing access.
- A `QTimer` per token fires at `expiresAt`; fires `TokenExpired` signal and removes from store.
- Token is **not** persisted to disk — daemon restart invalidates all tokens by design.

### Scope-Check Algorithm

```
function checkTokenScope(token, requestedPath, requestedOp):
    now = QDateTime::currentDateTimeUtc()
    if now >= token.expiresAt:
        return EXPIRED
    canonPath = canonicalize(requestedPath)   // resolves symlinks
    if ".." in path components of canonPath:
        return PATH_TRAVERSAL
    if not canonPath.startsWith(token.pathPrefix + "/")
       and canonPath != token.pathPrefix:
        return SCOPE_VIOLATION
    if requestedOp not in token.opSet:
        return SCOPE_VIOLATION
    return OK
```

### Persistence

Tokens are **not** persisted. If `slm-fsd` restarts (e.g. crash recovery), the caller
must re-request. This is intentional — a restart clears any compromised token state.

### Connection-death Revocation

`FsdTokenStore` connects to `QDBusConnection::serviceUnregistered` for each unique-name
that holds a token. When the connection disappears (app crash, clean exit), all tokens
bound to that connection are immediately revoked and `TokenRevoked` is emitted.

---

## 5. Polkit Action Catalog

Policy file: `scripts/polkit/org.slm.desktop.fsd.policy`

| Action ID | Operation | allow_active | Description (EN) |
|-----------|-----------|-------------|------------------|
| `org.slm.desktop.fsd.rename` | RequestRename | `auth_admin_keep` | Rename a protected file or directory |
| `org.slm.desktop.fsd.delete` | RequestDelete | `auth_admin_keep` | Move a protected item to the recovery trash |
| `org.slm.desktop.fsd.purge` | RequestPurge | `auth_admin` | Permanently delete from recovery trash |
| `org.slm.desktop.fsd.move` | RequestMove | `auth_admin_keep` | Move a protected file or directory |
| `org.slm.desktop.fsd.copy` | RequestCopy | `auth_admin_keep` | Copy into a protected location |
| `org.slm.desktop.fsd.write` | RequestWrite | `auth_admin_keep` | Write content to a protected file |
| `org.slm.desktop.fsd.create` | RequestCreate | `auth_admin_keep` | Create a file or directory in a protected location |
| `org.slm.desktop.fsd.chmod` | RequestChmod | `auth_admin_keep` | Change permissions on a protected path |
| `org.slm.desktop.fsd.chown` | RequestChown | `auth_admin` | Change ownership of a protected path |
| `org.slm.desktop.fsd.mount` | RequestMount | `auth_admin` | Mount a filesystem |
| `org.slm.desktop.fsd.unmount` | RequestUnmount | `auth_admin_keep` | Unmount a filesystem |

**Rationale for `auth_admin` (no keep):** `chown`, `purge`, and `mount` are irreversible or
high-impact enough that requiring explicit authentication per-invocation is the right trade-off.
`auth_admin_keep` is appropriate for the 10-minute working window that matches the token TTL.

**Indonese message templates** are included in the policy XML (section 9).

---

## 6. Protected Path Detection

### Initial Allowlist (`/etc/slm/fsd/protected-paths.d/00-system.conf`)

```
/boot
/efi
/etc
/usr
/lib
/lib64
/opt
/sbin
/bin
/var/lib/slm
/var/cache/slm
/proc
/sys
/dev
/run/systemd
```

### User Extension

Drop additional `.conf` files into `/etc/slm/fsd/protected-paths.d/`. Each file is
a newline-separated list of absolute path prefixes. Blank lines and `#` comments
are ignored. `FsdPathPolicy` loads all files at startup and inotify-watches the
directory for runtime updates (Phase 2).

### Scope Names

`IsProtected` returns a `scope` string derived from the first matching entry:

| Prefix | Scope name |
|--------|-----------|
| `/boot`, `/efi` | `boot` |
| `/etc` | `etc` |
| `/usr`, `/bin`, `/sbin`, `/lib`, `/lib64` | `system` |
| `/opt` | `opt` |
| `/var/lib/slm`, `/var/cache/slm` | `slm-data` |
| `/proc`, `/sys`, `/dev`, `/run/systemd` | `pseudo` |

---

## 7. BTRFS Snapshot Integration

### Subvolume Detection

Before a destructive operation on a protected path, `FsdSnapshot` calls
`statfs()` and checks `f_type == BTRFS_SUPER_MAGIC (0x9123683e)`. If the
path is not on BTRFS, snapshot is skipped; `result["snapshot"]` is empty.

### Snapshot Naming Convention

```
slm-fsd-<op>-<YYYYMMDDTHHMMSS>-<uuid-short>
```

Example: `slm-fsd-delete-20260524T143022-a3f9b1`

Snapshots are created as read-only BTRFS snapshots of the containing subvolume
using `btrfs subvolume snapshot -r <source> <dest>` via `QProcess`.

### Snapshot Location

`/var/lib/slm/snapshots/fsd/`

### Retention Policy (Phase 2)

`slm-snapshot-retention.service` (already in `scripts/systemd/system/`) will
gain an fsd-specific retention rule: keep the last 10 fsd snapshots per subvolume,
purge older ones. Coordinate with the existing retention timer.

### Coordination with recoveryd

`slm-recoveryd` exposes `RequestRecoveryPartition` and `RequestSafeMode`.
`slm-fsd` emits `SnapshotCreated` with the snapshot name. `slm-recoveryd`
can listen to `slm-fsd`'s signals via D-Bus subscription and present rollback
UI in Phase 2. No direct C++ coupling — loose coupling through signals.

---

## 8. Safe-Delete / Recovery Trash Layout

```
/var/lib/slm/recovery-trash/
  <uid>/                       ← per-user namespace
    <YYYYMMDDTHHMMSS>-<uuid>/  ← one directory per deleted item
      metadata.json            ← original path, op, token_id, snapshot_name, timestamp
      content                  ← the actual moved file/dir (symlink-safe)
```

### metadata.json Schema

```json
{
  "original_path": "/etc/foo/bar",
  "operation": "delete",
  "token_id": "...",
  "snapshot_name": "slm-fsd-delete-...",
  "deleted_at": "2026-05-24T14:30:22Z",
  "uid": 1000
}
```

### Permanent Purge

`RequestPurge(token_id, path)` where `path` is the trash entry path.
Requires `auth_admin` polkit action `org.slm.desktop.fsd.purge`.
Calls `FsdRecoveryTrash::purge()` which does a recursive `rm -rf` via
C++ `std::filesystem::remove_all`.

---

## 9. Security Model

### Threat Model

| Threat | Mitigation |
|--------|-----------|
| Malicious user app calls verbs directly without polkit | `FsdPolkit::checkAction()` is called for every mutating verb before any filesystem operation. polkit denies if the user hasn't authenticated. |
| TOCTOU: check path then act on different file | Path is resolved and canonicalized once, stored in a local variable, and all subsequent `open()` / `rename()` calls use that fd obtained by `open(O_PATH)` — not re-looked-up by name (Phase 1 stubs; Phase 2 full implementation). |
| Symlink race: attacker swaps protected dir for symlink | `FsdPathPolicy::validate()` uses `O_NOFOLLOW` for the final component and `lstat` to verify the type matches intent. Symlinks in the middle of a path are allowed (they are resolved) but the final target is verified. |
| Token stolen from IPC | Tokens are bound to the D-Bus connection unique name. A different caller presenting a stolen token ID will fail the `ownerUid != callerUid` check. |
| Path traversal via `../` | `FsdPathPolicy::validate()` rejects any raw path containing `..` components before canonicalization, and also rejects the result if it escapes the token's `pathPrefix`. |
| Non-UTF-8 / oversized path | `FsdPathPolicy::validate()` checks `path.size() <= PATH_MAX`, `QStringView::isValidUtf8()` (Qt 6.6+), and rejects on failure. |
| Immutable mount (`/usr` on read-only overlay) | `FsdService` catches `EROFS` / `EPERM` from write operations and returns `error_code = "ImmutableMount"` with a hint string explaining that changes must go through the package/runtime layer. |
| Caller impersonation via PID reuse | Caller identity is captured atomically at D-Bus connection setup using `GetConnectionUnixUser` on the unique name, not the PID. PIDs can be reused; unique names cannot within a bus session. |

### systemd Sandboxing

See section 10 for the full unit. Key directives:
- `NoNewPrivileges=yes` — prevents privilege escalation via SUID bits.
- `CapabilityBoundingSet=CAP_CHOWN CAP_DAC_OVERRIDE CAP_DAC_READ_SEARCH CAP_FOWNER CAP_SYS_ADMIN` — minimal set for filesystem ops and BTRFS ioctls.
- `RestrictAddressFamilies=AF_UNIX` — no network.
- `ProtectKernelTunables=yes`, `ProtectKernelModules=yes`, `ProtectClock=yes`.
- `ProtectSystem=no` — must be omitted; fsd needs write access to system paths.
- `PrivateTmp=yes` — for temporary files used in atomic writes.

---

## 10. systemd Unit

**Path:** `scripts/systemd/system/slm-fsd.service` (system unit — requires root)

```ini
[Unit]
Description=SLM File System Daemon (privileged access gateway)
Documentation=https://slm.desktop.local/docs/fsd
After=dbus.service local-fs.target
Wants=dbus.service

[Service]
Type=dbus
BusName=org.slm.Desktop.Fsd1
ExecStart=/usr/libexec/slm-fsd
Restart=on-failure
RestartSec=2

# Capability hardening — minimal set for filesystem + BTRFS ops
CapabilityBoundingSet=CAP_CHOWN CAP_DAC_OVERRIDE CAP_DAC_READ_SEARCH CAP_FOWNER CAP_SYS_ADMIN
AmbientCapabilities=CAP_CHOWN CAP_DAC_OVERRIDE CAP_DAC_READ_SEARCH CAP_FOWNER CAP_SYS_ADMIN

# No privilege escalation via setuid/capabilities on exec
NoNewPrivileges=yes

# Filesystem isolation
PrivateTmp=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectClock=yes
ProtectHostname=yes
LockPersonality=yes
RestrictRealtime=yes
MemoryDenyWriteExecute=yes

# Network: only AF_UNIX for D-Bus
RestrictAddressFamilies=AF_UNIX

# System calls whitelist
SystemCallArchitectures=native
SystemCallFilter=@system-service @file-system @io-event rename renameat renameat2 mount umount2

# State directory (auto-created by systemd, owned by root)
StateDirectory=slm/recovery-trash slm/snapshots/fsd
StateDirectoryMode=0750

[Install]
WantedBy=multi-user.target
```

---

## 11. Polkit Policy XML

**Path:** `scripts/polkit/org.slm.desktop.fsd.policy`

(See the actual file — kept in sync with this doc.)

---

## 12. Client-Side API Guide (for src/apps/filemanager)

The filemanager calls into `slm-fsd` only when `IsProtected()` returns true.

### Typical call flow

```
1. filemanager calls IsProtected(path) → (protected=true, scope="etc")
2. filemanager decides it needs "delete" on that path
3. filemanager calls RequestDelete(token_id="", path=...)
   → fsd performs polkit check (launches auth dialog via slm-polkit-agent)
   → on success, fsd takes BTRFS snapshot, moves to recovery-trash, returns token dict
4. filemanager receives result: ok=true, snapshot="slm-fsd-delete-...", token_id="..."
5. Filemanager can call ReleaseToken(token_id) when done, or let it expire
6. Filemanager subscribes to TokenExpired / TokenRevoked signals to update UI state
```

**Note on token_id for first call:** Pass an empty string as `token_id` on the first
call to a protected path. `slm-fsd` will trigger polkit, issue a token, and the token
ID is included in the result dict. Subsequent calls for the same path-prefix + op-set
within the TTL window can reuse that token_id without re-authentication.

### D-Bus connection

```cpp
// Use system bus — fsd is a system-level daemon
QDBusInterface fsd("org.slm.Desktop.Fsd1",
                   "/org/slm/Desktop/Fsd",
                   "org.slm.Desktop.Fsd1",
                   QDBusConnection::systemBus());
```

### Error handling

Always check `result["ok"].toBool()` and `result["error_code"].toString()`:
- `"TokenExpired"` → re-request (re-authentication may be needed)
- `"ImmutableMount"` → show user a message about read-only system
- `"PermissionDenied"` → user declined polkit auth dialog
- `"PathNotAllowed"` → path escaped scope; treat as programming error

---

## 13. Phase Split

### Phase 0 — Scaffold (this PR)
- Daemon compiles and registers on system D-Bus.
- All verbs return `{ok: false, error_code: "NotImplemented"}`.
- Token store, path policy, polkit, snapshot, recovery-trash are stub classes.
- UX can wire up QML against the real D-Bus surface.

### Phase 1 — Core Implementation
- `FsdPathPolicy`: real validation, protected-path loading from `/etc/slm/fsd/`.
- `FsdPolkit`: real polkit calls via `org.freedesktop.PolicyKit1`.
- `FsdTokenStore`: real TTL timers, connection-death revocation.
- `FsdService`: real `RequestRename`, `RequestDelete` (to recovery-trash), `RequestMove`, `RequestCopy`, `RequestWrite`, `RequestCreate`.
- `FsdRecoveryTrash`: real move-to-trash, metadata.json write.
- Unit tests for path validation and token scope-check.

### Phase 2 — Hardening + BTRFS
- `FsdSnapshot`: real BTRFS snapshot via `btrfs` ioctl or subprocess.
- `RequestChmod`, `RequestChown`, `RequestMount`, `RequestUnmount`.
- Inotify watch on protected-paths.d for runtime config reload.
- Snapshot retention integration with `slm-snapshot-retention.timer`.
- recoveryd signal subscription for rollback UI.
- Full TOCTOU mitigation via `O_PATH` + `openat` fd-based ops.

### Phase 3 — User-facing Integration
- Filemanager QML: `ProtectedOperationDialog.qml` using existing `slm-polkit-agent`.
- Recovery Trash browser in filemanager sidebar.
- Settings page: view/purge recovery trash, manage protected-paths.
- Audit log: structured journald entries for all privileged operations.
