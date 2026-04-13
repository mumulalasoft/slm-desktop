# Recovery Partition Pipeline

Dokumen ini menjelaskan pipeline build artifact recovery partition dan otomasi selector bootloader yang dipakai `slm-recoveryd`.

## 1. Build Artifact Recovery Partition

Script:
- `scripts/recovery/build-recovery-partition-image.sh`

Output default:
- `build/recovery-partition/slm-recovery-rootfs.squashfs`
- `build/recovery-partition/recovery-partition.manifest.json`
- `build/recovery-partition/slm-recovery-systemd-boot.conf.template`
- `build/recovery-partition/slm-recovery-grub.cfg.template`

Contoh:

```bash
scripts/recovery/build-recovery-partition-image.sh \
  --build-dir build/toppanel-Debug \
  --out-dir build/recovery-partition
```

Kebutuhan tool:
- `mksquashfs` (`squashfs-tools`)
- `sha256sum`

## 2. Install Boot Entry (Root)

Script:
- `scripts/recovery/install-recovery-boot-entry.sh`

Contoh auto-detect bootloader:

```bash
sudo scripts/recovery/install-recovery-boot-entry.sh \
  --artifact-dir build/recovery-partition \
  --bootloader auto
```

Script akan:
- systemd-boot: install entry `.conf` ke `loader/entries`.
- GRUB: generate snippet `/etc/grub.d/42_slm_recovery` lalu regenerate grub config.
- Root spec wajib diberikan (`--root-spec`) atau auto-resolve dari device (`--root-device`).

## 2b. Deploy ke Recovery Partition (Root, Destructive)

Script:
- `scripts/recovery/deploy-recovery-partition.sh`

Contoh:

```bash
sudo scripts/recovery/deploy-recovery-partition.sh \
  --device /dev/nvme0n1p8 \
  --image build/recovery-partition/slm-recovery-rootfs.squashfs \
  --artifact-dir build/recovery-partition
```

Perilaku:
- menulis image ke partisi target dengan `dd` (destruktif),
- deteksi `PARTUUID`/`UUID` target,
- install/update boot entry dengan root spec yang sesuai.

## 3. Next-Boot Selector Automation

Script selector:
- `scripts/recovery/detect-recovery-boot-entry.sh`

Script request next boot:
- `scripts/recovery/request-bootloader-recovery.sh`

Perilaku:
- `request-bootloader-recovery.sh auto` akan memanggil detector entry.
- Detector memilih entry berdasarkan prioritas:
  1. hint explicit
  2. kata kunci `recovery|rescue|safe`
  3. entry pertama

Contoh:

```bash
sudo scripts/recovery/request-bootloader-recovery.sh auto
```

## 4. Integrasi slm-recoveryd

Unit `slm-recoveryd.service` mengatur env:
- `SLM_RECOVERY_BOOTLOADER_HELPER=/usr/local/libexec/slm/recovery/request-bootloader-recovery.sh`
- `SLM_RECOVERY_BOOT_ENTRY=auto`
- `SLM_RECOVERY_BOOT_ENTRY_HINT=recovery`

Dengan konfigurasi ini, saat `RequestRecoveryPartition` dipanggil, daemon akan menulis marker recovery dan mencoba set next boot entry secara otomatis.

Opsional untuk aksi UI Safe Mode:
- `SLM_RECOVERY_REPAIR_HELPER=/path/to/repair-script`
- `SLM_RECOVERY_REINSTALL_HELPER=/path/to/reinstall-script`

Jika helper tidak diset:
- `Repair System` fallback ke `fsck -A -T -p`.
- `Reinstall System` menulis request flag `~/.config/slm-desktop/flags/reinstall-system-request.flag`.
