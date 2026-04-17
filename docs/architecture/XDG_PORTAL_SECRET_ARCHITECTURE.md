# XDG Portal Secret Architecture (SLM Desktop)

Status: draft design ready for implementation planning  
Last updated: 2026-03-29

## 1. Prinsip Desain

1. Secure by default  
Semua secret disimpan terenkripsi, akses default deny kecuali policy mengizinkan.
2. Least privilege  
Aplikasi hanya dapat akses secret miliknya; lintas-aplikasi butuh consent eksplisit.
3. Consent jelas dan minim friksi  
Popup hanya muncul saat benar-benar perlu, dengan opsi one-time vs remember.
4. Unified API untuk sandbox dan non-sandbox  
`org.freedesktop.portal.Secret` menjadi pintu masuk standar.
5. UX non-teknis  
Tidak menampilkan istilah keyring/DBus/backend ke user umum.
6. Recovery-first  
Kegagalan subsistem secret tidak boleh memblokir login sesi grafis.
7. Auditable without leaking  
Audit menyimpan metadata akses, tidak pernah menyimpan nilai secret.
8. Progressive hardening  
MVP cepat (reuse ecosystem), jangka panjang dapat migrasi ke backend internal tanpa memecah kontrak portal.

## 2. Posisi Dalam Arsitektur Desktop

Peran komponen:
- Aplikasi biasa: akses secret via portal client library atau DBus.
- Aplikasi sandbox: wajib lewat `xdg-desktop-portal`.
- `xdg-desktop-portal`: front door standar portal.
- `xdg-desktop-portal-slm`: backend policy + UI request + bridge ke secret daemon.
- PermissionStore: menyimpan grant per app/secret action.
- Session bus: jalur utama request portal.
- Secret daemon (`slm-secretd`): otoritas operasi secret (store/get/delete/metadata).
- Backend storage terenkripsi: penyimpanan actual secret.
- Settings app: manajemen grant + metadata secret.
- Shell/dialog: consent UI, trust indicator, re-auth prompt.
- Lock screen/login session: lock/unlock state untuk gating akses.
- Recovery mode: mode degrade untuk perbaikan metadata/daemon/storage.

Diagram teks:

```text
App (sandbox/non-sandbox)
    |
    v
org.freedesktop.portal.Desktop (session bus)
    |
    v
xdg-desktop-portal-slm (Secret backend + Policy adapter)
    |                   \
    |                    -> PermissionStore (grant/read/write/delete)
    |
    v
slm-secretd (session service, policy-enforced data plane)
    |
    v
Encrypted Secret Backend (MVP: Secret Service/libsecret provider)
```

## 3. Spesifikasi Interface Portal

### 3.1 Identitas dan Binding App

- Sandbox app: gunakan `app_id` dari portal metadata sandbox.
- Non-sandbox app: derive identity dari desktop file/app-id terverifikasi, fallback ke DBus unique-name + executable path fingerprint.
- Binding secret key scope: `(owner_app_id, secret_namespace, secret_key)`.

### 3.2 Interface Proposal

- Object path: `/org/freedesktop/portal/desktop`
- Interface: `org.freedesktop.portal.Secret`
- Request object path (async): `/org/freedesktop/portal/desktop/request/<sender>/<token>`

Methods:

1. `StoreSecret(in s parent_window, in a{sv} options, in ay secret, out o handle)`  
Async (Request object).  
`options`: `key`, `namespace`, `label`, `sensitivity`, `expires_at`, `allow_cross_app`.  
Result: success + metadata id.  
Error: `org.freedesktop.portal.Error.InvalidArgument`, `Denied`, `Cancelled`, `Failed`.

2. `GetSecret(in s parent_window, in a{sv} query, out o handle)`  
Async.  
`query`: `key`, `namespace`, optional `owner_app_id` (cross-app).  
Result: `secret` (bytes), metadata ringkas.  
Error: `NotFound`, `Denied`, `Locked`, `Cancelled`.

3. `DeleteSecret(in s parent_window, in a{sv} query, out o handle)`  
Async bila butuh consent/re-auth, sync-fast path bila already granted.

4. `DescribeSecret(in a{sv} query, out a{sv} metadata)`  
Sync allowed. Tidak pernah mengembalikan nilai secret.

5. `ListOwnSecretMetadata(in a{sv} options, out aa{sv} items)`  
Sync/paginated. Hanya metadata milik app caller.  
Catatan: menggantikan `ListSecrets` global demi keamanan.

6. `ClearAppSecrets(in s parent_window, in a{sv} options, out o handle)`  
Async, untuk hapus semua secret milik app caller (atau app tertentu jika privileged service).

7. `GrantAccess(in s parent_window, in a{sv} rule, out o handle)`  
Async, umumnya hanya untuk trusted desktop services/admin UX.

8. `RevokeAccess(in a{sv} rule, out b ok)`  
Sync untuk Settings/system components.

9. `LockSecrets(in a{sv} options, out b ok)`  
Sync.

10. `UnlockSecrets(in s parent_window, in a{sv} options, out o handle)`  
Async, bisa memicu re-auth user.

Signal:
- `SecretAccessed(s app_id, s action, s namespace, s key, t ts)` metadata-only.
- `SecretsLockStateChanged(b locked)`.

Method yang **tidak direkomendasikan**:
- `ListSecrets` global lintas-app: terlalu besar risiko enumeration.
Alternatif:
- `ListOwnSecretMetadata` + privileged audit API internal (bukan public portal).

## 4. Model Permission dan Consent

Integrasi dengan `org.freedesktop.portal.PermissionStore`:
- Scope key: `portal.secret.<owner_app_id>.<namespace>.<key>`.
- Action granularity: `read`, `write`, `delete`, `cross_read`, `cross_write`, `clear_all`.
- Decision: `allow-once`, `allow-always`, `deny`, `ask`.

Policy table:

| Skenario | Default | Popup | Persist |
|---|---|---|---|
| App store secret milik sendiri | Allow | Tidak (kecuali high sensitivity) | Auto grant write-own |
| App read secret milik sendiri | Ask first | Ya | Bisa allow-always |
| App delete secret milik sendiri | Ask first | Ya | Bisa allow-always |
| App read secret app lain | Deny | Ya (strict warning) | Default one-time only |
| Trusted system service akses lintas app | Policy allowlist | Opsional | Managed policy |
| Background helper tanpa UI token | Deny | Tidak | Harus delegated token |

Aturan sensitivitas:
- `normal`: consent awal cukup.
- `high`: re-auth saat read pertama per unlock cycle.
- `critical`: selalu re-auth atau one-time consent.

## 5. Desain UX

Prinsip:
- Bahasa sederhana.
- Fokus ke aksi user, bukan detail backend.
- Tombol utama jelas.

Dialog utama:
- Save: “Allow `<App>` to securely save sign-in data?”
- Read: “Allow `<App>` to access saved sign-in data?”
- Cross-app: “`<App>` wants data created by `<Other App>`.”

Microcopy contoh (EN):
- Title: `Allow Access to Secure Data`
- Body (own): `AppName wants to access data it saved earlier.`
- Body (cross): `AppName wants to access secure data from OtherApp.`
- Primary: `Allow`
- Secondary: `Not Now`
- Tertiary (optional): `Always Allow for AppName`

Notifikasi:
- Success: `Secure data saved.`
- Read blocked: `Access was denied.`
- Backend unavailable: `Secure storage is temporarily unavailable.`

Re-auth:
- Trigger saat `high/critical`, cross-app, atau session baru setelah lock.

## 6. Integrasi ke Fitur Desktop

### a) Settings

Halaman: `Settings > Privacy & Security > Secrets`
- daftar aplikasi yang punya grant/secret metadata
- revoke grant per action
- hapus secret per app/namespace/key
- audit ringan (last access, action, outcome)
- tidak menampilkan raw secret value.

### b) Login & Unlock

- Default: unlock secret store saat login user (same credential path).
- Saat screen lock: read secret sensitif diblok sampai unlock.
- Autologin: store tetap lock-mode terbatas sampai user unlock first time.
- Separate secret password: optional advanced mode (bukan default MVP).

### c) Network & Wi-Fi

- Password Wi-Fi tetap authoritative di NetworkManager.
- Secret portal dapat menyimpan token tambahan untuk UI/cloud/network helpers.
- Jika butuh baca kredensial jaringan: lewat adaptor terkontrol, bukan expose raw NM secrets.

### d) Browser / Web apps / Cloud accounts

- Online Accounts menyimpan refresh token di secret portal.
- Token sync/file provider disimpan per-account namespace.
- Third-party app dapat akses terbatas via explicit grant.

### e) File Manager

- SMB/SFTP/WebDAV creds via secret portal dengan opsi `remember this password`.
- Support secret `session-only` vs `persistent`.
- Reconnect flow membaca secret tanpa mengganggu user jika grant valid.

### f) Developer Settings

- Mode debug: tampilkan decision trace, grant table, dan reset per-app grant.
- Tetap tidak menampilkan nilai secret.

### g) Global Search

- Dilarang indexing raw secret.
- Hanya metadata aman (misal “Cloud account connected”) tanpa key/value.

### h) Clipboard / Share / Automation

- Clipboard manager tidak boleh membaca raw secret dari portal.
- Automation API perlu capability khusus + explicit user consent.

## 7. Backend Storage

Opsi:

1. libsecret/Secret Service  
Pro: cepat, mature, interoperable.  
Kontra: tergantung implementasi daemon eksternal.

2. GNOME Keyring  
Pro: stabil di ekosistem GNOME.  
Kontra: coupling desktop-specific.

3. KWallet  
Pro: matang di KDE stack.  
Kontra: coupling desktop-specific.

4. Backend internal SLM terenkripsi  
Pro: kontrol penuh policy/recovery.  
Kontra: biaya implementasi dan audit tinggi.

5. File-based encrypted store  
Pro: sederhana.  
Kontra: risk tinggi jika implementasi crypto salah.

6. systemd credentials (ephemeral)  
Pro: bagus untuk secret sementara process/service.  
Kontra: bukan pengganti full secret vault.

7. TPM/hardware-backed  
Pro: hardening kuat.  
Kontra: portabilitas perangkat.

Rekomendasi:
- MVP: Secret Service via libsecret adapter di `slm-secretd`.
- v1 stabil: tetap adapter-based + fallback provider matrix (secretservice primary).
- Jangka panjang: backend internal SLM (with migration bridge), TPM enhancement optional.

## 8. Reliability dan Recovery

Aturan utama:
- Secret subsystem bukan hard dependency untuk greeter/session start.
- Failure harus degrade gracefully.

Failure scenarios and recovery paths:

1. Daemon secret gagal start  
- Portal return `temporarily unavailable`; app dapat retry/backoff.  
- Desktop tetap login normal.  
- Watchdog restart service.

2. Storage korup  
- Read-only quarantine mode.  
- Offer repair/backup-restore di recovery app.

3. Permission DB rusak  
- Rebuild permission index dari baseline deny + metadata scan.  
- Secret data tidak langsung dihapus.

4. Backend provider hilang  
- Missing component warning + guided install via existing missing-component flow.

5. Schema migration gagal  
- Rollback ke snapshot metadata sebelumnya.

Health checks:
- daemon liveness
- backend integrity probe
- permission store consistency check.

## 9. Security Model

Ancaman dan mitigasi:
- App baca secret app lain: namespace owner binding + strict consent.
- Identity spoofing: resolve app identity dari portal sandbox metadata / verified desktop entry.
- Helper abuse: delegated token dengan TTL, scope ketat.
- Memory dump: minimize in-memory lifetime, zero buffers, no raw secret in logs.
- Logs/crash reports: redact otomatis.
- Clipboard/search leakage: explicit deny policy.
- Backup leakage: encrypted backups + key separation.
- Brute force store: KDF + rate limit unlock.
- Session unlocked risk: sensitivity policy + optional re-auth.
- Root/admin: tidak sepenuhnya bisa dicegah; target mitigasi accidental/low-privilege abuse.

Boundary:
- Strong vs app biasa/sandbox: yes.
- Strong vs user lokal lain: yes (per-user isolation).
- Against root: limited by Linux trust model.

## 10. Integrasi Dengan Portal Lain

- FileChooser/Documents: secret hanya untuk credential path, bukan file payload.
- Background portal: background access harus grant khusus.
- Notification portal: notifikasi hasil akses tanpa detail secret.
- GlobalShortcuts: tidak boleh jadi jalan bypass consent.
- Screenshot/ScreenCast: tak ada akses secret.
- PermissionStore: source of truth grants.
- OpenURI/Accounts portal: token fetch tetap lewat Secret portal untuk konsistensi trust model.

## 11. Strategi Implementasi Bertahap

Phase 0: Research & foundation decisions  
- Output: backend decision, threat model baseline, scope final.

Phase 1: DBus spec draft  
- Output: XML/interface doc + error contract.

Phase 2: `slm-secretd` MVP  
- Output: store/get/delete own-secret + encrypted backend adapter.

Phase 3: Portal backend integration  
- Output: `xdg-desktop-portal-slm` Secret bridge + identity resolver.

Phase 4: Consent UI + Settings UI  
- Output: dialog flows + settings revoke/manage.

Phase 5: Login/session lock integration  
- Output: lock/unlock gating + re-auth policy.

Phase 6: FileManager + Online Accounts integration  
- Output: SMB/cloud token flows.

Phase 7: Hardening + migration + recovery  
- Output: watchdog, health checks, backup/restore, migration tests.

Per fase wajib:
- dependency map
- risk register
- definition of done
- unit + integration + recovery tests.

## 12. Checklist Implementasi

DBus/API:
- [ ] finalize interface XML
- [ ] define error code taxonomy
- [ ] add compatibility contract tests

Backend storage:
- [ ] implement provider abstraction
- [ ] secure encryption config
- [ ] migration hooks

Secret daemon:
- [ ] session service unit
- [ ] liveness + health endpoints
- [ ] audit metadata pipeline

Portal backend:
- [ ] permission gate bridge
- [ ] request object flow
- [ ] app identity resolver

Permission system:
- [ ] action-level grants
- [ ] policy table mapping
- [ ] revoke/reset semantics

Shell/dialog UI:
- [ ] consent dialog variants
- [ ] re-auth prompt
- [ ] trust indicator microcopy

Settings app:
- [ ] secrets page
- [ ] per-app grant view
- [ ] clear/revoke actions

Login/session:
- [ ] unlock policy hooks
- [ ] lock-state propagation
- [ ] autologin degraded behavior

File manager/network/accounts:
- [ ] SMB remember-password integration
- [ ] cloud token integration
- [ ] network boundary policy

QA/security:
- [ ] cross-app access denial tests
- [ ] spoofing tests
- [ ] redaction/log leak tests
- [ ] fuzzing input payload

Recovery:
- [ ] daemon fail-start scenario test
- [ ] storage corruption test
- [ ] permission-db rebuild test

Packaging/deployment:
- [ ] systemd unit/package dependency
- [ ] post-install health check
- [ ] docs + admin playbook.

## 13. Contoh Skenario End-to-End

1. Sandbox app store token pertama kali  
- Portal request -> consent -> store -> grant write/read-own -> success toast.

2. Non-sandbox app read token sendiri  
- Check grant -> read -> return secret -> no popup jika already allowed.

3. FileManager simpan SMB password (`remember`)  
- Mount dialog -> `StoreSecret(session|persistent)` -> reconnect memakai `GetSecret`.

4. Online Accounts refresh token  
- Service update token via `StoreSecret` namespace account -> audit metadata update.

5. User revoke akses di Settings  
- Revoke read/write grants -> app berikutnya akan diprompt ulang.

6. Daemon gagal start saat login  
- Portal return unavailable -> desktop tetap masuk sesi -> recovery hint di Settings.

7. Storage korup  
- `slm-secretd` masuk quarantine read-only -> recovery app tawarkan repair/restore.

8. App baca secret app lain  
- Default deny -> consent strict dialog -> jika reject, log denied + user-friendly error.

## 14. Rekomendasi Final

Keputusan tegas:
- Bangun `slm-secretd` + `xdg-desktop-portal-slm` Secret backend sendiri (control plane).
- Reuse Secret Service/libsecret untuk data-plane backend pada MVP/v1.
- Jangan expose `ListSecrets` global; pakai `ListOwnSecretMetadata`.
- Jadikan consent + PermissionStore sebagai source of truth lintas portal.
- Secret subsystem harus optional-degraded, bukan blocker login sesi.

Urutan paling realistis:
1. Phase 0-1 (spec + threat model)
2. Phase 2-3 (daemon + portal bridge)
3. Phase 4 (consent/settings)
4. Phase 5-6 (session + integrations)
5. Phase 7 (hardening/recovery).

Fitur yang ditunda:
- UI viewer raw secret
- full custom crypto backend di MVP
- broad cross-app sharing UX untuk non-trusted apps.

Tradeoff terbesar:
- Reuse backend existing mempercepat delivery tapi mengurangi kontrol penuh awal.
- Backend internal penuh memberi kontrol maksimal, tetapi harus ditunda sampai fondasi policy, testing, dan recovery matang.

