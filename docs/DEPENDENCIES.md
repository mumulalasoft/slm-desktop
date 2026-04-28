# Dependency Map

Dokumen ini merangkum dependency yang saat ini dipakai SLM Desktop untuk:

- build/development di Ubuntu guest
- runtime login stack (`greetd -> cage -> slm-greeter`)
- fitur opsional yang hanya aktif bila paketnya tersedia

## Build Core

Daftar paket Ubuntu yang saat ini dipasang oleh [scripts/dev/qemu-guest-bootstrap.sh](/home/garis/Development/Qt/Desktop_Shell/scripts/dev/qemu-guest-bootstrap.sh:8):

```text
build-essential
ccache
cmake
git
libdbus-1-dev
libglib2.0-dev
libgtk-3-dev
libpipewire-0.3-dev
libpulse-dev
libqt6svg6-dev
libspa-0.2-dev
libsystemd-dev
libwayland-dev
libxkbcommon-dev
ninja-build
openssh-server
pkg-config
spice-vdagent
qml6-module-qt-labs-folderlistmodel
qml6-module-qtquick
qml6-module-qtquick-controls
qml6-module-qtquick-dialogs
qml6-module-qtquick-layouts
qml6-module-qtquick-window
qt6-base-dev
qt6-base-private-dev
qt6-declarative-dev
qt6-declarative-private-dev
qt6-shadertools-dev
qt6-tools-dev
qt6-tools-dev-tools
qt6-wayland-dev
wayland-protocols
```

Catatan:

- `qt6-base-private-dev` dibutuhkan karena repo memakai `Qt6::GuiPrivate`.
- `qt6-declarative-private-dev` dibutuhkan untuk bagian Qt Quick/QML private API yang diekspor oleh Qt repo Ubuntu.
- `qml6-module-qt-labs-folderlistmodel` dibutuhkan untuk `FolderListModel`.
- `spice-vdagent` dipakai untuk clipboard sharing guest pada workflow QEMU.

## Optional Build Features

Paket berikut saat ini diperlakukan opsional di bootstrap guest:

```text
libarchive-dev
libpam0g-dev
libpolkit-agent-1-dev
libpolkit-gobject-1-dev
libpoppler-qt6-dev
qml6-module-qt5compat
```

Implikasinya:

- `libarchive-dev`: backend archive/file ops tambahan
- `libpam0g-dev`: dukungan PAM
- `libpolkit-agent-1-dev` dan `libpolkit-gobject-1-dev`: dukungan polkit agent
- `libpoppler-qt6-dev`: integrasi Poppler Qt6
- `qml6-module-qt5compat`: modul QML opsional; tidak lagi dianggap blocker bootstrap

## CMake Requirements

Sumber yang lebih otoritatif dari sisi build ada di [CMakeLists.txt](/home/garis/Development/Qt/Desktop_Shell/CMakeLists.txt:7).

Qt yang dicari:

```text
Qt6 6.10 REQUIRED COMPONENTS:
  Gui Quick QuickControls2 QuickLayouts
  Network DBus Test Sql Concurrent ShaderTools

Qt6 OPTIONAL_COMPONENTS:
  WaylandClient GuiPrivate
```

Native/system package yang dicari via `pkg-config` atau finder lain:

```text
gio-unix-2.0
wayland-client
wayland-protocols
wayland-scanner
libpipewire-0.3
libspa-0.2
pam
polkit-agent-1
polkit-gobject-1
libarchive
poppler-qt6
```

## Login Runtime

Untuk stack login di guest, dependency runtime minimumnya adalah:

```text
greetd
cage
systemd-logind
```

Tambahan penting:

- `slm-greeter` dijalankan oleh `greetd`
- `cage` dipakai sebagai compositor kiosk untuk greeter
- backend seat saat ini dipaksa ke `logind`, bukan `seatd`
- renderer wlroots saat ini dipaksa ke `pixman`

Konfigurasi ini ditulis oleh [scripts/login/install-greetd-slm.sh](/home/garis/Development/Qt/Desktop_Shell/scripts/login/install-greetd-slm.sh:61) melalui environment:

```text
LIBSEAT_BACKEND=logind
WLR_RENDERER=pixman
QT_QPA_PLATFORM=wayland
QT_QUICK_BACKEND=software
LIBGL_ALWAYS_SOFTWARE=1
```

Arti praktisnya:

- `logind`: jalur seat/session management yang sekarang dipakai untuk greeter
- `pixman`: renderer software wlroots untuk menghindari masalah EGL/DRM virtio GPU di QEMU
- `QT_QUICK_BACKEND=software` dan `LIBGL_ALWAYS_SOFTWARE=1`: mengurangi ketergantungan pada akselerasi grafis guest

## QEMU Workflow Notes

Untuk workflow QEMU, paket dan asumsi tambahan yang relevan:

- `openssh-server`: remote bootstrap/build/smoke via SSH
- `spice-vdagent`: clipboard sharing guest
- `9p` host share: repo host dimount ke `/mnt/hostshare`
- build output guest default: `~/.cache/slm-qemu/build/dev`

Referensi tooling:

- [docs/QEMU_DEV.md](/home/garis/Development/Qt/Desktop_Shell/docs/QEMU_DEV.md)
- [docs/GREETER_TO_DESKTOP_TEST_ARCHITECTURE.md](/home/garis/Development/Qt/Desktop_Shell/docs/GREETER_TO_DESKTOP_TEST_ARCHITECTURE.md)
