# Archive Service Architecture & Roadmap (libarchive-first)

Status: implemented baseline + ongoing hardening.
Last updated: 2026-03-29

## Implementation snapshot (already done)

- `slm-archived` daemon exists (`src/daemon/archived/archived_main.cpp`).
- D-Bus service contract implemented:
  - `ListArchive`
  - `ExtractArchive`
  - `CompressPaths`
  - `TestArchive`
  - `CancelJob`
  - `GetJobStatus`
- Backend integrated with `libarchive` for list/extract/compress.
- Async job model implemented (`pending/running/completed/failed/cancelled`) with progress + cancel.
- FileManager integration already active:
  - double click default extract
  - `Open/Extract`, `Extract to...`, `Compress`
  - archive preview read-only (without full extraction)
  - batch/progress indicator wired for extract/compress jobs
- Security baseline implemented:
  - path traversal guard (`../`)
  - destination boundary checks
  - symlink/hardlink guarded extraction path
  - tests in `tests/archivebackend_test.cpp`

## Still in progress

- resource limit policy (entry count / expanded size / archive bomb heuristics)
- progress UX threshold policy (tiny archive -> toast-only path)
- telemetry and compatibility matrix by archive format

## 1. Prinsip desain

- Finder-like simplicity: arsip terasa seperti file biasa, bukan workflow teknis.
- Default action first: user cukup double click, sistem pilih aksi paling natural dan aman.
- UI thin layer: file manager tidak punya logika format arsip.
- Service-centric: semua operasi archive lewat `Archive Service`.
- Safe by default: proteksi eksploit arsip aktif tanpa membebani user.
- Progressive disclosure: fitur advanced ada, tapi tidak muncul di alur utama.

## 2. Arsitektur komponen

### 2.1 File Manager

Responsibilities:
- Deteksi item adalah arsip (via MIME + service capability check).
- Menampilkan action utama (`Open/Extract`, `Extract to...`, `Compress`).
- Menampilkan progress, notifikasi, dan pesan error user-friendly.

Non-responsibilities:
- Parse format archive.
- Implement extract/compress logic.
- Security policy detail archive.

### 2.2 Archive Service (`slm-archived`)

Role:
- Daemon user-session (non-root), API stabil via D-Bus.
- Menangani:
  - list/inspect/test archive
  - extract/compress
  - layout detection (single file/folder/multi root)
  - progress + cancellation
  - validation/security checks
  - error mapping (internal detail -> user message)

### 2.3 Backend `libarchive`

Primary backend:
- Read/list/test/extract untuk format umum: zip/tar/tar.gz/tgz/tar.xz dan format libarchive lain.
- Compress default `.zip`; tar-variants optional via explicit format parameter.

Known constraints:
- Beberapa format/encryption/feature khusus mungkin parsial.
- Untuk format unsupported: service mengembalikan `ERR_UNSUPPORTED_FORMAT` (bukan crash/silent).

### 2.4 Optional helpers

Policy:
- Default: tanpa helper eksternal agar stack sederhana dan konsisten.
- Optional fallback helper hanya jika ada kebutuhan format enterprise tertentu.
- Jika fallback dipakai: tetap lewat Archive Service API, tidak expose tool eksternal ke file manager.

## 3. Kontrak API service

Transport rekomendasi: D-Bus session service (`org.slm.Archive1`).

### 3.1 `ListArchive(path)`

Input:
- `path: string`

Output:
- `archiveMeta` (format, compressedSize, encryptedFlag, entryCountEstimate)
- `entries[]` (name, size, compressedSize, mtime, isDir, isSymlink)
- `layout` (`single_file|single_root_folder|multi_root`)

Errors:
- `ERR_NOT_FOUND`, `ERR_UNSUPPORTED_FORMAT`, `ERR_CORRUPT_ARCHIVE`, `ERR_PERMISSION`

Mode:
- Async disarankan untuk arsip besar; sinkron boleh untuk arsip kecil (dengan timeout kecil).

### 3.2 `ExtractArchive(path, destination, mode)`

Input:
- `path: string`
- `destination: string`
- `mode: string` (`extract_sibling_default`, `extract_to_destination`)
- `conflictPolicy: string` (`auto_rename` default, `replace`, `skip`)

Output:
- `jobId: string`
- `plannedOutputPath: string`

Errors:
- `ERR_NOT_FOUND`, `ERR_PERMISSION`, `ERR_DISK_FULL`, `ERR_UNSAFE_ARCHIVE`, `ERR_UNSUPPORTED_FORMAT`

Mode:
- Async wajib.

### 3.3 `CompressPaths(paths, destination, format)`

Input:
- `paths: string[]`
- `destination: string`
- `format: string` (`zip` default)

Output:
- `jobId: string`
- `outputPath: string`

Errors:
- `ERR_PERMISSION`, `ERR_DISK_FULL`, `ERR_INVALID_INPUT`

Mode:
- Async wajib.

### 3.4 `TestArchive(path)`

Input:
- `path: string`

Output:
- `ok: bool`
- `warnings[]`

Errors:
- Sama seperti `ListArchive`.

Mode:
- Async recommended.

### 3.5 `CancelJob(jobId)`

Input:
- `jobId: string`

Output:
- `cancelRequested: bool`

### 3.6 `GetJobStatus(jobId)`

Output:
- `state` (`pending|running|completed|failed|cancelled`)
- `progress` (`0..100`)
- `bytesProcessed`, `bytesTotalEstimate`
- `userMessage` (short)
- `internalErrorCode` + `traceId` (diagnostic)

### 3.7 Signals

- `JobUpdated(jobId, statusPayload)`
- `JobCompleted(jobId, resultPayload)`
- `JobFailed(jobId, errorPayload)`

## 4. Kebijakan keamanan default

- Path traversal: blok `../` dan canonicalize path sebelum write.
- Absolute path stripping: entry `/etc/...` diekstrak relatif ke destination (atau diblok jika mencurigakan).
- Symlink policy (default):
  - Preview/list: tampilkan sebagai metadata saja.
  - Extract: symlink hanya diizinkan jika target tetap di bawah destination.
- Hardlink policy: treat konservatif; blok jika mengarah keluar destination.
- Permission policy: jangan preserve owner/group original archive; gunakan user saat ini.
- Overwrite default: `auto_rename` (paling aman + minim dialog).
- Resource limits:
  - max entries
  - max expanded size
  - max ratio compressed->expanded (bomb heuristic)
  - per-job timeout + cancellation.
- Process security:
  - Service berjalan sebagai user biasa, bukan root.
  - Optional sandbox (bubblewrap/systemd sandbox profile) untuk extract worker.

## 5. Alur UX utama

### 5.1 Double click (keputusan utama)

Default final:
- **Auto extract ke sibling folder** dengan nama arsip.

Conflict handling:
- Default `auto_rename` (`Archive`, `Archive 2`, `Archive 3`, ...).
- Tidak tampil dialog kecuali kebijakan explicit `replace` dipilih user.

Post extract behavior:
- Jika layout `single_root_folder`: auto-open folder hasil (rasa Finder).
- Jika `single_file` atau `multi_root`: tetap di parent, seleksi folder hasil + toast sukses.

### 5.2 Preview

- File manager memanggil `ListArchive(path)` saat panel preview aktif.
- Preview read-only: list top entries + metadata singkat.
- Tanpa ekstraksi penuh.

### 5.3 Context menu minimal

Default visible:
- `Open / Extract`
- `Extract to...`
- `Compress`

Advanced submenu:
- `Extract with options...`
- `Test archive integrity`
- `Advanced format options`

## 6. Integrasi ke file manager

- Archive detection:
  - MIME type + extension fallback + `CanHandleFormat(path)` optional.
- Default action resolver:
  - on double click -> `ExtractArchive(..., mode=extract_sibling_default)`.
- Preview integration:
  - quick panel call `ListArchive` with debounce/cache.
- Drag-and-drop:
  - drag archive as file biasa.
  - drop “into archive” tidak default (advanced only) untuk menjaga sederhana.
- Progress UX:
  - operasi < 500ms: tidak tampil dialog, hanya toast.
  - > 500ms: non-modal progress panel.
  - > 5s: tampilkan cancel CTA jelas.

## 7. Roadmap implementasi bertahap

### Phase 1 - Service skeleton + read path

- `slm-archived` daemon + D-Bus contract dasar.
- Implement `ListArchive`, `TestArchive` via libarchive.
- Job model + signals.
- File manager: context menu minimal + preview panel read-only.

### Phase 2 - Extract default UX

- Implement `ExtractArchive` + security guards.
- Default double click behavior + auto-open heuristic.
- Conflict strategy `auto_rename`.
- Error mapping user-friendly.

### Phase 3 - Compress

- Implement `CompressPaths` default `.zip`.
- Naming rules untuk single folder vs multi selection.
- Progress/cancel UI integration.

### Phase 4 - Hardening

- Bomb/resource limits + timeout.
- Symlink/hardlink strict policy.
- Optional worker sandbox.
- Robust telemetry + regression test suite.

### Phase 5 - Optional advanced

- Advanced submenu options.
- Optional fallback helper strategy untuk format niche.
- Performance tuning untuk very large archives.

## 8. Risiko teknis dan mitigasi

- libarchive feature gap on niche formats:
  - Mitigasi: capability response + clear unsupported error.
- False positives bomb heuristic:
  - Mitigasi: conservative defaults + allow admin tuning.
- UI regressi karena async complexity:
  - Mitigasi: strict job-state contract + integration tests.
- Path/security bypass edge cases:
  - Mitigasi: centralized sanitizer + fuzz tests + canonical path checks.
- Long-running tasks block UX:
  - Mitigasi: non-modal progress + cancellation + worker isolation.

## 9. Keputusan final yang direkomendasikan

1. Default double click: **extract otomatis ke sibling folder**.
2. Conflict default: **auto_rename**, bukan prompt.
3. Preview: **read-only listing** via service, tanpa full extraction.
4. UI utama: hanya `Open/Extract`, `Extract to...`, `Compress`.
5. Advanced options: pindahkan ke submenu, bukan alur utama.
6. Service runtime: user-session daemon (non-root), libarchive-first.
7. Security defaults: traversal block, absolute strip/block, symlink/hardlink safe policy, resource limits.
8. Progress UX: toast untuk cepat, non-modal progress + cancel untuk job berat.
9. Unsupported format: tampilkan pesan sederhana + fallback path terkontrol, tetap lewat service.

Rekomendasi ini menjaga pengalaman tetap sederhana, konsisten, dan “magis” untuk mayoritas user, tanpa mengorbankan stabilitas desktop.
