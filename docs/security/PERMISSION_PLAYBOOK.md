# SLM Permission Playbook

Playbook ini adalah panduan operasional saat menambah capability, memasang guard di service DBus, dan memastikan audit/deny-path tetap konsisten.

## 1. Tujuan

- Menjaga **default-deny** untuk caller third-party.
- Menjaga mapping method -> capability tetap eksplisit.
- Menjaga semua keputusan izin tercatat di audit log (tanpa data sensitif).
- Menjaga test security mudah dijalankan di lokal, CI, dan headless.

Referensi ringkas policy default per capability:
- `docs/security/CAPABILITY_MATRIX.md`

## 2. Checklist Saat Menambah Capability Baru

1. Tambahkan enum di `src/core/permissions/Capability.h/.cpp`:
   - enum value,
   - `capabilityToString`,
   - `capabilityFromString`,
   - `allCapabilities()`.
2. Tetapkan sensitivity + gesture default di `PolicyEngine`:
   - `m_sensitivity[Capability::...] = ...`
   - `m_requiresGesture[Capability::...] = true/false`
3. Tambahkan policy rule default:
   - `evaluateCoreComponent`,
   - `evaluatePrivilegedService`,
   - `evaluateThirdParty`.
4. Jika capability dipakai DBus method:
   - map method ke capability lewat `DBusSecurityGuard::registerMethodCapability(...)`
     atau helper service seperti `permissionCapabilityForList/Invoke`.
5. Tambahkan test:
   - unit policy matrix (`policyengine_test`),
   - deny-path atau allow-path DBus integration sesuai flow.
6. Perbarui dokumen:
   - `docs/architecture/PERMISSION_FOUNDATION.md`
   - changelog API jika kontrak method berubah.

## 3. Mapping Method -> Capability (Current Critical Paths)

### `org.freedesktop.SLMCapabilities`

- `ListActionsWithContext(capability, context)`:
  - `Share` -> `Share.Invoke`
  - `QuickAction` -> `QuickAction.Invoke`
  - `SearchAction` -> `Search.QueryFiles`
  - fallback (`ContextMenu`, `DragDrop`) -> `FileAction.Invoke`
- `InvokeActionWithContext(actionId, context)`:
  - context `capability=share` -> `Share.Invoke`
  - context `capability=quickaction` -> `QuickAction.Invoke`
  - context `capability=openwith` -> `OpenWith.Invoke`
  - fallback -> `FileAction.Invoke`
- `SearchActions(query, context)` -> `Search.QueryFiles`

### `org.slm.Desktop.Search.v1`

- `Query(text, options)` -> `Search.QueryFiles`
- `PreviewResult(id)` -> `Search.QueryFiles`
- `ActivateResult(id, activate_data)`:
  - provider `clipboard` -> `Search.ResolveClipboardResult`
  - result type `app` -> `Search.QueryApps`
  - selain itu -> `Search.QueryFiles`

## 4. AccessContext Minimal yang Wajib Dikirim

Saat service memanggil guard, context minimal:

- `capability` (diisi backend, bukan dari UI/caller),
- `resourceType` (contoh: `search-result`, `action`),
- `resourceId` (id item/action),
- `initiatedByUserGesture` (bool),
- `initiatedFromOfficialUI` (bool),
- `sensitivityLevel` (`Low|Medium|High|Critical`).

Catatan: `DBusSecurityGuard` akan menormalkan marker gesture menjadi `false` untuk caller third-party non-official.

## 5. Audit Logging Contract

Audit disimpan di `PermissionStore::appendAudit(...)`.

Field utama:

- `appId`
- `capability`
- `decision`
- `resourceType`
- `timestamp`
- `reason`

Aturan:

- Jangan simpan konten sensitif (clipboard body, token, mail body).
- `reason` harus ringkas dan actionable (contoh: `user-gesture-required`, `default-deny`).

## 6. Deny/Allow Error Contract di Service

Untuk DBus method yang mengembalikan map:

- deny:
  - `ok=false`
  - `error=permission-denied`
  - `reason=<policy reason/label>`

Untuk method list:

- deny return list kosong (plus audit/log deny).

## 7. Testing Matrix yang Wajib Dijaga

## 7.1 Policy unit tests

- `tests/policyengine_test.cpp`:
  - trust level x capability x gesture.

## 7.2 Permission payload contract

- `tests/permissionstore_payload_contract_test.cpp`:
  - shape field output `listPermissions()`,
  - shape field output `listAuditLogs()`.

## 7.3 Guard tests

- `tests/dbussecurityguard_test.cpp`:
  - third-party deny pada capability gesture-gated,
  - allow pada low-risk capability.

## 7.4 Service integration tests

- `tests/slmcapabilityservice_dbus_test.cpp`:
  - deny file action tanpa gesture,
  - deny endpoint admin-only untuk caller non-admin.
- `tests/globalsearchservice_dbus_test.cpp`:
  - split capability pada `ActivateResult` (apps/files/clipboard).

## 8. Cara Menjalankan Test Security di Headless/CI

Gunakan CTest regex:

```bash
ctest --output-on-failure -R "policyengine_test|permissionstore_payload_contract_test|dbussecurityguard_test|slmcapabilityservice_dbus_test|globalsearchservice_dbus_test"
```

Untuk test DBus, project menggunakan wrapper:

- `scripts/run-dbus-test.sh`

Perilaku wrapper:

- pakai `dbus-run-session` jika tersedia,
- fallback direct run jika sandbox melarang bind socket DBus.

## 9. Troubleshooting Cepat

### Gejala: test DBus gagal `Failed to bind socket ... Operation not permitted`

- Penyebab: sandbox/headless env melarang socket DBus.
- Aksi:
  - pastikan test dijalankan via `scripts/run-dbus-test.sh`,
  - atau jalankan lokal non-sandbox dengan session bus aktif.

### Gejala: test skip `session bus is not available`

- Jika di CI/headless, valid bila memang tidak ada session bus.
- Untuk verifikasi penuh integration flow, jalankan di session yang punya DBus.

### Gejala: endpoint tiba-tiba allow untuk third-party

- Cek:
  - mapping method->capability di service,
  - rule `PolicyEngine::evaluateThirdParty`,
  - marker gesture dari context.

## 10. Definition of Done (Permission Change)

Perubahan permission/capability dianggap selesai jika:

1. capability + string mapping selesai,
2. policy default selesai,
3. service mapping selesai,
4. audit path ter-cover,
5. test unit + integration yang relevan hijau,
6. dokumen `PERMISSION_FOUNDATION` + playbook diperbarui.
