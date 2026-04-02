# FileManager Integration

Panduan integrasi `slm-filemanager` dengan desktop shell.

## Arsitektur Singkat

```
Desktop Shell
  └─ FileManagerShellBridge  (in-process, C++)
       ├─ In-process: FileManagerApi (listing, search, sidebar, thumbnails)
       └─ Cross-process: DBus
            ├─ org.slm.Desktop.FileManager1   (navigasi, open path)
            └─ org.slm.Desktop.FileOperations (copy, move, trash → fileopsd)
```

## Setup Dev Mode (Workspace Lokal)

### Struktur direktori yang diharapkan
```
/workspace/
  desktop-shell/
  filemanager/
```

### Build awal
```bash
# Build filemanager
cmake -B filemanager/build/dev \
  -S filemanager \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=filemanager/build/dev/install
cmake --build filemanager/build/dev -j$(nproc)
cmake --install filemanager/build/dev

# Build desktop shell dengan local filemanager
cmake --preset dev-local-fm desktop-shell/
cmake --build desktop-shell/build/dev-local-fm -j$(nproc)
```

### Jalankan dev session
```bash
cd desktop-shell/
source dev/workspace.env
bash dev/run-dev.sh
```

### Hot rebuild saat mengubah filemanager
```bash
bash dev/dev-rebuild.sh
```

## Mode Build CMake

| Preset | `SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE` | `SLM_USE_LOCAL_FILEMANAGER` | Sumber filemanager |
|--------|----|----|---|
| `dev-intree` | OFF | OFF | In-tree sources (default) |
| `dev-local-fm` | ON | ON | `../filemanager/build/dev` |
| `release-installed-fm` | ON | OFF | System installed package |
| `ci` | OFF | OFF | In-tree sources |

## QML Components

| Komponen | Fungsi |
|---|---|
| `FileManagerSidebarIntegration` | Sidebar places, mount events, trash count |
| `FileOperationsProgressOverlay` | Progress bar operasi copy/move/delete |
| `TrashBadgeController` | Badge jumlah item trash di dock |
| `FileManagerRecentFilesModel` | Model recent files untuk quick access |
| `FileManagerSearchBridge` | In-process search dengan debounce |
| `FileManagerOpenWithSheet` | Bottom sheet pilih aplikasi untuk open-with |

## DBus Services

| Service | Binary | Fungsi |
|---|---|---|
| `org.slm.Desktop.FileManager1` | `slm-filemanager` | Navigasi, open path, sidebar |
| `org.slm.Desktop.FileOperations` | `slm-fileopsd` | Copy, move, trash, empty-trash |

## Thumbnail di QML

```qml
Image {
    source: "image://thumbnail/256/" + filePath
    // Ukuran: 32, 64, 128, 256, 512
}
```

## Environment Variables Dev

Lihat `dev/workspace.env` untuk daftar lengkap. Variabel kunci:

```bash
SLM_FILEMANAGER_SOURCE_DIR   # path source checkout filemanager
SLM_FILEMANAGER_BUILD_DIR    # path build dir filemanager
SLM_FILEOPSD_BINARY          # override binary fileopsd
QT_LOGGING_RULES=slm.*=true  # aktifkan semua log
```

## Versioning Interface

- DBus interface menggunakan suffix nomor: `FileManager1`, `FileManager2`
- C++ library menggunakan SemVer via CMake `find_package(SlmFileManager 0.1.0 REQUIRED)`
- Breaking change → interface baru (side-by-side selama 1 release cycle)
- Lihat `scripts/dbus/` untuk XML introspection files

## Debugging

```bash
# Watch semua DBus traffic filemanager
dbus-monitor "destination='org.slm.Desktop.FileManager1'"

# Filter log per komponen
QT_LOGGING_RULES="slm.desktop.fmbridge=true;slm.fileops*=true" ./slm-desktop

# Trace request end-to-end
SLM_REQUEST_TRACE=1 QT_LOGGING_RULES="slm.*=true" ./slm-desktop
```
