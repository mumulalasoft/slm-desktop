# Structure Migration TODO

Checklist migrasi struktur proyek SLM Desktop.

## Stage 1 - Bootstrap (DONE)

- [x] Buat folder `src/bootstrap/`
- [x] Pindahkan:
  - [x] `appstartupargs.*`
  - [x] `appstartupbridge.*`
- [x] Update include path di:
  - [x] `main.cpp`
  - [x] `tests/appstartupargs_test.cpp`
- [x] Update `CMakeLists.txt`

## Stage 2 - Services Layer (DONE)

- [x] Buat folder:
  - [x] `src/services/network/`
  - [x] `src/services/bluetooth/`
  - [x] `src/services/power/`
  - [x] `src/services/sound/`
  - [x] `src/services/notifications/`
- [x] Pindahkan `networkmanager.*`, `bluetoothmanager.*`, `batterymanager.*`, `soundmanager.*`, `notificationmanager.*`, `mediasessionmanager.*`
- [x] Update CMake + include
- [x] Build + smoke popup indikator

## Stage 3 - File Operations Domain (DONE)

- [x] Buat folder `src/filemanager/ops/`
- [x] Pindahkan `fileoperations*`, `globalprogresscenter.*`, `fileoperationserrors.h`
- [x] Build `desktopd`, `slm-fileopsd`, `appSlm_Desktop`
- [x] Jalankan test file operation DBus

## Stage 4 - Daemons and CLI (DONE)

- [x] Buat folder:
  - [x] `src/daemon/desktopd/`
  - [x] `src/daemon/fileopsd/`
  - [x] `src/daemon/devicesd/`
  - [x] `src/daemon/portald/`
  - [x] `src/cli/`
- [x] Pindahkan entrypoint `*_main.cpp` daemon
- [x] Pindahkan `*ctl.cpp` tools
- [x] Update install scripts jika perlu

## Stage 5 - CMake Hygiene

- [x] Pecah source list ke `cmake/Sources.cmake`
- [x] Pakai grouping per domain
- [x] Kurangi panjang `CMakeLists.txt` root
