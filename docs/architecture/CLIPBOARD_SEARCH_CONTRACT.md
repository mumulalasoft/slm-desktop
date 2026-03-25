# Clipboard + Global Search Integration Contract

## Scope
Dokumen ini mendefinisikan kontrak integrasi antara:
- `ClipboardService`
- `ClipboardSearchProvider`
- `GlobalSearchService`
- Trusted UI (`ClipboardOverlay`, `ClipboardApplet`, `GlobalSearchUI`)
- Portal adapter untuk aplikasi pihak ketiga

Fokus: pemisahan **preview query** vs **full resolution** untuk mencegah kebocoran data clipboard sensitif.

## Architecture Summary
- `ClipboardService` adalah single owner untuk history clipboard.
- `ClipboardSearchProvider` hanya expose proyeksi summary aman (preview).
- `GlobalSearchService` hanya consume provider summary, tidak membaca raw storage clipboard.
- Full content hanya melalui resolve satu item + policy check + gesture check.
- API raw clipboard history tidak dipaparkan ke third-party secara langsung.

## Canonical Model Choice
### Chosen: Model B
`GlobalSearchService` query ke `ClipboardSearchProvider` (summary-only), lalu resolve balik ke `ClipboardService` untuk item yang dipilih user.

### Rejected: Model A
`GlobalSearchService` baca raw clipboard DB langsung.
Alasan ditolak:
- boundary keamanan rusak (search jadi implicit superuser),
- sulit audit/least-privilege,
- sulit enforce redaction konsisten.

## Capability Contract
### Clipboard domain
- `Clipboard.ReadCurrent`
- `Clipboard.WriteCurrent`
- `Clipboard.ReadHistoryPreview`
- `Clipboard.ReadHistoryContent`
- `Clipboard.DeleteHistory`
- `Clipboard.PinItem`
- `Clipboard.ClearHistory`

### Search domain
- `Search.QueryClipboardPreview`
- `Search.ResolveClipboardResult`

### Distinction
- `Clipboard.ReadHistoryPreview`: akses preview langsung dari clipboard subsystem.
- `Clipboard.ReadHistoryContent`: akses konten penuh history.
- `Search.QueryClipboardPreview`: summary hasil query via search pipeline.
- `Search.ResolveClipboardResult`: resolve satu hasil terpilih ke data lebih kaya.

## Data Flow
1. User copy content.
2. `ClipboardWatcher` menangkap event.
3. `ClipboardSecurityPolicy` klasifikasi sensitive/non-sensitive.
4. `ClipboardHistoryStore` simpan (atau redaksi/skip untuk sensitive).
5. `ClipboardPreviewGenerator` buat preview aman.
6. `ClipboardSearchProvider` update index token summary.
7. `GlobalSearchUI` query -> `GlobalSearchService.Query`.
8. `GlobalSearchService` minta summary ke provider.
9. User pilih result.
10. `GlobalSearchService.ResolveClipboardResult` (dengan context gesture) -> `ClipboardService`.
11. Policy check capability + trust + gesture.
12. Jika allowed, item resolved dan UI lakukan recopy ke current clipboard.

## Search Summary Contract
`ClipboardSearchSummary`:
- `resultId`
- `clipboardItemId` (opaque)
- `itemType` (`text|url|file|code|image|color|sensitive`)
- `previewText` (maks 80 chars, normalized single-line)
- `timestampBucket` (minute-level; bukan ms exact)
- `sourceAppLabel` (opsional, trusted-only)
- `pinned`
- `iconName`
- `score`

Rules:
- Tidak boleh ada full content.
- Sensitive item default: exclude dari search; fallback boleh `Sensitive item` redacted.
- Image default: label/thumbnailRef trusted-only, bukan raw bytes.
- File URI default: index filename, bukan full path untuk untrusted.

## Full Resolution Contract
Method: `ResolveClipboardResult(resultId, context)`

`ClipboardResolvedItem`:
- `clipboardItemId`
- `itemType`
- `displayMode` (`text|uri|imageRef|redacted`)
- `fullText` (trusted, user-selected flow)
- `fileUri` (trusted scoped)
- `imageCachePath` (trusted scoped)
- `requiresRecopyBeforePaste` (default true)
- `isSensitive`
- `isRedacted`

Resolution policy:
- Trusted UI + explicit selection gesture: scoped allow.
- Background consumer tanpa gesture: deny by default.
- Third-party via portal: deny by default.

## Internal vs Portal API Boundary
### Internal trusted only
- Raw history preview/content listing.
- Delete/pin/clear history.
- Full item resolve for clipboard history.

### Portal-facing (conservative)
- Optional narrow: `QueryClipboardPreview` summary-only.
- `RequestClipboardContent` tetap deny-by-default, dengan mediation ketat bila diaktifkan.
- `ReadHistoryPreview/Content` tidak diekspos sebagai API portal umum.

## Default Policy Matrix
- `ClipboardOverlay`:
  - Preview: Allow
  - Full content: ScopedAllow (gesture)
- `ClipboardApplet`:
  - Preview: Allow
  - Full content: ScopedAllow (click)
- `GlobalSearchUI`:
  - Query preview: Allow
  - Resolve result: ScopedAllow (selection gesture)
- `GlobalSearchService`:
  - Query summary provider: Allow
  - Raw history content dump: Deny/InternalOnly
- `ThirdPartyApplication`:
  - Query clipboard preview (portal): Deny default
  - Resolve clipboard content (portal): Deny default
- `PortalMediator`:
  - ScopedAllow broker role only

## Sensitive Content Policy (Default)
Pattern keluarga sensitif:
- password/token/otp/secret/recovery/auth-header/cookie/session/private-key

Default handling:
1. Storage: simpan minimal atau ephemeral (opsional policy strict: skip save).
2. Preview generation: redacted.
3. Search index: exclude by default.
4. Query summary: return nothing atau redacted label.
5. Resolve: trusted-only + gesture + policy.
6. UI applet/overlay: tampil label sensitif, reveal terbatas.

## DBus Interface Skeleton (Contract Level)
### `org.slm.Desktop.Clipboard` (internal)
- `GetHistoryPreview(options) -> aa{sv}`
- `ResolveHistoryItem(itemId, context) -> a{sv}`
- `RecopyToCurrentClipboard(itemId) -> b`
- `PinItem(itemId, pinned) -> b`
- `DeleteItem(itemId) -> b`
- `ClearHistory(options) -> b`

### `org.slm.Desktop.Search.v1` (internal)
- `Query(text, options) -> a(sa{sv})`
- `ResolveClipboardResult(resultId, context) -> a{sv}`

### `org.slm.Desktop.SearchProvider.Clipboard` (internal provider contract)
- `QueryPreview(query, options) -> aa{sv}`
- `ResolveResult(resultId, context) -> a{sv}`
- `GetRecentPreview(limit, options) -> aa{sv}`

### `org.slm.Desktop.Portal` (optional narrow)
- `QueryClipboardPreview(query, options) -> a{sv}` (summary-only)
- `RequestClipboardContent(options) -> a{sv}` (request handle; deny default)

## UX Contract
- Global search row: tampil preview ringkas, jangan tampil secret penuh.
- Sensitive item: label `Sensitive item`.
- Resolusi item dari search: recopy ke clipboard current dulu, bukan broadcast full content ke semua consumer.
- Applet/overlay: preview-first; full reveal only by direct user intent.

## Audit Logging
Wajib log:
- caller identity
- capability
- action (`query_preview|resolve_result|pin|delete|clear|recopy`)
- decision
- timestamp
- userGesture flag

Jangan log:
- full clipboard text
- token/otp/password/secret payload.

## Recommended Implementation Order
1. Secure preview pipeline di `ClipboardService`.
2. `ClipboardSearchProvider` summary query + token index.
3. `GlobalSearchService` integrate summary provider.
4. `ResolveClipboardResult` dengan gesture+policy.
5. `ClipboardOverlay` / `ClipboardApplet` pakai kontrak yang sama.
6. Sensitive filtering + redaction hardening.
7. Optional portal endpoint (strict deny default).
