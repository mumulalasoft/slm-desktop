# SLM Portal Notes

Dokumen ringkas untuk perilaku `org.slm.Desktop.Portal` saat ini.

## Impl Backend Surface

Selain API internal `org.slm.Desktop.Portal`, portald juga mengekspose bridge surface:

- service: `org.freedesktop.impl.portal.desktop.slm`
- path: `/org/freedesktop/portal/desktop`
- interface: `org.freedesktop.impl.portal.desktop.slm`

Method bridge awal yang tersedia:

- `FileChooser(handle, appId, parentWindow, options)`
- `OpenURI(handle, appId, parentWindow, uri, options)`
- `Screenshot(handle, appId, parentWindow, options)`
- `ScreenCast(handle, appId, parentWindow, options)`
- `GetScreencastState()`
- `CloseScreencastSession(sessionHandle)`
- `RevokeScreencastSession(sessionHandle, reason)`
- `CloseAllScreencastSessions()`

Context normalisasi call impl (lintas toolkit):
- `appId` dinormalisasi ke format `.desktop` (fallback ke caller identity bila kosong).
- `parentWindow` diparse ke:
  - `parentWindowType` (`wayland`, `x11`, `none`, `invalid`)
  - `parentWindowId`
  - `parentWindowValid` (bool)
- metadata caller ditambahkan:
  - `callerBusName`
  - `callerExecutablePath`
  - `desktopFileId`

Di path yang sama juga tersedia split interface adaptor:

- `org.freedesktop.impl.portal.FileChooser`
  - `OpenFile(handle, appId, parentWindow, options)`
  - `SaveFile(handle, appId, parentWindow, options)`
- `org.freedesktop.impl.portal.OpenURI`
  - `OpenURI(handle, appId, parentWindow, uri, options)`
- `org.freedesktop.impl.portal.Screenshot`
  - `Screenshot(handle, appId, parentWindow, options)`
- `org.freedesktop.impl.portal.ScreenCast`
  - `CreateSession(handle, appId, parentWindow, options)`
  - `SelectSources(handle, appId, parentWindow, sessionPath, options)`
  - `Start(handle, appId, parentWindow, sessionPath, options)`
  - `Stop(handle, appId, parentWindow, sessionPath, options)`
- `org.freedesktop.impl.portal.Settings`
  - `ReadAll(namespaces)` -> `a{sa{sv}}` (read-only)
  - `Read(namespace, key)` -> `v` (read-only)
- `org.freedesktop.impl.portal.Notification`
  - `AddNotification(appId, id, notification)` -> normalized map response
  - `RemoveNotification(appId, id)` -> normalized map response
- `org.freedesktop.impl.portal.Inhibit`
  - `Inhibit(handle, appId, window, flags, options)` -> normalized map response
- `org.freedesktop.impl.portal.OpenWith`
  - `QueryHandlers(handle, appId, parentWindow, path, options)`
  - `OpenFileWith(handle, appId, parentWindow, path, handlerId, options)`
  - `OpenUriWith(handle, appId, parentWindow, uri, handlerId, options)`
- `org.freedesktop.impl.portal.Documents`
  - `Add(handle, appId, parentWindow, uris, options)`
  - `Resolve(handle, appId, parentWindow, token, options)`
  - `List(handle, appId, parentWindow, options)`
  - `Remove(handle, appId, parentWindow, token, options)`
- `org.freedesktop.impl.portal.Trash`
  - `TrashFile(handle, appId, parentWindow, path, options)`
  - `EmptyTrash(handle, appId, parentWindow, options)`

Catatan:
- ini adalah layer split untuk migrasi ke backend `xdg-desktop-portal` style (`org.freedesktop.impl.portal.*`).
- backend capability/policy tetap source-of-truth yang sama (shared mediator).
- guard input impl (bridge layer):
  - `OpenURI`: reject `handle` kosong, `parentWindow` invalid, URI invalid.
  - `FileChooser`: reject `handle` kosong, `parentWindow` invalid, `mode` di luar `open/save`.
  - `Inhibit`: reject `handle` kosong, `parentWindow` invalid.
  - `Screenshot`: reject `handle` kosong, `parentWindow` invalid.
  - `ScreenCast` (`CreateSession/SelectSources/Start/Stop`): reject `handle` kosong, `parentWindow` invalid, `sessionPath` kosong (untuk operation call).
- anti-spam:
  - `Notification.AddNotification` dibatasi `20` request / `30` detik per `appId` (response error `rate-limited`).
- openwith notes:
  - `QueryHandlers` mengembalikan `handlers[]` + `default_handler` berbasis GIO app association.
  - `OpenFileWith`/`OpenUriWith` mendukung `options.dryRun=true` untuk kontrak/testing tanpa launch process.
- screencast notes:
  - `GetScreencastState` menyediakan snapshot runtime (`active`, `active_count`, `active_sessions`, `active_apps`) untuk sinkronisasi indikator privasi.
  - snapshot juga menyediakan `active_session_items[]` (`session`, `app_id`, `app_label`, `icon_name`) untuk UI action per-session.
  - tersedia kontrol close sesi melalui `CloseScreencastSession` dan `CloseAllScreencastSessions`.
  - `SelectSources` menormalkan `results.sources_selected` + `results.selected_sources` (snapshot pilihan source untuk session).
  - `Start` menormalkan `results.streams`:
    - jika backend mengirim descriptor stream (`streams[]` atau `node_id/stream_id`), nilai itu dipakai langsung,
    - jika belum ada backend stream real, bridge menyediakan descriptor stream sintetis stabil (`stream_id`, `node_id`, `source_type`, `cursor_mode`) agar kontrak klien tetap deterministic.
  - impl bridge juga mencoba mengambil stream descriptor dari provider capture opsional:
    - service: `org.slm.Desktop.Capture`
    - path: `/org/slm/Desktop/Capture`
    - iface: `org.slm.Desktop.Capture`
    - method: `GetScreencastStreams(sessionPath, appId, options)` (fallback kompatibel: `GetScreencastStreams(sessionPath, options)`).
  - `desktopd` sekarang menyediakan provider capture default (`CaptureService`) yang:
    - menormalkan descriptor stream dari `options` (`streams[]`, `node_id`, `stream_id`, `pipewire_node_id`),
    - menerima format tuple style PipeWire/portal (`[node_id, a{sv}]`) selain map style,
    - menjaga cache stream per-session agar payload `Start` stabil antar request pada sesi yang sama,
    - tidak lagi menjalankan fallback `pw-dump`; sumber stream bersifat deterministik
      (`options` -> cache session -> screencast-session -> metadata session portal).
  - `Stop` mendukung `options.closeSession` (default `true`) untuk otomatis menutup sesi backend.
  - `RevokeScreencastSession` tersedia untuk policy/system revoke:
    - jalur utama memanggil backend `revokeSession(sessionHandle, reason)`,
    - fallback kompatibilitas ke `closeSession` bila backend revoke belum ada,
    - emisi signal `ScreencastSessionRevoked(sessionHandle, appId, reason, activeCount)`.
  - close/revoke path juga menyinkronkan lifecycle sesi ke `org.slm.Desktop.Capture`:
    - close -> `ClearScreencastSession(sessionHandle)`
    - revoke -> `RevokeScreencastSession(sessionHandle, reason)`
    agar cache stream provider lintas-service tidak stale.
  - saat `ScreenCast.Start` menghasilkan descriptor stream final, bridge melakukan upsert ke capture provider:
    - `SetScreencastSessionStreams(sessionHandle, streams, {})`
    supaya lookup stream berikutnya konsisten pada level service capture.
  - jika payload `Start` masih synthetic-only, bridge mencoba refresh dari capture provider untuk mengganti dengan stream real bila tersedia.
    - prioritas refresh: `org.slm.Desktop.Screencast:GetSessionStreams` -> fallback `org.slm.Desktop.Capture:GetScreencastStreams`.
  - `org.slm.Desktop.Portal` sekarang mengekspose helper internal:
    - `GetScreencastSessionStreams(sessionPath)` -> stream descriptor dari metadata session backend,
    - dipakai `CaptureService` sebagai sumber stream internal saat opsi/cached stream tidak tersedia.
  - `CaptureService` berlangganan sinyal impl portal:
    - `ScreencastSessionStateChanged` dan `ScreencastSessionRevoked`,
    - cache stream per-session otomatis dibersihkan saat sesi tidak aktif/revoked.
  - `desktopd` juga menjalankan `CaptureStreamIngestor`:
    - subscribe sinyal `org.slm.Desktop.Screencast` (`SessionStreamsChanged/SessionEnded/SessionRevoked`),
    - meng-upsert stream ke `CaptureService` via `SetScreencastSessionStreams` secara event-driven.
  - `desktopd` kini mengekspor service `org.slm.Desktop.Screencast` aktif:
    - method `UpdateSessionStreams`, `EndSession`, `RevokeSession` sebagai jalur ingest utama.
    - service menggunakan adapter backend `ScreencastStreamBackend`:
      - saat ini mode backend = portal-metadata mirror (subscribe signal impl-portal + fetch stream metadata),
      - siap diganti backend PipeWire native tanpa mengubah kontrak DBus service.
      - selector backend via env:
        - `SLM_SCREENCAST_STREAM_BACKEND=portal-mirror` (default),
        - `SLM_SCREENCAST_STREAM_BACKEND=pipewire` (butuh build dengan `libpipewire-0.3` + `libspa-0.2`; jika tidak tersedia auto fallback ke `portal-mirror`).
      - status backend diekspos di `Ping/GetState`:
        - `stream_backend_requested`, `stream_backend`, `stream_backend_fallback_reason`, `pipewire_build_available`.
      - mode `pipewire` saat ini:
        - inisialisasi native PipeWire (`pw_init`, thread-loop/context/core/registry),
        - membaca candidate video stream node dari registry PipeWire,
        - fallback ke metadata portal bila candidate stream belum tersedia.
  - `implportalservice` mem-publish lifecycle/stream ke `org.slm.Desktop.Screencast` lebih dulu,
    dengan fallback kompatibilitas ke `org.slm.Desktop.Capture` jika service belum tersedia.
  - `results.session_closed` dikembalikan untuk menandai apakah close-session sukses.
  - impl service mengekspose sinyal DBus `ScreencastSessionStateChanged(sessionHandle, appId, active, activeCount)` untuk integrasi indikator privasi shell.
- documents/trash notes:
  - `Documents` saat ini memakai token store in-memory per-app (scope sesi daemon), bukan persistent document store.
  - `Trash` dimediasi ke `org.slm.Desktop.FileOperations` (`Trash`, `EmptyTrash`) dengan guard input impl.

## xdg-desktop-portal Discovery Artifacts

Installer `scripts/install-portal-user-service.sh` sekarang juga memasang artefak discovery user-level:

- `~/.local/share/xdg-desktop-portal/portals/slm.portal`
- `~/.config/xdg-desktop-portal/portals.conf`
- `~/.config/xdg-desktop-portal/slm-portals.conf`
- `~/.local/share/dbus-1/services/org.freedesktop.impl.portal.desktop.slm.service`

Tujuan:
- `xdg-desktop-portal` bisa me-resolve backend `slm` lewat `portals.conf`.
- DBus activation untuk `org.freedesktop.impl.portal.desktop.slm` tersedia walau service belum di-start.

### Profile-aware `portals.conf`

Installer sekarang mendukung profile rollout agar coexistence dengan backend lain tetap terkontrol:

- `mvp` (default): `default=gtk`, override hanya portal inti SLM.
- `beta`: tambah route `ScreenCast/Documents/Trash/OpenWith/GlobalShortcuts` ke SLM.
- `production`: `default=slm` dengan fallback targeted untuk `RemoteDesktop/Camera/Location/InputCapture`.
- `legacy`: pakai template lama `scripts/xdg-desktop-portal/portals.conf`.

Pemakaian:

```bash
scripts/install-portal-user-service.sh /path/to/slm-portald beta
```

Variabel env:

- `SLM_PORTAL_PROFILE={mvp|beta|production|legacy}`
- `SLM_PORTAL_SET_DEFAULT=1` untuk memaksa overwrite `~/.config/xdg-desktop-portal/portals.conf`.
  Tanpa flag ini, installer hanya mengisi `portals.conf` bila file belum ada dan selalu mengupdate
  `slm-portals.conf`.
- `SLM_PORTAL_SKIP_SYSTEMCTL=1` untuk mode CI/test (skip `systemctl --user ...`).

## OpenURI

Method: `OpenURI(uri, options)`

### Validasi URI

- URI kosong/whitespace ditolak (`error=invalid-argument`)
- URI harus valid dan punya scheme
- Scheme harus ada di whitelist

### Whitelist Scheme Priority

Urutan prioritas sumber whitelist (paling tinggi ke rendah):

1. `options.allowedSchemes`
2. env `SLM_PORTAL_OPENURI_ALLOWED_SCHEMES`
3. config file `~/.config/slm/portald.conf` (atau `$XDG_CONFIG_HOME/slm/portald.conf`)
4. default built-in

Default built-in:

- `file`
- `http`
- `https`
- `mailto`
- `ftp`
- `smb`
- `sftp`

### Eksekusi

- Jalur normal: `gio open <uri>`
- Jika gagal start process: `error=launch-failed`
- Untuk testing/non-side-effect, set `options.dryRun=true`
  - return `ok=true`
  - `launched=false`
  - tanpa menjalankan proses eksternal

## Config File Example

Path:

- `~/.config/slm/portald.conf`

Contoh:

```ini
openuri_allowed_schemes=file,https,mailto,smb
```

Key alternatif yang juga didukung:

- `Portal/openuri_allowed_schemes`
- `Portal/OpenUriAllowedSchemes`
