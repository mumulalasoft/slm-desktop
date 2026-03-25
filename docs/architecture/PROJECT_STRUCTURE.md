# SLM Desktop Project Structure (Target)

Dokumen ini mendefinisikan struktur proyek ideal agar codebase mudah dipelihara, diskalakan, dan diaudit.

## Masalah Struktur Saat Ini

- Banyak file `*.cpp/*.h` masih berada di root repository.
- Boundary modul belum tegas (UI shell, services, daemon, tooling bercampur di level yang sama).
- Onboarding dan ownership file menjadi sulit.

## Struktur Target (Ideal)

```text
Slm_Desktop/
  cmake/
    Modules/
    Sources.cmake
  src/
    app/
      main.cpp
      bootstrap/
      runtime/
    core/
      execution/
      workspace/
      prefs/
      icons/
    services/
      system/
      media/
      notifications/
      network/
      bluetooth/
      power/
    filemanager/
      api/
      model/
      ops/
    search/
      global/
      tothespot/
    portal/
      core/
      bridge/
    daemon/
      desktopd/
      fileopsd/
      devicesd/
      portald/
    cli/
      indicatorctl/
      windowingctl/
      fileopctl/
      devicectl/
      workspacectl/
  Qml/
    components/
      topbar/
      dock/
      filemanager/
      shell/
  Style/
  scripts/
    systemd/
    polkit/
    dev/
  docs/
    architecture/
    api/
    operations/
  tests/
    unit/
    integration/
    smoke/
  compositor/
  themes/
  icons/
```

## Prinsip Penataan

1. `src/app` hanya orchestration startup.
2. `src/core` untuk domain logic lintas fitur.
3. `src/services` untuk adapter state sistem (network, battery, bluetooth, audio, dsb).
4. `src/daemon` terpisah per proses DBus service.
5. `src/cli` untuk utilitas command-line.
6. `tests` dipisah berdasarkan level (unit/integration/smoke).

## Tahap Migrasi Aman

1. Pindahkan file bootstrap startup ke `src/bootstrap` (DONE tahap 1).
2. Pindahkan service adaptor (`*manager`) ke `src/services/*`.
3. Pindahkan file operation stack ke `src/filemanager/ops`.
4. Pindahkan daemon entrypoint dan service ke `src/daemon/*`.
5. Pindahkan CLI ke `src/cli/*`.
6. Ekstrak daftar source ke `cmake/Sources.cmake`.

Setiap tahap wajib:
- build `appSlm_Desktop`
- build daemon terkait
- jalankan smoke tests penting

