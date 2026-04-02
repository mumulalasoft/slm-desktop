# Portal Adapter Layer (SLM)

Dokumen ini mendefinisikan pemisahan jalur **portal-facing** vs **internal trusted API** untuk desktop SLM.

## Scope

- Menjembatani app pihak ketiga/sandbox ke permission foundation internal.
- Menjaga `PolicyEngine` sebagai source of truth.
- Menyediakan model async request/session ala portal.

## Component Graph

```text
ThirdPartyApp
  -> PortalService (DBus entry)
  -> PortalBackendService
      -> PortalAccessMediator
          -> TrustResolver + CallerIdentity
          -> PortalCapabilityMapper
          -> PermissionBroker -> PolicyEngine
          -> PortalPermissionStoreAdapter (persist decision)
          -> PortalDialogBridge (AskUser flow)
          -> PortalRequestManager / PortalSessionManager
  -> Internal subsystem service (clipboard/search/share/screencast)
```

## Module Responsibilities

- `PortalRequestObject/Manager`
  - object path: `/org/desktop/portal/request/<app>/<id>`
  - state: `Pending/AwaitingUser/Allowed/Denied/Cancelled/Failed`
  - signal final `Response(...)` sekali.
- `PortalSessionObject/Manager`
  - object path: `/org/desktop/portal/session/<app>/<id>`
  - state: `Created/Active/Suspended/Revoked/Closed/Expired`
  - lifecycle revocable untuk capability long-lived.
- `PortalPermissionStoreAdapter`
  - load/save grant decision berbasis app/capability/resource.
  - persistence input ke policy, bukan bypass policy.
- `PortalCapabilityMapper`
  - map portal method -> capability internal + request kind + sensitivity.
- `PortalAccessMediator`
  - orkestrasi caller resolve -> context -> policy -> request/session + consent.
- `PortalDialogBridge`
  - jembatan trusted UI untuk `AskUser`.
- `PortalResponseSerializer`
  - format error/success map konsisten.

## Request Flow (One-shot)

1. App call portal method.
2. Resolver ambil `CallerIdentity` dari DBus message.
3. Mapper tentukan capability internal.
4. `AccessContext` dibangun (resource, gesture, sensitivity).
5. `PermissionBroker` evaluasi lewat `PolicyEngine`.
6. Hasil:
   - `Allow`: request selesai sukses.
   - `Deny`: request selesai denied.
   - `AskUser`: request jadi `AwaitingUser`, tampil consent UI, lalu final response.

## Session Flow

1. App request capability session-based.
2. Policy evaluate seperti flow one-shot.
3. Jika allow (langsung/hasil consent), buat `PortalSessionObject`, `activate()`.
4. Session bisa `Close` oleh app atau `revoke()` oleh sistem.

## Security Boundary

- Third-party app **tidak** boleh akses internal service sensitif langsung.
- Official desktop component tetap bisa pakai jalur internal trusted DBus API.
- Portal layer hanya mediator/compatibility, bukan policy authority.
- UI dialog tidak menentukan policy sendiri; hasil tetap diproses backend.

## Current mapped methods (initial)

- `RequestClipboardPreview` -> `Search.QueryClipboardPreview` (one-shot)
- `RequestClipboardContent` -> `Clipboard.ReadHistoryContent` (one-shot, gesture)
- `RequestShare` -> `Share.Invoke` (one-shot, gesture)
- `StartScreencastSession` -> `Screencast.Start` (session, revocable)

## Next hardening

- Integrasi DBus request/session interface lebih dekat ke `org.freedesktop.portal.*`.
- Hook consent UI riil dari trusted shell component.
- Tambah coverage test:
  - policy ask/deny/allow per trust level,
  - request/session lifecycle,
  - persistence scope semantics.

