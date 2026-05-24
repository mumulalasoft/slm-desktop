# SLM INSTALLER — GRAND DESIGN ARCHITECTURE
# READY TO COPY PROMPT

====================================================
VISION
====================================================

SLM Installer is NOT merely a Linux installer.

SLM Installer is:

- deployment platform
- system provisioning layer
- recovery-aware installer
- unbreakable-desktop bootstrapper
- deterministic installation system

SLM Installer MUST produce:

- stable system
- recoverable system
- modern UEFI-only architecture
- Btrfs-first system
- systemd-boot platform
- future-ready desktop foundation

Installer philosophy:

- simple
- robust
- deterministic
- offline-capable
- recovery-first
- modern
- minimal but premium

====================================================
CORE ARCHITECTURE
====================================================

SLM installation architecture is divided into TWO layers:

----------------------------------------------------
LAYER 1 — SYSTEM INSTALLER
----------------------------------------------------

Package:
slm-installer

Based on:
Calamares

Responsibilities:

- disk provisioning
- EFI validation
- filesystem provisioning
- Btrfs layout creation
- recovery setup
- system deployment
- systemd-boot setup
- snapshot initialization
- protected package initialization
- firstboot preparation

Installer MUST NOT handle:

- onboarding
- personalization
- cloud setup
- nearby setup
- AI assistant setup
- wallpaper
- theme selection
- desktop UX flows

----------------------------------------------------
LAYER 2 — DESKTOP OOBE
----------------------------------------------------

Package:
slm-welcome

Responsibilities:

- onboarding
- personalization
- accessibility
- privacy
- theme setup
- wallpaper
- online account
- nearby integration
- AI integration
- device integration
- backup restore
- welcome/tutorial flows

This layer belongs to:
SLM Desktop Platform

NOT installer layer.

====================================================
BOOT MODE POLICY
====================================================

SLM Desktop is:

UEFI-only

Legacy BIOS installation is NOT supported.

Installer MUST validate:

- /sys/firmware/efi exists
- EFI firmware boot active
- EFI partition available or creatable

If invalid:

BLOCK installation.

Display readable message:

"SLM Desktop requires modern UEFI firmware."

====================================================
FILESYSTEM POLICY
====================================================

SLM root filesystem MUST use:

Btrfs

Unsupported root filesystems:

- ext4
- xfs
- f2fs

unless manually overridden in advanced mode.

Default architecture:

ESP
ROOT (Btrfs)
RECOVERY

====================================================
EFI PARTITION POLICY
====================================================

EFI partition requirements:

Format:
FAT32

Flags:
esp
boot

Size:
minimum: 512MB
recommended: 1GB

Mount:
/boot/efi

Installer SHOULD reuse existing valid ESP if possible.

====================================================
BTRFS LAYOUT POLICY
====================================================

Installer MUST create Btrfs subvolumes:

@
@home
@var
@log
@cache
@snapshots
@resources
@recovery-staging

Recommended mount options:

compress=zstd
noatime

====================================================
SNAPSHOT POLICY
====================================================

After successful installation:

Automatically create:

factory snapshot

Example:

@factory-initial

Snapshot MUST be recoverable from recovery mode.

====================================================
RECOVERY SYSTEM
====================================================

Recovery architecture is CRITICAL.

Recovery MUST be:

- independent
- bootable
- self-contained

Recovery MUST NOT depend on root filesystem.

Recovery partition SHOULD contain:

- kernel
- initramfs
- recovery shell
- recovery UI
- repair tools
- snapshot restore tools
- recovery metadata
- optional compressed factory image

====================================================
SYSTEMD-BOOT POLICY
====================================================

SLM MUST use:

systemd-boot

NOT GRUB by default.

Installer MUST:

- install systemd-boot
- generate loader entries
- create recovery entry
- create fallback entry

Required boot entries:

- SLM Desktop
- SLM Recovery
- SLM Safe Mode

====================================================
SAFE MODE
====================================================

Installer MUST prepare:

SLM Safe Mode

Purpose:

- recover broken shell
- bypass graphics issues
- disable advanced effects
- launch minimal desktop

User MUST always have recovery access.

====================================================
ATOMIC INSTALLATION STRATEGY
====================================================

Installation MUST use staging area.

Example:

/mnt/slm-install-root

Only finalize after:

- filesystem success
- package deployment success
- bootloader success
- recovery success

If any step fails:

- rollback cleanly
- avoid half-installed system

====================================================
PROTECTED PACKAGE SYSTEM
====================================================

Installer MUST generate:

/usr/share/slm/protected-packages.json

Contains critical system components.

Used later by:

- recovery system
- repair system
- package protection
- rollback validation

====================================================
OFFLINE INSTALLATION POLICY
====================================================

Installer MUST support:

100% offline installation

Internet MUST NOT be required.

Optional online features:

- package updates
- codecs
- language packs
- proprietary drivers

====================================================
HARDWARE VALIDATION
====================================================

Installer SHOULD validate:

- RAM minimum
- GPU compatibility
- Wayland capability
- Secure Boot state
- disk SMART health
- battery level
- internet availability

Warnings MUST be human-readable.

====================================================
DRIVER POLICY
====================================================

Installer SHOULD detect:

- NVIDIA
- Broadcom
- unsupported GPUs
- problematic hardware

Installer SHOULD offer:

- proprietary driver recommendation
- offline driver installation if available

====================================================
LOGGING POLICY
====================================================

Installer MUST generate structured logs.

Paths:

/var/log/slm-installer.log

Recovery copy:

/recovery/logs/

Logs MUST be:

- readable
- structured
- exportable
- diagnostic friendly

====================================================
ERROR HANDLING POLICY
====================================================

Never expose raw technical errors directly.

BAD:

"mkfs failed error 255"

GOOD:

"Unable to create Btrfs filesystem on selected partition."

All errors MUST include:

- explanation
- suggested action
- retry option if possible

====================================================
FIRST BOOT HANDOFF
====================================================

After installation:

Create marker:

/var/lib/slm/firstboot_pending

Installer responsibility ENDS here.

====================================================
DESKTOP OOBE HANDOFF
====================================================

On first login:

slm-session detects:

firstboot_pending

Then launches:

slm-welcome

This app belongs to:

SLM Desktop Platform

NOT installer.

====================================================
OOBE DESIGN PRINCIPLES
====================================================

OOBE MUST support:

- skip
- offline mode
- continue later

Desktop MUST remain accessible even if OOBE crashes.

OOBE is a normal desktop application.

Benefits:

- independently updateable
- reusable
- restartable
- multi-user capable
- settings-reset capable

====================================================
IMMUTABLE/FUTURE READY POLICY
====================================================

Installer architecture MUST prepare for future:

- immutable root
- overlay updates
- atomic updates
- rollback updates
- TPM2 integration
- LUKS2 integration
- cloud recovery
- remote repair

====================================================
IMPLEMENTATION STACK
====================================================

Preferred technologies:

- Calamares
- Qt6
- QML
- systemd
- systemd-boot
- Btrfs native tools

Avoid:

- legacy BIOS assumptions
- fragile shell scripts
- hardcoded disk paths
- live-session-dependent logic

====================================================
UI/UX PRINCIPLES
====================================================

Installer UX:

- minimal
- clean
- modern
- stable
- readable
- deterministic

NOT overloaded with personalization.

OOBE UX:

- animated
- motion-driven
- premium
- immersive
- desktop-focused

====================================================
FINAL ARCHITECTURE SUMMARY
====================================================

SLM Installer responsibilities:

- install system
- provision filesystem
- provision recovery
- provision boot
- prepare first boot

SLM Desktop responsibilities:

- onboarding
- personalization
- cloud/device integration
- desktop experience
- user setup

====================================================
FINAL GOAL
====================================================

SLM Installer must feel like:

- modern OS deployment platform
- reliable recovery-aware installer
- premium desktop bootstrap system
- foundation for unbreakable desktop

NOT merely:

"a Linux distro installer with themes"
