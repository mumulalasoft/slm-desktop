# QEMU Dev Workflow

Tooling ini menyiapkan VM Ubuntu lokal untuk iterasi Desktop Shell dengan ISO di `~/ubuntu.iso`.

Ringkasan dependency guest dan runtime login ada di [docs/DEPENDENCIES.md](/home/garis/Development/Qt/Desktop_Shell/docs/DEPENDENCIES.md).

## File

- `scripts/dev/qemu-create-disk.sh`: buat disk `qcow2` persisten.
- `scripts/dev/qemu-run.sh`: jalankan VM dengan port forward SSH dan shared folder ke repo ini.
- `scripts/dev/qemu-ssh.sh`: SSH helper ke guest melalui `127.0.0.1:2222`.
- `scripts/dev/qemu-scp.sh`: SCP helper ke guest melalui `127.0.0.1:2222`.
- `scripts/dev/qemu-guest-bootstrap.sh`: bootstrap dependency di guest.
- `scripts/dev/qemu-guest-build.sh`: configure/build project dari dalam guest.
- `scripts/dev/qemu-bootstrap-remote.sh`: jalankan bootstrap guest dari host via SSH.
- `scripts/dev/qemu-install-deps-remote.sh`: wrapper kompatibilitas untuk `qemu-bootstrap-remote.sh --apt-only`.
- `scripts/dev/qemu-build-remote.sh`: canonical host->guest build entrypoint.
- `scripts/dev/qemu-session-smoke-remote.sh`: jalankan smoke login/session di guest dan tarik artefaknya ke host.
- `scripts/dev/qemu-login-smoke-pipeline.sh`: build target runtime via entrypoint canonical, install runtime login, verify, lalu opsional jalankan smoke session.

Launcher QEMU juga mengaktifkan clipboard sharing host↔guest secara default bila backend `qemu-vdagent` tersedia di host.

## Alur cepat

1. Buat disk:

```bash
bash scripts/dev/qemu-create-disk.sh
```

2. Boot installer Ubuntu:

```bash
bash scripts/dev/qemu-run.sh --with-iso
```

3. Setelah Ubuntu terpasang di disk, boot normal:

```bash
bash scripts/dev/qemu-run.sh
```

4. SSH ke guest:

```bash
bash scripts/dev/qemu-ssh.sh --user <username-guest>
```

5. Bootstrap guest langsung dari host:

```bash
bash scripts/dev/qemu-bootstrap-remote.sh --user <username-guest>
```

Script ini akan:

- mount `hostshare` ke `/mnt/hostshare`
- install dependency build Ubuntu yang dibutuhkan
- enable service `ssh`

Wrapper remote mengirim script bootstrap langsung via SSH, jadi tidak bergantung pada `/mnt/hostshare` sudah ter-mount.

Kalau Anda hanya ingin memasang dependency dev:

```bash
bash scripts/dev/qemu-install-deps-remote.sh --user <username-guest>
```

Itu alias kompatibilitas untuk:

```bash
bash scripts/dev/qemu-bootstrap-remote.sh --user <username-guest> --apt-only
```

6. Build dari guest:

```bash
bash /mnt/hostshare/scripts/dev/qemu-guest-build.sh
```

Atau kalau sudah SSH ke guest, Anda bisa build target lain:

```bash
bash /mnt/hostshare/scripts/dev/qemu-guest-build.sh --target desktopd
```

Kalau ingin build langsung dari host:

```bash
bash scripts/dev/qemu-build-remote.sh --user <username-guest>
```

Argumen setelah `--` akan diteruskan ke `scripts/dev/qemu-guest-build.sh` di guest:

```bash
bash scripts/dev/qemu-build-remote.sh --user <username-guest> -- --target desktopd --jobs 4
```

Wrapper ini memaksa TTY SSH supaya `sudo` di guest bisa meminta password bila perlu.
Semua pipeline build host->guest lain memanggil script ini, jadi jalur build tetap satu.

Default build dir guest sekarang memakai direktori writable milik user:

```bash
~/.cache/slm-qemu/build/dev
```

Ini sengaja menghindari masalah permission bila repo host di-mount ke `/mnt/hostshare` dan parent path `/mnt/build` tidak writable untuk user guest.

## Session Smoke QEMU

Untuk lane `login-qemu-session-smoke`, jalankan dari host:

```bash
bash scripts/dev/qemu-session-smoke-remote.sh --user <username-guest> --session-user <username-desktop>
```

Script ini akan:

- upload runner smoke ke guest
- menjalankan validasi lewat `sudo`
- menunggu `~/.config/slm-desktop/state.json` menjadi `healthy`
- mengumpulkan oracle guest:
  - `state.json`
  - `journalctl -b -u greetd`
  - `journalctl` untuk UID user desktop
  - `ps -ef`
  - status `greetd`
  - deteksi process `slm-greeter`, `slm-session-broker`, `slm-watchdog`, `slm-shell`
  - deteksi socket `wayland-*`
- menarik artefak ke host di `artifacts/qemu-session-smoke/<timestamp>/`

Kalau ingin menjadikan process oracle sebagai hard failure:

```bash
bash scripts/dev/qemu-session-smoke-remote.sh --user <username-guest> --strict-process
```

Untuk pipeline yang lebih lengkap dari host:

```bash
bash scripts/dev/qemu-login-smoke-pipeline.sh --user <username-guest> --session-user <username-desktop> --skip-smoke
```

Itu akan:

- mount `hostshare` via bootstrap remote
- configure build dir guest lewat `qemu-build-remote.sh`
- build target login/runtime yang dibutuhkan
- install runtime SLM ke guest
- menjalankan:
  - `scripts/login/verify-slm-desktop-runtime.sh`
  - `scripts/login/verify-greetd-slm.sh`

Setelah guest reboot/login lewat greetd atau autologin test path siap, jalankan smoke:

```bash
bash scripts/dev/qemu-session-smoke-remote.sh --user <username-guest> --session-user <username-desktop>
```

## Default yang dipakai

- ISO: `~/ubuntu.iso`
- Disk: `~/.local/state/slm-qemu/ubuntu-dev.qcow2`
- RAM: `8192` MB
- CPU: `4`
- SSH forward: host `127.0.0.1:2222` ke guest `:22`
- Shared folder host: root repo Desktop Shell ini
- Mount tag shared folder di guest: `hostshare`
- Clipboard sharing: aktif default untuk mode GUI

## Mount repo host di guest

Jalankan di Ubuntu guest:

```bash
sudo mkdir -p /mnt/hostshare
sudo mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/hostshare
```

Lalu repo host akan tersedia di `/mnt/hostshare`.

Kalau Anda sudah masuk ke guest dan ingin menjalankan bootstrap dari sana tanpa mengetik banyak:

```bash
bash /mnt/hostshare/scripts/dev/qemu-guest-bootstrap.sh
```

Sesudah bootstrap, build standar:

```bash
bash /mnt/hostshare/scripts/dev/qemu-guest-build.sh
```

## Override konfigurasi

Semua script mendukung override via environment variable:

```bash
SLM_QEMU_MEMORY_MB=12288 \
SLM_QEMU_CPUS=8 \
SLM_QEMU_SSH_PORT=2223 \
bash scripts/dev/qemu-run.sh
```

Variable yang didukung:

- `SLM_QEMU_STATE_DIR`
- `SLM_QEMU_ISO`
- `SLM_QEMU_DISK`
- `SLM_QEMU_DISK_SIZE`
- `SLM_QEMU_SHARED_DIR`
- `SLM_QEMU_MEMORY_MB`
- `SLM_QEMU_CPUS`
- `SLM_QEMU_SSH_PORT`
- `SLM_QEMU_SSH_USER`
- `SLM_QEMU_DISPLAY`
- `SLM_QEMU_GUEST_REPO_DIR`
- `SLM_QEMU_GUEST_BUILD_DIR`
- `SLM_QEMU_GUEST_GENERATOR`
- `SLM_QEMU_GUEST_BUILD_TYPE`
- `SLM_QEMU_GUEST_TARGET`
- `SLM_QEMU_GUEST_JOBS`

## Catatan

- Script akan memakai KVM jika `/dev/kvm` tersedia.
- Script akan memakai OVMF UEFI jika firmware tersedia di host, dan fallback ke BIOS bila tidak ada.
- Default launcher tidak memasang ISO. Gunakan `--with-iso` atau `--iso /path/to/file.iso` bila ingin boot installer.
- Clipboard sharing memakai `qemu-vdagent` di host dan `spice-vdagent` di guest.
- Bila perlu mematikannya, jalankan `bash scripts/dev/qemu-run.sh --no-clipboard`.
- Pada Ubuntu 26.04, source installer sering muncul sebagai `/etc/apt/sources.list.d/cdrom.sources`.
  Bootstrap guest akan menonaktifkan file itu sebelum `apt update`.
