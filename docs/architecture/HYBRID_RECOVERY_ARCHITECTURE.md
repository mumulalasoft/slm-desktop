# Hybrid Recovery Architecture (Unbreakable Wayland Desktop)

## 1) Architecture Diagram

```text
+--------------------- Main OS ----------------------+
|  slm-session-broker                                 |
|      |                                              |
|      +--> compositor + shell + greeter             |
|      +--> slm-watchdog                             |
|      +--> slm-recoveryd (health + escalation)      |
|                                                     |
|  Snapshot Layer (Btrfs subvol/snapshot metadata)   |
|  Config Layer   (~/.config/slm-desktop/*)          |
+--------------------------+--------------------------+
                           |
                           | escalation marker + boot flag
                           v
+---------------- Recovery Partition -----------------+
|  Minimal OS (independent root)                      |
|  fallback compositor + lightweight Qt/QML UI        |
|  slm-recoveryd (recovery mode)                      |
|  mount/fsck/btrfs/network tooling                   |
+-----------------------------------------------------+
```

## 2) Partition Layout

```text
Disk
├─ EFI System Partition (FAT32)
├─ Root (Btrfs)
│  ├─ @ (system)
│  ├─ @home (optional)
│  └─ snapshots/*
└─ Recovery Partition (EXT4 or SquashFS image)
```

Rules:
- Recovery partition bootable independently.
- Recovery partition must not depend on main root.
- Recovery partition treated read-only for normal users.

## 3) Bootloader Integration (GRUB/systemd-boot)

- Add explicit `Recovery` menu entry.
- Add health/escalation handoff:
  - `slm-recoveryd` writes recovery request marker.
  - boot-stage helper reads marker and selects recovery entry for next boot.
- Normal flow keeps default entry to main OS.

Operational helper in repo:
- `scripts/recovery/request-bootloader-recovery.sh`
  - systemd-boot: `bootctl set-oneshot <entry>`
  - GRUB: `grub-reboot <entry>`

## 4) systemd Services

Main session:
- `slm-session-broker`
- `slm-watchdog`
- `slm-recoveryd`

Recovery session:
- `slm-recoveryd` (recovery mode)
- fallback compositor
- recovery UI app

Current repo additions:
- `scripts/systemd/slm-recoveryd.service`
- `scripts/install-recoveryd-user-service.sh`

## 5) slm-recoveryd Design

Responsibilities:
- Health monitoring API per component (`compositor`, `shell`, `login`, `gpu-wayland`, `network`, `portal`, `greeter`).
- Crash-loop tracking (component crash timestamps in 10s window).
- Auto-recovery actions:
  - rollback previous/safe config
  - set `safeModeForced`
- Escalation:
  - repeated failed auto-recovery -> request recovery partition boot

DBus contract:
- Service: `org.slm.Desktop.Recovery`
- Path: `/org/slm/Desktop/Recovery`
- Interface: `org.slm.Desktop.Recovery`
- Methods:
  - `Ping`, `GetStatus`
  - `ReportHealth`, `ReportCrash`
  - `TriggerAutoRecovery`, `RequestSafeMode`, `RequestRecoveryPartition`, `ClearRecoveryFlags`

## 6) Safe Mode UI (QML)

Mandatory actions:
- Restart Desktop
- Reset Desktop Settings (non-destructive)
- Disable Extensions
- Reset Graphics Stack
- Rollback Snapshot
- Open Terminal
- Network Repair

Implementation note:
- Reuse existing recovery/safe-mode QML shell and explicitly avoid GPU-heavy effects.

## 7) Recovery Partition UI

Mandatory actions:
- Repair System
- Rollback Snapshot
- Reset Desktop Only
- Reinstall System
- Open Terminal

Safety:
- destructive actions require confirmation
- rollback should present pre-apply summary

## 8) Snapshot Integration Plan

Triggers:
- before system update
- before driver install
- before major config change
- before critical package removal

Flow:
1. Create named snapshot (`Before System Update`, etc.)
2. Persist metadata for UI listing
3. On rollback request: show summary, confirm, apply, reboot

Hard guarantees:
- No black screen without fallback path
- Recovery UI never GPU-acceleration dependent
- Terminal not required for basic recovery operations
