# SLM Installer — Backend Implementation Plan

## Gambaran Umum Konteks

Rencana ini didasarkan pada `docs/SLM_INSTALLER.md` sebagai spec yang mengikat, arsitektur daemon yang ada di `src/daemon/`, dan infrastruktur login/recovery di `src/login/`. `recoveryd` sudah memiliki D-Bus API untuk `RequestRecoveryPartition`, `RequestSafeMode`, dan state machine berbasis `recovery-partition-request.json` — arsitek installer harus compatible dengan kontrak ini, bukan menggantikannya.

---

## 1. Calamares Integration Strategy

### Pendekatan: Calamares sebagai Execution Engine, Logika di SLM Modules

Calamares dipertahankan sebagai framework (job scheduling, progress reporting, D-Bus bridge ke UI Qt6/QML), namun **semua modul kritis ditulis ulang sebagai modul C++** bawaan SLM. Modul Python Calamares tersedia, namun dipilih C++ karena: (a) runtime dependency lebih kecil di live ISO, (b) error handling lebih deterministik, (c) konsisten dengan seluruh codebase Qt6/C++ yang ada.

### Modul Calamares yang Dibuang

Buang atau nonaktifkan modul-modul berikut dari Calamares upstream (mereka digantikan oleh implementasi SLM):

- `partition` → diganti `slm-partition`
- `bootloader` → diganti `slm-bootloader`
- `unpackfs` / `unsquashfs` → diganti `slm-deploy`
- `fstab` → diganti `slm-fstab`
- `grubcfg` → tidak relevan (systemd-boot)
- `users`, `locale`, `keyboard` → dipindahkan ke OOBE (`slm-welcome`), bukan installer
- `displaymanager` → tidak relevan (greetd sudah hardcoded di shell)

### Modul Calamares yang Dipertahankan

- `welcome` → hanya untuk validasi hardware pre-install (dikustomisasi)
- `summary` → ringkasan tindakan sebelum konfirmasi user
- `finished` → trigger reboot / unmount

### File Layout SLM Calamares Modules

```
src/installer/
  calamares-modules/
    slm-uefi-check/          # C++ module
      CMakeLists.txt
      SlmUefiCheckJob.cpp
      SlmUefiCheckJob.h
      module.desc
    slm-hardware-probe/      # C++ module
      CMakeLists.txt
      SlmHardwareProbeJob.cpp
      SlmHardwareProbeJob.h
      module.desc
    slm-partition/           # C++ module
      CMakeLists.txt
      SlmPartitionJob.cpp
      SlmPartitionJob.h
      module.desc
    slm-btrfs-layout/        # C++ module
      CMakeLists.txt
      SlmBtrfsLayoutJob.cpp
      SlmBtrfsLayoutJob.h
      module.desc
    slm-deploy/              # C++ module (unsquashfs wrapper)
      CMakeLists.txt
      SlmDeployJob.cpp
      SlmDeployJob.h
      module.desc
    slm-fstab/               # C++ module
    slm-bootloader/          # C++ module
    slm-recovery/            # C++ module
    slm-snapshot/            # C++ module
    slm-protected-packages/  # C++ module
    slm-firstboot/           # C++ module
  settings.conf              # urutan modul (lihat §5)
  branding/
    slm/
      branding.desc
      show.qml               # slide show (bukan UI utama)
      assets/
```

### Wrapper Package `slm-installer`

`slm-installer` adalah Debian package yang:
1. Depends on: `calamares`, `btrfs-progs`, `dosfstools`, `systemd-boot`, `efibootmgr`, `squashfs-tools`, `smartmontools`, `pciutils`, `usbutils`
2. Meng-install konfigurasi Calamares ke `/etc/calamares/` (settings.conf, modul)
3. Meng-install modul SLM ke `/usr/lib/calamares/modules/`
4. Meng-install helper scripts ke `/usr/libexec/slm-installer/`

---

## 2. Disk & Filesystem Provisioning

### UEFI-Only Partition Table

Semua disk target menggunakan **GPT**. MBR tidak pernah ditulis.

```bash
# Phase 1: Wipe dan buat GPT baru
sgdisk --zap-all /dev/TARGET

# Phase 2: Buat partisi
sgdisk \
  --new=1:0:+1G    --typecode=1:EF00 --change-name=1:"EFI System Partition" \
  --new=2:0:+8G    --typecode=2:8300 --change-name=2:"SLM Recovery" \
  --new=3:0:0      --typecode=3:8300 --change-name=3:"SLM Root" \
  /dev/TARGET

# Phase 3: Format
mkfs.fat -F32 -n "SLM-EFI" /dev/TARGET1
mkfs.ext4 -L "SLM-RECOVERY" /dev/TARGET2
# Recovery pakai ext4 bukan Btrfs — alasan: kesederhanaan, tidak ada dependency
# pada btrfs-progs saat recovery mode minimal

mkfs.btrfs -L "SLM-ROOT" -f /dev/TARGET3
```

Catatan: Recovery partition menggunakan ext4 secara sengaja. Ini memaksimalkan portabilitas initramfs recovery (busybox + ext4 sudah built-in di hampir semua kernel Ubuntu, tanpa perlu module btrfs di recovery initramfs).

### Btrfs Subvolume Creation Sequence

```bash
BTRFS_DEV=/dev/TARGET3
MNT=/mnt/slm-btrfs-scratch

mount "$BTRFS_DEV" "$MNT"

# Root subvolumes (urutan penting: @ harus dibuat pertama)
btrfs subvolume create "$MNT/@"
btrfs subvolume create "$MNT/@home"
btrfs subvolume create "$MNT/@var"
btrfs subvolume create "$MNT/@log"
btrfs subvolume create "$MNT/@cache"
btrfs subvolume create "$MNT/@snapshots"
btrfs subvolume create "$MNT/@resources"
btrfs subvolume create "$MNT/@recovery-staging"

umount "$MNT"
```

### Mount Sequence untuk Staging Area

```bash
ROOT=/mnt/slm-install-root

# Mount root subvolume
mount -o subvol=@,compress=zstd:3,noatime,ssd "$BTRFS_DEV" "$ROOT"

# Buat mountpoints di dalam @
mkdir -p "$ROOT"/{home,var,var/log,var/cache,boot/efi,.snapshots,opt/slm/resources}

# Mount subvolumes lain
mount -o subvol=@home,compress=zstd:3,noatime,ssd  "$BTRFS_DEV" "$ROOT/home"
mount -o subvol=@var,compress=zstd:3,noatime,ssd   "$BTRFS_DEV" "$ROOT/var"
mount -o subvol=@log,compress=zstd:3,noatime,ssd   "$BTRFS_DEV" "$ROOT/var/log"
mount -o subvol=@cache,compress=zstd:3,noatime,ssd "$BTRFS_DEV" "$ROOT/var/cache"
mount -o subvol=@snapshots,noatime,ssd             "$BTRFS_DEV" "$ROOT/.snapshots"
mount -o subvol=@resources,compress=zstd:3,noatime "$BTRFS_DEV" "$ROOT/opt/slm/resources"

# ESP
mount /dev/TARGET1 "$ROOT/boot/efi"
```

### Fstab Generation Rules

`slm-fstab` module menulis `/etc/fstab` dengan UUID (tidak pernah `/dev/sdX`). Format:

```
# SLM Desktop — generated by slm-installer, DO NOT EDIT MANUALLY
UUID=<btrfs-uuid>  /              btrfs  subvol=@,compress=zstd:3,noatime,ssd,space_cache=v2  0 0
UUID=<btrfs-uuid>  /home          btrfs  subvol=@home,compress=zstd:3,noatime,ssd              0 0
UUID=<btrfs-uuid>  /var           btrfs  subvol=@var,compress=zstd:3,noatime,ssd               0 0
UUID=<btrfs-uuid>  /var/log       btrfs  subvol=@log,compress=zstd:3,noatime,ssd               0 0
UUID=<btrfs-uuid>  /var/cache     btrfs  subvol=@cache,compress=zstd:3,noatime,ssd             0 0
UUID=<btrfs-uuid>  /.snapshots    btrfs  subvol=@snapshots,noatime,ssd                         0 0
UUID=<btrfs-uuid>  /opt/slm/resources  btrfs  subvol=@resources,compress=zstd:3,noatime       0 0
UUID=<esp-uuid>    /boot/efi      vfat   umask=0077,shortname=mixed                            0 2
```

`space_cache=v2` wajib untuk Btrfs >= 5.15 (default Ubuntu 26.04).

### ESP Reuse Detection Logic

```
IF /sys/firmware/efi/efivars EXISTS:
  Enumerate existing partitions via libparted or sgdisk --print
  FOR EACH partition where type == EF00:
    IF size >= 512MB AND filesystem == FAT32:
      Candidate for reuse
      IF existing ESP contains /EFI/ directory:
        WARN: "ESP already contains boot entries from another OS"
        OFFER: [Reuse ESP] [Wipe ESP] [Select Different Disk]
        DEFAULT: Reuse
      ELSE:
        Auto-reuse silently, mkfs.fat only if user chose Wipe
    ELSE:
      Mark as too-small, create new ESP on available space
```

Keputusan reuse disimpan di Calamares global storage key `slm.esp.reuse = true/false` untuk digunakan modul `slm-bootloader`.

---

## 3. UEFI / Boot Validation

### Runtime Checks (dilakukan di `slm-uefi-check` module, sebelum partisi)

| Check | Metode | Threshold | Aksi |
|---|---|---|---|
| UEFI boot aktif | `access("/sys/firmware/efi", F_OK)` | Wajib | BLOCK: "SLM Desktop requires modern UEFI firmware." |
| efivars writable | `access("/sys/firmware/efi/efivars", W_OK)` | Wajib | BLOCK: "EFI variables not accessible. Check firmware settings." |
| ESP capacity | `statvfs()` pada kandidat ESP | >= 512MB | BLOCK jika tidak ada kandidat; WARNING jika ada tapi < 1GB |
| Secure Boot state | Baca `SecureBoot-8be4df61-...` dari efivars | Info only | WARNING: "Secure Boot enabled. SLM will install unsigned shim." |
| Disk SMART health | `smartctl -H /dev/TARGET` | Passed | WARNING jika FAILED; tidak blokir install |
| RAM minimum | `/proc/meminfo` MemTotal | >= 2GB | WARNING; >= 512MB BLOCK |
| GPU Wayland capable | Lihat §8 | Info | WARNING saja |

### systemd-boot Installation Flow

```bash
ROOT=/mnt/slm-install-root

# Install bootloader ke ESP
chroot "$ROOT" bootctl install \
  --path=/boot/efi \
  --no-variables    # jangan tulis EFI variables dulu

# Tulis EFI variables setelah chroot selesai, dari live env
efibootmgr --create \
  --disk /dev/TARGET --part 1 \
  --label "SLM Desktop" \
  --loader '\EFI\systemd\systemd-bootx64.efi'
```

### loader.conf

```ini
# /boot/efi/loader/loader.conf
default  slm-desktop.conf
timeout  3
console-mode  auto
editor   no
```

### Boot Entry: SLM Desktop

```ini
# /boot/efi/loader/entries/slm-desktop.conf
title   SLM Desktop
linux   /EFI/slm/vmlinuz
initrd  /EFI/slm/initrd.img
options root=UUID=<btrfs-uuid> rootflags=subvol=@ rw quiet splash \
        loglevel=3 udev.log_level=3 \
        systemd.show_status=auto
```

### Boot Entry: SLM Recovery

```ini
# /boot/efi/loader/entries/slm-recovery.conf
title   SLM Recovery
linux   /EFI/slm-recovery/vmlinuz
initrd  /EFI/slm-recovery/initrd.img
options root=UUID=<recovery-uuid> rw quiet \
        systemd.unit=slm-recovery.target \
        slm.recovery=1
```

### Boot Entry: SLM Safe Mode

```ini
# /boot/efi/loader/entries/slm-safe-mode.conf
title   SLM Safe Mode
linux   /EFI/slm/vmlinuz
initrd  /EFI/slm/initrd.img
options root=UUID=<btrfs-uuid> rootflags=subvol=@ rw \
        slm.session=safe systemd.unit=graphical.target \
        nomodeset    # fallback GPU
```

`slm.session=safe` dibaca oleh `slm-session-broker` melalui `/proc/cmdline` dan mengeset `StartupMode::Safe` secara langsung tanpa membaca `safeModeForced` dari state file — ini bypass yang aman untuk kasus GPU rusak.

---

## 4. Recovery System Architecture

### Konten Recovery Partition (`/dev/TARGET2`, ext4, 8GB)

```
/recovery/
  boot/
    vmlinuz              # kernel sama dengan main, disalin saat install
    initrd.img           # initramfs recovery, dibuild via dracut dengan modul btrfs + squashfs
  rootfs/
    recovery.squashfs    # minimal rootfs: busybox + btrfs-progs + slm-recovery-app + util-linux
  factory/
    slm-factory.img.zst  # optional: compressed btrfs send stream dari @factory-initial
    slm-factory.img.zst.sha256
  logs/                  # installer logs disalin ke sini pasca-install
  metadata/
    install-manifest.json  # versi, tanggal, disk layout, subvolume UUIDs
    protected-packages.json  # salinan dari /usr/share/slm/
```

### Independence Guarantee

Recovery TIDAK pernah mount `@` (root Btrfs) untuk boot. Kernel dan initramfs di `/recovery/boot/` adalah file yang disalin secara fisik ke ext4 — bukan symlink ke `/boot/` di root Btrfs. `slm-recovery` module bertanggung jawab menyalin kernel + initramfs ke sini sebagai langkah terpisah.

### Snapshot Restore Mechanism

Dua strategi tersedia, dipilih berdasarkan severity kerusakan:

**Strategi A — Subvolume Swap (lebih cepat, untuk @root rusak ringan):**
```bash
# Dari recovery shell
btrfs subvolume snapshot /mnt/btrfs-root/@factory-initial \
  /mnt/btrfs-root/@.restore-candidate

# Rename atomis via btrfs
btrfs subvolume delete /mnt/btrfs-root/@
mv /mnt/btrfs-root/@.restore-candidate /mnt/btrfs-root/@
```

**Strategi B — btrfs receive dari factory image (kerusakan total, @ corrupted):**
```bash
# Dari recovery environment
zstd -d /recovery/factory/slm-factory.img.zst | btrfs receive /mnt/btrfs-root/
```

Strategi B membutuhkan `slm-factory.img.zst` ada di recovery partition. Ini opsional per spec; jika tidak ada, hanya Strategi A tersedia. `slm-recovery-app` (di `src/login/recovery-app/`) yang mengekspos pilihan ini ke user via UI.

### Factory Image Generation (post-install)

```bash
# Jalankan di akhir install, setelah @factory-initial dibuat
btrfs send /mnt/btrfs-root/@factory-initial | \
  zstd -T0 -c > /recovery/factory/slm-factory.img.zst

sha256sum /recovery/factory/slm-factory.img.zst \
  > /recovery/factory/slm-factory.img.zst.sha256
```

Trade-off: Menghasilkan ~3–6GB file. Hanya layak jika recovery partition 8GB+. Bisa dilewati jika disk kecil — `slm-snapshot` module harus memeriksa free space recovery sebelum memutuskan.

---

## 5. Atomic Installation Pipeline

### State Machine & Phase Markers

```
/tmp/slm-install-state/          # di live env, hilang saat reboot
  phase-provision.done
  phase-deploy.done
  phase-bootloader.done
  phase-recovery.done
  phase-finalize.done
  rollback-needed                # ditulis jika phase gagal
  error.json                     # structured error dari phase yang gagal
```

### Urutan Phase di `settings.conf`

```yaml
sequence:
  - show:
    - welcome          # hardware validation (slm-uefi-check, slm-hardware-probe)
    - disk-select      # UI: pilih disk target
    - summary          # konfirmasi
  - exec:
    - slm-uefi-check      # BLOCK jika non-UEFI
    - slm-hardware-probe  # collect hardware info, store di global storage
    - slm-partition       # sgdisk, mkfs.fat, mkfs.ext4, mkfs.btrfs
    - slm-btrfs-layout    # subvolume creation, mount staging
    - slm-deploy          # unsquashfs ke /mnt/slm-install-root
    - slm-fstab           # generate /etc/fstab
    - slm-bootloader      # bootctl install + entry generation
    - slm-recovery        # populate /recovery/, copy kernel
    - slm-snapshot        # btrfs snapshot @factory-initial, optionally factory image
    - slm-protected-packages  # generate protected-packages.json
    - slm-firstboot       # tulis /var/lib/slm/firstboot_pending
    - slm-finalize        # umount staging, sync, cleanup
  - show:
    - finished
```

### Rollback per Phase

| Phase | Yang Di-rollback |
|---|---|
| `slm-partition` | `sgdisk --zap-all TARGET` (jika disk baru); unmount ESP jika reuse |
| `slm-btrfs-layout` | `umount -R /mnt/slm-install-root`; `mkfs.btrfs -f` ulang jika perlu |
| `slm-deploy` | Hapus `/mnt/slm-install-root/*` — cukup karena staging belum di-finalize |
| `slm-bootloader` | `bootctl remove`; hapus entries; `efibootmgr -B` untuk entry yang dibuat |
| `slm-recovery` | `rm -rf /recovery/*` |
| `slm-snapshot` | `btrfs subvolume delete @factory-initial` jika ada |
| `slm-finalize` | Tidak ada rollback — jika gagal di sini, installer log saja dan report |

Setiap Calamares C++ Job memiliki `rollback()` method yang dipanggil oleh Calamares jika job berikutnya gagal.

---

## 6. Protected Packages System

### Schema `/usr/share/slm/protected-packages.json`

```json
{
  "schema_version": "1.0",
  "generated_at": "2026-05-22T10:00:00Z",
  "installer_version": "1.0.0",
  "categories": {
    "bootloader": {
      "description": "Boot and EFI components",
      "packages": [
        { "name": "systemd-boot-unsigned", "min_version": "255.0" },
        { "name": "efibootmgr", "min_version": "18" }
      ]
    },
    "kernel": {
      "description": "Linux kernel and modules",
      "packages": [
        { "name": "linux-image-generic", "min_version": null },
        { "name": "linux-modules-extra-generic", "min_version": null }
      ]
    },
    "filesystem": {
      "description": "Root filesystem tools",
      "packages": [
        { "name": "btrfs-progs", "min_version": "6.0" }
      ]
    },
    "display": {
      "description": "Wayland and GPU stack",
      "packages": [
        { "name": "libwayland-client0", "min_version": null },
        { "name": "mesa-vulkan-drivers", "min_version": null }
      ]
    },
    "shell": {
      "description": "SLM Desktop Shell core",
      "packages": [
        { "name": "slm-desktop", "min_version": null },
        { "name": "slm-greeter", "min_version": null },
        { "name": "greetd", "min_version": null }
      ]
    }
  }
}
```

### Cara Generasi

`slm-protected-packages` module men-generate file ini dengan cara:

1. Ambil daftar packages yang ter-install di staging root: `chroot /mnt/slm-install-root dpkg-query -W --showformat='${Package}\t${Version}\n'`
2. Filter berdasarkan curated category list yang di-hardcode di module (bukan dari dpkg metadata semata — dpkg tidak tahu mana yang "critical")
3. Tulis file ke `$ROOT/usr/share/slm/protected-packages.json` dan salin ke `/recovery/metadata/protected-packages.json`

### Integration dengan `recoveryd`

`recoveryd` yang ada (di `src/daemon/recoveryd/`) sudah memiliki `RequestRecoveryPartition` dan `GetStatus`. Extend interface D-Bus `org.slm.Desktop.Recovery` dengan method:

```
GetProtectedPackages() → QVariantList
ValidateSystemIntegrity() → QVariantMap  # bandingkan dpkg state vs protected list
```

---

## 7. Snapshot & Firstboot

### Btrfs Snapshot `@factory-initial`

```bash
# Dari slm-snapshot module, setelah slm-deploy selesai
btrfs subvolume snapshot -r \
  /mnt/slm-install-root \
  /mnt/slm-btrfs-root/@factory-initial

# -r = read-only snapshot; ini penting agar btrfs send bisa berjalan
# Mount point sementara untuk akses ke top-level subvolume:
mount -o subvolid=5 /dev/TARGET3 /mnt/slm-btrfs-root
```

Snapshot ini **read-only** secara intentional. Jika recovery butuh restore, dia snapshot `@factory-initial` → `@` baru (read-write). Ini mengikuti pola COW Btrfs yang benar.

### Marker `firstboot_pending`

```
/var/lib/slm/firstboot_pending
```

Format: JSON file kecil (bukan empty file) agar bisa dibaca versi baru dengan metadata:

```json
{
  "created_at": "2026-05-22T10:00:00Z",
  "installer_version": "1.0.0",
  "install_id": "uuid-v4-random",
  "oobe_required": true
}
```

**Siapa yang menulis**: `slm-firstboot` Calamares module, ditulis ke `$ROOT/var/lib/slm/firstboot_pending` sebelum `slm-finalize`.

**Siapa yang membaca**: `slm-session-broker` (di `src/login/session-broker/sessionbroker.cpp`). Session broker harus ditambahkan deteksi:

```cpp
bool SessionBroker::isFirstBoot() const {
    return QFile::exists(ConfigManager::stateDir() + "/firstboot_pending");
}
```

Jika `isFirstBoot()` true, session broker meneruskan info ini ke `slm-session` yang kemudian meluncurkan `slm-welcome` setelah desktop ready.

**Siapa yang menghapus**: `slm-welcome` menghapus marker setelah OOBE selesai (atau user skip). Desktop harus tetap accessible jika file ini ada tapi OOBE crash — sesuai spec OOBE design principles.

---

## 8. Hardware / Driver Detection

### Detection Strategy (`slm-hardware-probe` module)

Dilakukan di awal pipeline (sebelum partition), karena hasilnya disimpan di Calamares global storage dan digunakan UI untuk menampilkan warning.

```
GPU Detection:
  lspci -nn | grep -i "VGA\|3D\|Display"
  → Map PCI ID ke vendor (NVIDIA: 10de:, AMD: 1002:, Intel: 8086:)
  → Cek /proc/driver/nvidia jika nvidia.ko loaded
  → Cek DRM subsystem: ls /sys/class/drm/

Broadcom Detection:
  lspci -nn | grep -i "Broadcom\|14e4:"
  lsusb | grep -i "Broadcom"

Wayland Capability:
  Cek kernel module: modinfo drm (selalu ada di Ubuntu 26.04)
  Cek libwayland: ldconfig -p | grep libwayland
  Cek DRM master capable: cat /sys/class/drm/card*/dev (harus non-empty)
  → Hasil: capable/limited/unsupported

Secure Boot:
  Baca /sys/firmware/efi/vars/SecureBoot-*/data byte[4]
  0x00 = disabled, 0x01 = enabled
```

### Driver Packages (Offline Mode)

Paket proprietary driver dibundle di live ISO sebagai squashfs terpisah:

```
/iso/
  slm-drivers-optional.squashfs
    nvidia/
      nvidia-driver-xxx_amd64.deb
      nvidia-dkms-xxx_amd64.deb
    broadcom/
      bcmwl-kernel-source_xxx_amd64.deb
```

Installer mount squashfs ini di `/opt/slm-drivers/` dan menjalankan `dpkg -i` ke staging root jika user memilih driver. Jika ISO tidak punya squashfs ini (minimal ISO), installer menawarkan download online (opsional, tidak blokir install).

### API Surface ke UI Layer

`slm-hardware-probe` menulis ke Calamares global storage dengan keys:

```
slm.hw.gpu.vendor         = "nvidia" | "amd" | "intel" | "unknown"
slm.hw.gpu.wayland        = "capable" | "limited" | "unsupported"
slm.hw.gpu.proprietary_available = true | false
slm.hw.broadcom.detected  = true | false
slm.hw.uefi.secure_boot   = "enabled" | "disabled" | "unknown"
slm.hw.ram.mb             = <integer>
slm.hw.disk.smart_ok      = true | false
slm.hw.warnings           = ["warn-code-1", "warn-code-2"]   # list
slm.hw.blocks             = ["block-code-1"]                  # kosong = boleh lanjut
```

UI layer (designer agent) tinggal membaca keys ini dari Calamares global storage via QML `Calamares.globalStorage` untuk merender warning/block screens.

---

## 9. Logging & Error Handling

### Log Schema (JSON Lines)

Setiap line di `/var/log/slm-installer.log` adalah satu JSON object:

```json
{"ts":"2026-05-22T10:01:23.456Z","level":"info","phase":"slm-partition","code":"PART_001","msg":"Creating GPT partition table on /dev/sda","detail":{"disk":"/dev/sda","size_gb":256}}
{"ts":"2026-05-22T10:01:25.123Z","level":"error","phase":"slm-btrfs-layout","code":"BTRFS_002","msg":"Unable to create Btrfs filesystem on selected partition.","detail":{"raw":"mkfs.btrfs: error -5 (Input/output error)","device":"/dev/sda3"},"action":"Check disk health. Try running disk diagnostics.","retryable":true}
```

Fields wajib: `ts`, `level` (debug/info/warn/error/fatal), `phase`, `code`, `msg`.
Fields opsional: `detail` (raw error + context), `action` (user-facing suggestion), `retryable`.

### Error Code Taxonomy

| Range | Category | Contoh |
|---|---|---|
| UEFI_001–099 | UEFI/EFI validation | UEFI_001: Non-UEFI system |
| PART_001–099 | Partitioning | PART_002: Disk too small |
| BTRFS_001–099 | Btrfs operations | BTRFS_001: mkfs failed |
| DEPLOY_001–099 | Package deployment | DEPLOY_001: squashfs not found |
| BOOT_001–099 | Bootloader | BOOT_001: bootctl install failed |
| RCVR_001–099 | Recovery setup | RCVR_001: kernel copy failed |
| SNAP_001–099 | Snapshot | SNAP_001: btrfs snapshot failed |
| HW_001–099 | Hardware detection | HW_001: SMART failure |

### Raw Error Translation

`SlmErrorTranslator` static class di C++:

```cpp
// src/installer/calamares-modules/shared/SlmErrorTranslator.cpp
struct SlmError {
    QString code;
    QString userMessage;
    QString suggestedAction;
    bool retryable;
};

SlmError translate(const QString &phase, int exitCode, const QString &rawOutput);
```

Mapping dilakukan via table (bukan regex) untuk determinisme. Raw output tetap dicatat di field `detail.raw` untuk diagnostik, tapi tidak pernah ditampilkan ke user.

---

## 10. Future-Ready Hooks

### TPM2 / LUKS2

`slm-partition` module sudah harus mengalokasikan LUKS header space jika opsi enkripsi diaktifkan kelak. Saat ini: tidak diimplementasikan, tapi **jangan hardcode asumsi "no encryption"**. Gunakan flag:

```cpp
bool SlmPartitionJob::encryptionEnabled() const {
    return Calamares::JobQueue::instance()->globalStorage()
        ->value("slm.install.luks2").toBool();  // false untuk sekarang
}
```

`slm-bootloader` module harus menyimpan kernel cmdline di helper file terpisah (`/etc/kernel/cmdline`) bukan hanya di loader entries — ini persiapan untuk `ukify` / UKI (Unified Kernel Images) yang dibutuhkan TPM2 measured boot.

### Immutable Root Preparation

Buat `@resources` subvolume sebagai **read-only setelah deploy**:

```bash
btrfs property set /mnt/slm-install-root/opt/slm/resources ro true
```

Ini bukan immutable root penuh, tapi mempersiapkan mental model bahwa `/opt/slm/resources` adalah "system assets" yang tidak boleh dimodifikasi user. Ketika overlay-based update system datang, `@resources` adalah kandidat pertama yang dijadikan immutable layer.

Juga: `@recovery-staging` subvolume sudah dibuat tapi belum di-mount di sistem normal — ini reserved untuk atomic update staging tanpa mount di fstab normal.

### Cloud Recovery Hooks

`/recovery/metadata/install-manifest.json` (lihat §4) harus berisi `install_id` (UUID v4) yang dibuat saat install. Ini menjadi identifier untuk cloud recovery service kelak. Field yang perlu ada sekarang:

```json
{
  "install_id": "uuid-v4",
  "slm_version": "1.0.0",
  "install_date": "2026-05-22T10:00:00Z",
  "disk_layout": { "esp_uuid": "...", "root_uuid": "...", "recovery_uuid": "..." },
  "kernel_version": "6.11.0-15-generic",
  "btrfs_subvolumes": ["@", "@home", "@var", "@log", "@cache", "@snapshots", "@resources", "@recovery-staging"],
  "cloud_recovery_endpoint": null
}
```

---

## 11. Package & Service Layout

### Debian Package Structure

```
slm-installer (source package)
  → slm-installer (binary): calamares + modules + branding + configs
  → slm-installer-dbg (binary): debug symbols

Build-Depends: calamares-dev, qt6-base-dev, qt6-declarative-dev,
               libbtrfsutil-dev, libparted-dev, libblkid-dev,
               libsystemd-dev, cmake, debhelper-compat (=13)

Depends (runtime): calamares (>= 3.3), btrfs-progs (>= 6.0),
                   dosfstools, efibootmgr, systemd-boot,
                   squashfs-tools, smartmontools, pciutils
```

### systemd Units (Live ISO Session)

Unit-unit ini hanya aktif selama live session, **tidak** diinstall ke target sistem:

```
slm-installer-prepare.service
  Description=Prepare SLM Installer environment
  Type=oneshot
  ExecStart=/usr/libexec/slm-installer/prepare-live-env.sh
  # Tugas: mount slm-drivers-optional.squashfs, load btrfs module,
  #        pastikan efivars writable
  RemainAfterExit=yes
  Before=calamares.service

calamares.service
  Description=SLM Installer (Calamares)
  After=graphical-session.target slm-installer-prepare.service
  ExecStart=/usr/bin/calamares -D 6
```

### Lokasi di Repository

```
src/installer/               # kode C++ modules (baru, belum ada)
scripts/installer/           # helper shell scripts untuk live env
  prepare-live-env.sh
  request-bootloader-recovery.sh  # sudah ada pattern di recoveryd
packaging/slm-installer/     # debian/ directory, CMakeLists untuk packaging
```

ISO build process sebaiknya tinggal di **repository terpisah** (`slm-iso-build` atau `slm-distro`) yang meng-include `slm-installer` sebagai dependency, bukan di repo ini. Alasan: ISO build menarik dependency besar (live-build/cubic/ubuntu-image) yang tidak relevan untuk development shell sehari-hari.

---

## 12. Open Questions / Risks

1. **Curated protected-packages list**: Siapa yang menentukan dan memaintain daftar ini? Perlu keputusan: apakah di-hardcode di module C++ (mudah di-review via PR), atau file eksternal di `/usr/share/slm/installer/protected-packages-template.json` (lebih fleksibel)?

2. **ISO build tooling**: `live-build` (Debian standard), `cubic` (GUI, kurang automation-friendly), atau `ubuntu-image` (untuk Core snaps, probably overkill)? Rekomendasi: `live-build` untuk kontrol penuh dan CI-friendliness.

3. **Secure Boot signing strategy**: SLM perlu signed shim (dari Ubuntu) atau self-signed MOK? Tanpa ini, Secure Boot enabled systems akan block booting. Keputusan ini mempengaruhi packaging dan release pipeline.

4. **squashfs source**: Apakah rootfs squashfs dibuild dari Ubuntu base + SLM packages (via live-build), atau dari installed system yang di-snapshot? Format dan mount path harus disepakati sebelum `slm-deploy` module ditulis.

5. **Recovery partition size policy**: 8GB cukup untuk kernel + recovery rootfs + factory image ~3–5GB. Jika disk target < 50GB, policy perlu menyesuaikan — apakah factory image dilewati? Berapa minimum disk size yang di-block?

6. **`@factory-initial` vs live snapshot**: Apakah `@factory-initial` adalah snapshot dari squashfs deploy (tanpa `/home`, `/var`), atau snapshot lengkap setelah firstboot selesai? Spec mengatakan "after successful installation" — ini berarti **sebelum** firstboot, yang adalah keputusan yang tepat untuk clean recovery baseline.

7. **Calamares versi minimum**: Ubuntu 26.04 menyertakan Calamares versi berapa? Perlu dicek apakah API C++ modules kompatibel atau perlu backport.

---

## Ringkasan Dependensi Antar Komponen

```
slm-hardware-probe ──writes──► Calamares GlobalStorage
                                    │
                    ┌───────────────▼──────────────────┐
                    │  UI Layer (designer agent reads)  │
                    └───────────────────────────────────┘

slm-partition ──creates──► /dev/TARGET1 (ESP)
              ──creates──► /dev/TARGET2 (Recovery, ext4)
              ──creates──► /dev/TARGET3 (Root, Btrfs)
                                    │
slm-btrfs-layout ──mounts──► /mnt/slm-install-root (@ subvol)
                                    │
slm-deploy ──deploys──► /mnt/slm-install-root/
                                    │
slm-bootloader ──installs──► /mnt/slm-install-root/boot/efi/
                                    │
slm-recovery ──populates──► /recovery/ (ext4 partition)
                                    │
slm-snapshot ──creates──► @factory-initial (read-only Btrfs snapshot)
                          ──optionally──► /recovery/factory/slm-factory.img.zst
                                    │
slm-protected-packages ──writes──► $ROOT/usr/share/slm/protected-packages.json
                        ──copies──► /recovery/metadata/protected-packages.json
                                    │
slm-firstboot ──writes──► $ROOT/var/lib/slm/firstboot_pending
                                    │
slm-finalize ──unmounts──► all staging mounts ──► install complete

─────────────── POST-REBOOT ──────────────────────────────────
session-broker reads firstboot_pending → launches slm-welcome
recoveryd D-Bus API ← reads protected-packages.json for integrity validation
```
