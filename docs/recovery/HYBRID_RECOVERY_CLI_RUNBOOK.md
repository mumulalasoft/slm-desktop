# Hybrid Recovery CLI Runbook

## 1) Partition disk (destructive)
```bash
sudo scripts/recovery/partition-hybrid-recovery.sh --disk /dev/nvme0n1
```

## 2) Format + create Btrfs subvolumes
```bash
sudo scripts/recovery/mkfs-hybrid-layout.sh \
  --esp /dev/nvme0n1p1 \
  --recovery /dev/nvme0n1p2 \
  --system /dev/nvme0n1p3
```

## 3) Apply fstab baseline
- Use template: `docs/recovery/HYBRID_RECOVERY_FSTAB.example`
- Replace UUID placeholders via `blkid`.

## 4) Install recovery scripts + systemd units
```bash
sudo scripts/recovery/install-system-recovery-services.sh
```

## 5) Enable bootloader recovery menu
- GRUB snippet template: `scripts/recovery/grub-recovery-snippet.cfg`
- Integrate into `/etc/grub.d`, then regenerate grub config.

## 6) Use recoveryctl
```bash
# snapshot ops
sudo scripts/recovery/recoveryctl.sh snapshot create auto pre-update
sudo scripts/recovery/recoveryctl.sh snapshot list
sudo scripts/recovery/recoveryctl.sh snapshot prune --keep-auto 10
sudo scripts/recovery/recoveryctl.sh snapshot restore 20260417-010203-manual-test --yes

# system reset (@root only)
sudo scripts/recovery/recoveryctl.sh system reset 20260417-010203-manual-test --yes

# boot repair
sudo scripts/recovery/recoveryctl.sh boot repair
sudo scripts/recovery/recoveryctl.sh boot grub-integrate

# boot counter checks
sudo scripts/recovery/recoveryctl.sh boot check
sudo scripts/recovery/recoveryctl.sh boot success
sudo scripts/recovery/recoveryctl.sh boot clear-state
```

## 8) Optional automation units
```bash
# snapshot retention timer
sudo install -m 0644 scripts/systemd/system/slm-snapshot-retention.service /etc/systemd/system/
sudo install -m 0644 scripts/systemd/system/slm-snapshot-retention.timer /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now slm-snapshot-retention.timer

# post-update verify path (requires post-update-verify.sh installed in /usr/local/lib/slm-package-policy/)
sudo install -m 0644 scripts/systemd/system/slm-post-update-verify.service /etc/systemd/system/
sudo install -m 0644 scripts/systemd/system/slm-post-update-verify.path /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now slm-post-update-verify.path
```

## 7) Log files
- Recovery actions: `/var/log/recovery.log`
- Boot failure detection: `/var/log/boot-recovery.log`
