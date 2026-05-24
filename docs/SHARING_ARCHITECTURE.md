# SLM Sharing Management System — Architecture

## Overview

`slm-sharingd` is the centralized sharing orchestration daemon for SLM Desktop. It abstracts all
Linux sharing technologies (Samba, Avahi/mDNS, CUPS, SSH, PipeWire) behind a single, unified DBus
API and a premium, zero-jargon settings UX.

Users never see Samba, CUPS config, mDNS announcements, or firewall rules. They see named devices,
named capabilities, and a trust-based permission model.

---

## DBus API

Bus name: `org.slm.Sharing`

### Interfaces

| Interface               | Object Path              | Purpose                          |
|-------------------------|--------------------------|----------------------------------|
| `org.slm.Sharing`       | `/org/slm/Sharing`       | Feature control, file shares, transfers |
| `org.slm.Sharing.Nearby`| `/org/slm/Sharing/Nearby`| Device discovery, send-to        |
| `org.slm.Sharing.Trust` | `/org/slm/Sharing/Trust` | Pairing, trust levels, permissions |

### org.slm.Sharing

**Methods**

| Method | Description |
|--------|-------------|
| `Ping()` | Health check |
| `GetCapabilities()` | Probed backend capabilities |
| `GetStatus()` | Current feature states |
| `SetFeatureEnabled(feature, enabled)` | Toggle sharing features |
| `GetFeatureState(feature)` | Query feature state |
| `AddSharedFolder(path, options)` | Share a folder |
| `RemoveSharedFolder(path)` | Stop sharing a folder |
| `UpdateSharedFolder(path, options)` | Update share settings |
| `ListSharedFolders()` | List all active shares |
| `SendFile(deviceId, path)` | Send file to nearby device |
| `ListActiveTransfers()` | List in-progress transfers |
| `CancelTransfer(transferId)` | Cancel a transfer |
| `AcceptIncomingTransfer(transferId, savePath)` | Accept incoming file |
| `RejectIncomingTransfer(transferId)` | Reject incoming file |
| `CheckEnvironment()` | Probe backend availability |
| `TryAutoFix(issue)` | Attempt to resolve a detected issue |

**Features (SetFeatureEnabled / GetFeatureState)**

| Key | Capability |
|-----|-----------|
| `file-sharing` | Samba/GVFS network file sharing |
| `nearby-sharing` | Send/receive files with nearby devices |
| `screen-sharing` | PipeWire-based screen cast |
| `printer-sharing` | CUPS printer publishing |
| `remote-access` | SSH terminal access |
| `media-sharing` | DLNA-compatible media streaming |
| `clipboard-sharing` | Nearby clipboard sync |

**Signals**

- `SharedFolderAdded(path, info)`
- `SharedFolderRemoved(path)`
- `TransferStarted(transferId, info)`
- `TransferProgress(transferId, bytesTransferred, totalBytes)`
- `TransferCompleted(transferId, success, error)`
- `TransferIncomingRequest(transferId, fromDeviceId, info)`
- `FeatureStateChanged(feature, enabled)`

### org.slm.Sharing.Nearby

**Methods**

| Method | Description |
|--------|-------------|
| `GetDevices()` | All currently visible devices |
| `StartDiscovery()` | Activate discovery |
| `StopDiscovery()` | Deactivate discovery |
| `GetDiscoveryState()` | Whether discovery is running |
| `SendFileTo(deviceId, path)` | Initiate file send |
| `GetDeviceInfo(deviceId)` | Full device details |

**Device info fields**

- `deviceId` — stable UUID
- `name` — human name (e.g. "Rudi's Tablet")
- `type` — `desktop`, `laptop`, `phone`, `tablet`, `tv`, `printer`
- `trustLevel` — `blocked`, `untrusted`, `trusted`, `full`
- `online` — bool
- `battery` — int 0–100 or -1 if unknown
- `capabilities` — list: `file`, `clipboard`, `screen`, `terminal`, `media`, `print`
- `transport` — list: `mdns`, `ble`, `lan`

**Signals**

- `DeviceFound(deviceId, info)`
- `DeviceLost(deviceId)`
- `DeviceUpdated(deviceId, info)`
- `DiscoveryStateChanged(active)`

### org.slm.Sharing.Trust

**Methods**

| Method | Description |
|--------|-------------|
| `GetTrustedDevices()` | All devices with trust records |
| `GetDeviceTrust(deviceId)` | Trust level + permissions |
| `SetDeviceTrust(deviceId, level)` | Raise/lower trust |
| `PairDevice(deviceId)` | Initiate pairing handshake |
| `UnpairDevice(deviceId)` | Remove trust record |
| `GetDevicePermissions(deviceId)` | Per-capability permissions |
| `SetDevicePermission(deviceId, permission, allowed)` | Grant/revoke capability |
| `AcceptPairingRequest(pairingId)` | Accept incoming pairing |
| `RejectPairingRequest(pairingId)` | Reject incoming pairing |
| `BlockDevice(deviceId)` | Block all contact |
| `UnblockDevice(deviceId)` | Unblock device |

**Trust levels**

| Level | Meaning |
|-------|---------|
| `blocked` | All interactions refused |
| `untrusted` | Visible but no capabilities granted |
| `trusted` | File send/receive + selected capabilities |
| `full` | All granted capabilities active |

**Signals**

- `DeviceTrustChanged(deviceId, level)`
- `PairingRequested(pairingId, deviceInfo)`
- `PairingCompleted(pairingId, success)`
- `DevicePermissionChanged(deviceId, permission, allowed)`

---

## Daemon Structure

```
src/daemon/sharingd/
├── sharingd_main.cpp            — entry point, registers all three services
├── sharingservice.h/cpp         — org.slm.Sharing implementation
├── nearbyservice.h/cpp          — org.slm.Sharing.Nearby implementation
├── trustservice.h/cpp           — org.slm.Sharing.Trust implementation
├── sharingmanager.h/cpp         — internal orchestration; owns adapters
├── nearbyengine.h/cpp           — mDNS device discovery via QtNetwork + Avahi
├── trustdatabase.h/cpp          — SQLite trust relationship store
├── transfersession.h/cpp        — in-progress transfer tracking
└── adapters/
    ├── isharingadapter.h        — pure-virtual adapter contract
    ├── sambaadapter.h/cpp       — Samba file sharing backend
    ├── avahiadapter.h/cpp       — mDNS announcement + discovery
    ├── cupsadapter.h/cpp        — CUPS printer publishing
    └── sshadapter.h/cpp         — SSH authorized_keys management
```

---

## Adapter Architecture

Every backend is a `ISharingAdapter` subclass. The manager probes adapters at startup; unavailable
backends degrade gracefully — their corresponding features are reported as `unavailable` rather than
enabled, and the UI shows a user-friendly explanation with a recovery suggestion.

```
ISharingAdapter
  ├── probe()        → AdapterStatus { available, unavailable, degraded }
  ├── activate()     → starts the backend service
  ├── deactivate()   → stops the backend service
  ├── capabilities() → list of feature keys this adapter handles
  └── recover()      → attempt self-healing (install missing pkg, restart service)
```

Adapters run in the daemon process but communicate with their backends via `QProcess` / D-Bus /
POSIX APIs. A crash in a backend does not crash the daemon — the adapter catches it and reports
`degraded`.

---

## Trust Model

Every nearby device is identified by a stable UUID derived from its cryptographic identity (public
key fingerprint). The trust database stores:

| Field | Description |
|-------|-------------|
| `deviceId` | UUID (primary key) |
| `displayName` | Human-readable name |
| `publicKeyFingerprint` | SHA-256 of device public key |
| `trustLevel` | blocked / untrusted / trusted / full |
| `pairedAt` | ISO-8601 timestamp |
| `lastSeenAt` | ISO-8601 timestamp |
| `permissions` | JSON capability grant map |

Pairing is a challenge-response exchange: the initiating device sends its public key; the accepting
user confirms via a notification dialog; both sides store the fingerprint. Subsequent reconnections
are verified by comparing fingerprints — no PIN re-entry required.

---

## Crash Recovery

- Daemon restarts automatically via systemd `Restart=on-failure`
- Adapter crashes are isolated — the `SharingManager` detects a gone adapter via a process watcher
  and marks the feature `degraded` without affecting other features
- `TransferSession` objects persist state to `$XDG_RUNTIME_DIR/slm-sharingd/transfers/` so
  incomplete transfers can be resumed or reported correctly after a restart
- The `NearbyEngine` re-announces all active shares on reconnection to Avahi

---

## Firewall Integration

`slm-sharingd` communicates with `slm-firewalld` via `org.slm.Firewall` when activating or
deactivating sharing features:

- File sharing active → open TCP 445 (Samba) on LAN zone only
- Printer sharing active → open TCP 631 (IPP) on LAN zone only
- Remote access active → open TCP 22 (SSH) on explicitly trusted hosts only
- Screen sharing → uses PipeWire portal (no network port needed)
- Nearby sharing → uses mDNS (UDP 5353) + ephemeral TCP ports, LAN zone

All rules are tagged with the `slm-sharingd` source and removed when features are disabled.

---

## System Integration

### Pulse (Search)
`slm-sharingd` publishes nearby devices and services to the Pulse search index via
`org.slm.Search.Pulse` so users can find "Rudi's Tablet" or "Office Printer" directly.

### AppDeck
Context actions injected via `org.slm.Desktop.ContextMenu`:
- **Send to Nearby Device** — triggers `SendFile` for selected file
- **Cast Screen** — activates screen sharing to selected device
- **Sync Clipboard** — triggers one-shot clipboard sync

### Crown
The Crown system indicator receives:
- `TransferStarted` / `TransferProgress` / `TransferCompleted` → progress indicator
- `TransferIncomingRequest` → incoming file notification with Accept/Reject actions
- `PairingRequested` → pairing confirmation dialog
- `DeviceFound` (first time, trusted device) → brief "Device nearby" toast

### File Manager
The file manager's sidebar "Nearby" section calls `GetDevices()` and renders each trusted device.
The share dialog calls `AddSharedFolder` / `RemoveSharedFolder`. The quick-share toolbar action
calls `SendFile`.

---

## Privacy & Security Policy

- **No auto-share**: No folder is ever shared automatically. Every share is explicit.
- **No auto-accept**: Incoming files always require a user decision.
- **Clipboard sync requires pairing**: Never syncs with `untrusted` devices.
- **Screen sharing requires per-session portal approval**: Follows the existing portal model.
- **Local-network-only by default**: All sharing features restrict to the LAN zone.
- **Bluetooth** (future): Uses OBEX only with `trusted` devices.

---

## Future Roadmap

| Phase | Capability |
|-------|-----------|
| 1 | File sharing (Samba), Nearby discovery (mDNS), Trust model, Settings UI |
| 2 | Clipboard sync, Printer sharing (CUPS), Firewall integration |
| 3 | Screen sharing (PipeWire portal), Remote terminal (SSH) |
| 4 | Media sharing (DLNA abstraction), Cast targets |
| 5 | BLE transport for Nearby, Android SLM client |
| 6 | Cloud handoff, cross-session continuity |
