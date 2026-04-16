# Snapshot Retention Policy

Policy implemented:
- Keep the latest 10 automatic Btrfs snapshots (`YYYYMMDD-HHMMSS-auto-*`).
- Keep all manual snapshots (`YYYYMMDD-HHMMSS-manual-*`).

Tools:
- Pruner: `scripts/recovery/prune-btrfs-snapshots.sh`
- CLI: `scripts/recovery/recoveryctl.sh snapshot prune --keep-auto 10`
- Timer unit: `scripts/systemd/system/slm-snapshot-retention.timer`

Notes:
- Automatic prune runs after `recoveryctl snapshot create auto ...` by default.
- Configure count with `SLM_KEEP_AUTO_SNAPSHOTS`.
