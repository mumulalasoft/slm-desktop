# SLM Desktop TODO

> Canonical session summary: `docs/SESSION_STATE.md`

## Fokus Aktif (Prioritas Tinggi)

### Storage & Automount
- [ ] QA cold reboot penuh (bukan hanya restart service) untuk persistence policy mount/automount.
- [ ] QA manual lintas kondisi media: locked, busy, unsupported FS, burst multi-partition.

### Startup Performance Desktop
- [ ] Turunkan waktu `qml.load.begin -> main.onCompleted` dengan profiling terarah per komponen.

---

## Program Network & Firewall

### Phase 1 (Wajib)
- [x] NftablesAdapter: jalankan `nft -f -` secara nyata.
- [x] PolicyEngine: reconcile fullstate setelah remove/clear IP policy.
- [x] UI Settings: Security -> Firewall smoke test contract.
  - verified via:
    - `firewallservice_dbus_contract_test`
    - `firewall_policyengine_contract_test`
    - `firewall_nftables_adapter_test`
    - `firewallserviceclient_contract_test`
    - `security_settings_routing_js_test`

### Phase 2
- [ ] Outgoing control (allow/block/prompt) berbasis app identity.
- [ ] Deteksi proses CLI/interpreter (python/node script target, parent/cwd/tty/cgroup).
- [ ] Popup policy yang kontekstual dan tidak spam.

### Phase 3
- [ ] Trust system (System/Trusted/Unknown/Suspicious) + policy adaptif.
- [ ] Behavior learning + auto suggestion.
- [ ] Quarantine mode untuk app mencurigakan.

---

## Notification
- [ ] Hardening multi-monitor behavior.

## Parking Lot (Tidak Aktif Sekarang)
- Struktur Dock: **dibekukan** pada state "last good" sampai siklus pengujian berikutnya.
- Rencana refactor arsitektur shell yang besar: lanjut setelah milestone stabilitas/firewall tercapai.

---

## Program (Hybrid Recovery System: Unbreakable Wayland Desktop)

### Execution Progress (Current)
- [~] `slm-recoveryd` core daemon scaffold implemented (DBus service, health reporting, crash-loop tracking, auto-recovery escalation).
- [~] Build integration added for `slm-recoveryd` (`cmake/Sources.cmake`, `CMakeLists.txt`, startup bootstrap in `main.cpp`).
- [~] User service assets added (`scripts/systemd/slm-recoveryd.service`, `scripts/install-recoveryd-user-service.sh`).
- [~] Escalation wiring integrated in login stack (`session-broker` reads recovery marker; `watchdog` clears stale recovery flags on healthy session).
- [~] Recovery ops tooling added (`scripts/recovery/recoveryctl.sh`, `scripts/recovery/request-bootloader-recovery.sh`).
- [~] `slm-recoveryd` bootloader hook added (optional helper via `SLM_RECOVERY_BOOTLOADER_HELPER`).
- [~] Contract test baseline added (`tests/recoveryservice_contract_test.cpp`).
- [~] Initial architecture output document added: `docs/architecture/HYBRID_RECOVERY_ARCHITECTURE.md`.
- [~] In-system Safe Mode page integrated (`Qml/recovery/SafeModePage.qml`) with simple-clean action UI and wired backend actions (restart/reset/disable-extension/reset-graphics/terminal/network/snapshot routing).
- [~] Recovery partition runtime pipeline ditingkatkan: build image + deploy-to-partition + boot entry install dengan `PARTUUID/UUID` root spec (`build-recovery-partition-image.sh`, `deploy-recovery-partition.sh`, `install-recovery-boot-entry.sh`, `detect-recovery-boot-entry.sh`).
- [~] Full safe-mode/recovery UI flow expanded per strict action list (Safe Mode now includes Repair System and Reinstall System actions; remaining gap: dedicated recovery-partition runtime integration and end-to-end validation).
- [~] Snapshot diff preview and guarded rollback execution from UI (`RecoveryApp.previewSnapshotDiff`, confirmation-gated restore flow in `Qml/recovery/SnapshotPage.qml`).

### TITLE
Design and Implement a Hybrid Recovery System for an "Unbreakable" Wayland Desktop OS

### GOAL
Design a fully integrated recovery architecture that guarantees:

* The system NEVER becomes unusable
* The user ALWAYS has access to a graphical recovery interface
* No scenario results in a black screen or terminal-only lockout

The system must combine:

1. In-system recovery (primary, seamless UX)
2. Dedicated recovery partition (low-level fallback)

---

### 1. ARCHITECTURE OVERVIEW

Design a hybrid system consisting of:

A. Main OS (Root System)
B. Recovery Partition (Independent minimal OS)
C. Shared Data Access Layer
D. Snapshot System (Btrfs or equivalent)
E. slm-recoveryd (core recovery daemon)

---

### 2. DISK LAYOUT (STRICT REQUIREMENT)

Define partition layout:

* EFI System Partition (FAT32)
* Root Partition (Btrfs recommended)
* User Data (optional separate subvolume)
* Recovery Partition (EXT4 or SquashFS-based)

Recovery Partition MUST:

* Be bootable independently
* Not depend on root filesystem
* Be protected from user modification

---

### 3. slm-recoveryd (MANDATORY DAEMON)

Implement a system daemon with responsibilities:

A. Health Monitoring:

* compositor startup success
* shell process stability
* login success/failure
* GPU/Wayland initialization
* critical services (network, portal, greeter)

B. Crash Loop Detection:

* if compositor OR shell fails >3 times within 10 seconds → trigger recovery state

C. Automatic Recovery Actions:

* revert last config change
* disable last extension/module
* fallback to default theme/UI
* restart affected services

D. Escalation Logic:

* if recovery fails → trigger Safe Mode
* if Safe Mode fails → signal bootloader to enter Recovery Partition

---

### 4. IN-SYSTEM RECOVERY (PRIMARY UX)

Must operate inside main OS.

#### SAFE MODE REQUIREMENTS:

* Launch minimal session:

  * software rendering (llvmpipe fallback)
  * no blur / heavy effects
  * minimal Qt/QML UI

#### SAFE MODE UI MUST INCLUDE:

* Restart Desktop
* Reset Desktop Settings (non-destructive)
* Disable Extensions
* Reset Graphics Stack
* Rollback Snapshot
* Open Terminal (advanced users)
* Network Repair

#### RULE:

Safe Mode MUST NOT depend on:

* GPU acceleration
* user configuration
* external plugins

---

### 5. RECOVERY PARTITION (FALLBACK SYSTEM)

#### CONTENTS:

A minimal independent OS including:

* init system (systemd or minimal init)
* Wayland compositor (fallback, wlroots-based preferred)
* Qt/QML lightweight UI
* slm-recoveryd (recovery mode)
* disk tools:

  * mount
  * fsck
  * btrfs tools
* network tools (DHCP + WiFi)

#### BOOT REQUIREMENTS:

* Bootable via bootloader fallback entry
* Automatically triggered if:

  * main OS fails to boot
  * kernel panic
  * root filesystem corruption

---

### 6. RECOVERY PARTITION RESPONSIBILITIES

Once booted, it MUST:

1. Detect and mount main root filesystem

2. Detect snapshot system

3. Provide UI actions:

   * Repair System
   * Rollback Snapshot
   * Reset Desktop Only
   * Reinstall System
   * Open Terminal

4. Ensure safe operations:

   * no destructive action without confirmation
   * preview changes before rollback

---

### 7. SNAPSHOT SYSTEM (CRITICAL)

Use Btrfs (preferred) OR equivalent.

#### AUTO SNAPSHOT TRIGGERS:

* before system update
* before driver install
* before major config change
* before package removal of critical components

#### SNAPSHOT NAMING:

Examples:

* "Before NVIDIA Driver Install"
* "Before System Update"
* "Before Theme Change"

#### ROLLBACK FLOW:

* user selects snapshot
* system shows diff preview (basic)
* confirm action
* perform rollback
* reboot

---

### 8. GPU FAILURE HANDLING

STRICT REQUIREMENTS:

* If Wayland compositor fails → fallback to software rendering
* If GPU driver is broken → force llvmpipe
* GPU MUST NEVER block UI from appearing

---

### 9. PROTECTED COMPONENTS

Prevent removal or corruption of:

* compositor
* shell
* greeter
* portal backend
* network manager

If user attempts removal:

* show warning dialog
* require explicit override

---

### 10. BOOT FLOW (STRICT)

Implement:

NORMAL:
Boot → Main OS → Desktop

FAILURE LEVEL 1:
Crash → slm-recoveryd → auto fix

FAILURE LEVEL 2:
Repeated crash → Safe Mode

FAILURE LEVEL 3:
Boot failure → Recovery Partition

FAILURE LEVEL 4 (optional):
No local recovery → Internet Recovery

RULE:
User MUST ALWAYS reach a graphical interface.

---

### 11. INTERNET RECOVERY (OPTIONAL ADVANCED)

If enabled:

* detect no valid system
* connect to WiFi
* download recovery image
* reinstall OS

---

### 12. UI REQUIREMENTS

ALL recovery UIs MUST:

* be graphical (NO terminal-first design)
* be consistent with desktop branding
* be usable via mouse and keyboard
* avoid GPU-heavy effects

---

### 13. DEVELOPER MODE

Provide tools:

* simulate crash
* force safe mode
* view logs
* test rollback

---

### 14. OUTPUT REQUIRED

Agent MUST produce:

1. Architecture diagram
2. Partition layout
3. Bootloader integration (GRUB/systemd-boot)
4. systemd service definitions
5. slm-recoveryd design
6. Safe Mode QML UI
7. Recovery Partition UI
8. Snapshot integration plan

---

### HARD RULES (NON-NEGOTIABLE)

* NEVER allow black screen without fallback
* NEVER rely on GPU for recovery UI
* NEVER require terminal for basic recovery
* ALWAYS provide at least one working graphical path

END.
