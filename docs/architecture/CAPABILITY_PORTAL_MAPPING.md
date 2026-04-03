# SLM Capability-to-Portal Mapping Specification

Dokumen ini mendefinisikan kontrak implementasi antara API portal-facing dan capability internal SLM.
`PolicyEngine` tetap source of truth; portal layer hanya mediator.

## 1) Architecture Summary

```text
ThirdPartyApp
  -> org.desktop.portal.* (DBus)
  -> PortalBackendService
      -> PortalAccessMediator
          -> PortalCapabilityMapper (method -> capability)
          -> PermissionBroker/PolicyEngine
          -> PortalRequestManager / PortalSessionManager
          -> PortalDialogBridge (AskUser)
          -> PortalPermissionStoreAdapter
  -> Internal Desktop Service (Clipboard/Search/Share/PIM/Screenshot/Screencast/Notifications/Shortcuts)
```

Prinsip:
- Portal API untuk app pihak ketiga/sandbox.
- Komponen trusted internal tetap pakai API internal (tetap dijaga capability guard).
- Summary/query dipisah dari full-content/resolve.

## 2) Canonical Mapping Table

Keterangan:
- Flow: `Direct | Request | Session`
- Policy third-party: `Allow | AskUser | Deny | InternalOnly`
- `(*)` = capability baru yang perlu ditambah di enum internal.

| Portal Interface | Portal Method | Internal Capability | Flow | Sensitivity | User Gesture | Persistable | Third-Party Default | Backend Service | Notes |
|---|---|---|---|---|---|---|---|---|---|
| `org.desktop.portal.Clipboard` | `ReadCurrent` | `Clipboard.ReadCurrent` | Request | Medium | No | Optional | AskUser | Clipboard Service | Return current only, bukan full history DB |
| `org.desktop.portal.Clipboard` | `WriteCurrent` | `Clipboard.WriteCurrent` | Direct | Low | No | No | Allow | Clipboard Service | Low-risk write path |
| `org.desktop.portal.Clipboard` | `ReadHistoryPreview` | `Clipboard.ReadHistoryPreview` | Request | High | Yes | No | Deny | Clipboard Service | Summary-only, no raw DB |
| `org.desktop.portal.Clipboard` | `ReadHistoryContent` | `Clipboard.ReadHistoryContent` | Request | High | Yes | Optional | Deny | Clipboard Service | Full content, strict mediation |
| `org.desktop.portal.Clipboard` | `ClearHistory` | `Clipboard.ClearHistory` | Request | High | Yes | Optional | Deny | Clipboard Service | Destructive |
| `org.desktop.portal.Clipboard` | `PinHistoryItem` | `Clipboard.PinItem` | Request | High | Yes | Optional | Deny | Clipboard Service | History mutation |
| `org.desktop.portal.Search` | `QueryClipboardPreview` | `Search.QueryClipboardPreview` | Direct | Medium | No | No | AskUser | Search Service | Summary list only |
| `org.desktop.portal.Search` | `ResolveClipboardResult` | `Search.ResolveClipboardResult` | Request | High | Yes | Optional | Deny | Search+Clipboard | Full item resolve |
| `org.desktop.portal.Search` | `QueryContacts` | `Search.QueryContacts` | Direct | Medium | No | No | AskUser | Search/PIM | Summary only |
| `org.desktop.portal.Search` | `ResolveContact` | `Accounts.ReadContacts` | Request | High | Yes | Optional | AskUser | PIM Service | Detail contact |
| `org.desktop.portal.Search` | `QueryEmailMetadata` | `Search.QueryEmailMetadata` | Direct | Medium | No | No | AskUser | Search/PIM | Subject/sender only |
| `org.desktop.portal.Search` | `ResolveEmailBody` | `Accounts.ReadMailBody` | Request | Critical | Yes | Optional | Deny | PIM Service | High-risk body access |
| `org.desktop.portal.Search` | `QueryCalendarEvents` | `Search.QueryCalendar(*)` | Direct | Medium | No | No | AskUser | Search/PIM | Event summary (capability pending) |
| `org.desktop.portal.Search` | `ResolveCalendarEvent` | `Accounts.ReadCalendar` | Request | High | Yes | Optional | AskUser | PIM Service | Event details |
| `org.desktop.portal.Search` | `QueryFiles` | `Search.QueryFiles` | Direct | Low | No | No | Allow | Search Service | File summary |
| `org.desktop.portal.Search` | `ResolveFileResult` | `Search.ResolveFileResult(*)` | Request | Medium | Yes | Optional | AskUser | Search/FileManager | Safe path + open intent (capability pending) |
| `org.desktop.portal.Share` | `ShareItems` | `Share.Invoke` | Request | Medium | Yes | No | AskUser | Share Service | Trusted picker + target exec internal |
| `org.desktop.portal.Share` | `ShareText` | `Share.Invoke` | Request | Medium | Yes | No | AskUser | Share Service | Same capability |
| `org.desktop.portal.Share` | `ShareFiles` | `Share.Invoke` | Request | Medium | Yes | No | AskUser | Share Service | Same capability |
| `org.desktop.portal.Share` | `ShareUri` | `Share.Invoke` | Request | Medium | Yes | No | AskUser | Share Service | Same capability |
| `org.desktop.portal.OpenWith` | `QueryHandlers` | `OpenWith.QueryHandlers(*)` | Direct | Low | No | No | Allow | FileManager/OpenWith | Metadata-only handlers (capability pending) |
| `org.desktop.portal.OpenWith` | `OpenFileWith` | `OpenWith.Invoke` | Request | Medium | Yes | Optional | AskUser | FileManager/OpenWith | Invoke via trusted launcher |
| `org.desktop.portal.OpenWith` | `OpenUriWith` | `OpenWith.Invoke` | Request | Medium | Yes | Optional | AskUser | FileManager/OpenWith | URI scheme validation |
| `org.desktop.portal.Actions` | `QueryActions` | `QuickAction.Query(*)` | Direct | Low | No | No | Allow | Action Registry | Action metadata only (capability pending) |
| `org.desktop.portal.Actions` | `QueryContextActions` | `FileAction.Query(*)` | Direct | Medium | No | No | AskUser | Action Registry | Context-filtered metadata (capability pending) |
| `org.desktop.portal.Actions` | `InvokeAction` | `QuickAction.Invoke` / `FileAction.Invoke` | Request | Medium | Yes | Optional | AskUser | Action Registry + AppExecutionGate | Invoke gated |
| `org.desktop.portal.PIM` | `QueryContacts` | `Search.QueryContacts` | Direct | Medium | No | No | AskUser | PIM/Search | Summary |
| `org.desktop.portal.PIM` | `ReadContact` | `Accounts.ReadContacts` | Request | High | Yes | Optional | AskUser | PIM Service | Detail |
| `org.desktop.portal.PIM` | `QueryCalendarEvents` | `Search.QueryCalendar(*)` | Direct | Medium | No | No | AskUser | PIM/Search | Summary |
| `org.desktop.portal.PIM` | `ReadCalendarEvent` | `Accounts.ReadCalendar` | Request | High | Yes | Optional | AskUser | PIM Service | Detail |
| `org.desktop.portal.PIM` | `QueryMailMetadata` | `Search.QueryEmailMetadata` | Direct | Medium | No | No | AskUser | PIM/Search | Summary |
| `org.desktop.portal.PIM` | `ReadMailBody` | `Accounts.ReadMailBody` | Request | Critical | Yes | Optional | Deny | PIM Service | Full content strict |
| `org.desktop.portal.PIM` | `PickContact` | `Accounts.ReadContacts` | Request | High | Yes | No | AskUser | PIM Picker | Picker-mediated |
| `org.desktop.portal.PIM` | `PickCalendarEvent` | `Accounts.ReadCalendar` | Request | High | Yes | No | AskUser | PIM Picker | Picker-mediated |
| `org.desktop.portal.Screenshot` | `CaptureScreen` | `Screenshot.CaptureScreen` | Request | High | Yes | Optional | AskUser | Screenshot Service | One-shot capture |
| `org.desktop.portal.Screenshot` | `CaptureWindow` | `Screenshot.CaptureWindow` | Request | High | Yes | Optional | AskUser | Screenshot Service | One-shot capture |
| `org.desktop.portal.Screenshot` | `CaptureArea` | `Screenshot.CaptureArea` | Request | High | Yes | Optional | AskUser | Screenshot Service | Area selector UI |
| `org.desktop.portal.Screencast` | `CreateSession` | `Screencast.CreateSession` | Session | High | Yes | Optional | AskUser | Screencast Service | Session bootstrap |
| `org.desktop.portal.Screencast` | `SelectSources` | `Screencast.SelectSources` | Session | High | Yes | No | AskUser | Screencast Service | Trusted source picker |
| `org.desktop.portal.Screencast` | `Start` | `Screencast.Start` | Session | High | Yes | Optional | AskUser | Screencast Service | Active stream |
| `org.desktop.portal.Screencast` | `Stop` | `Screencast.Stop` | Session | Medium | No | No | Allow | Screencast Service | Session close |
| `org.desktop.portal.Notifications` | `SendNotification` | `Notifications.Send` | Direct | Low | No | No | Allow | Notification Service | Existing safe path |
| `org.desktop.portal.Notifications` | `QueryHistoryPreview` | `Notifications.ReadHistoryPreview(*)` | Request | High | Yes | Optional | Deny | Notification Service | Prefer app-owned only |
| `org.desktop.portal.Notifications` | `ReadHistoryItem` | `Notifications.ReadHistoryContent(*)` | Request | High | Yes | Optional | Deny | Notification Service | System-wide stays internal |
| `org.desktop.portal.GlobalShortcuts` | `CreateSession` | `GlobalShortcuts.CreateSession` | Session | Medium | Yes | Optional | AskUser | Shortcut Service | Revocable |
| `org.desktop.portal.GlobalShortcuts` | `BindShortcuts` | `GlobalShortcuts.Register` | Session | Medium | Yes | Optional | AskUser | Shortcut Service | Collision-check required |
| `org.desktop.portal.GlobalShortcuts` | `ListBindings` | `GlobalShortcuts.List` | Session | Low | No | No | AskUser | Shortcut Service | Only own bindings |
| `org.desktop.portal.GlobalShortcuts` | `Activate` | `GlobalShortcuts.Activate` | Session | Medium | Yes | No | AskUser | Shortcut Service | Enable registered scope |
| `org.desktop.portal.GlobalShortcuts` | `Deactivate` | `GlobalShortcuts.Deactivate` | Session | Medium | No | No | Allow | Shortcut Service | Disable own scope |

## 3) Detailed Per-Subsystem API Contract

### Clipboard
- Portal-exposed: `WriteCurrent`, optional mediated `ReadCurrent`.
- History API (`ReadHistoryPreview/Content/Clear/Pin`) default third-party deny.
- Raw history list/database: **InternalOnly**.
- Summary vs full:
  - preview: id/type/snippet/time.
  - full: content payload, stricter ask/deny path.

### Global Search
- Query methods return summaries only.
- Resolve methods return richer details via Request flow.
- `ResolveEmailBody`/high-risk resolve: default deny or ask with strict gating.

### Share
- Always one-shot Request.
- Target selection in trusted share picker.
- Target execution hanya di share service internal.
- Persist biasanya tidak relevan.

### Open With
- `QueryHandlers` low-risk metadata (direct).
- `Open*With` butuh gesture + Request.
- Persist handler default boleh per app/resource (opsional).

### Quick Actions / File Actions
- Query metadata direct.
- Invocation hanya via Request + AppExecutionGate.
- Provider metadata boleh expose, execution tetap mediated.

### PIM (Contacts/Calendar/Email)
- Query = summaries only.
- Read detail = Request.
- Mail body = highest risk, default deny third-party.
- Picker APIs dianjurkan untuk akses detail terkontrol.

### Screenshot
- Semua capture via Request one-shot.
- Gesture mandatory.
- Area/window picker dari trusted UI.

### Screencast
- Session-based penuh (`CreateSession -> SelectSources -> Start -> Stop`).
- Session revocable/expirable.
- Source picker wajib trusted UI.

### Notifications
- Send = direct allow.
- Read history system-wide = default deny third-party, cenderung internal-only.
- Kalau expose, batasi per-app-owned notifications.

### Global Shortcuts
- Session-based.
- Bind harus lewat consent + collision handling.
- Hanya daftar binding milik app itu sendiri.

## 4) Request vs Session Decision Rules

- `Direct`:
  - low-risk metadata/query, no ongoing grant, no picker.
- `Request`:
  - one-shot sensitive invoke/read, consent/picker mungkin dibutuhkan.
- `Session`:
  - akses aktif berkelanjutan, revocable, punya lifecycle (`close/revoke/expire`).

## 5) Default Policy Matrix (Portal Methods)

Legend:
- `A` Allow, `D` Deny, `Q` AskUser, `I` InternalOnly, `SA` ScopedAllow.

| Method Group | CoreDesktopComponent | PrivilegedDesktopService | ThirdPartyApplication |
|---|---:|---:|---:|
| Clipboard WriteCurrent | A | SA | A |
| Clipboard ReadCurrent | A | SA | Q |
| Clipboard History Preview/Content/Clear/Pin | A | SA | D |
| Search QueryFiles/Apps | A | SA | A |
| Search QueryClipboard/Contacts/EmailMeta/CalendarSummary | A | SA | Q |
| Search ResolveClipboard/ResolveContact/ResolveCalendar | A | SA | Q |
| Search ResolveEmailBody | SA | SA | D/Q (strict) |
| Share Invoke | A | A | Q |
| OpenWith QueryHandlers | A | SA | A |
| OpenWith Invoke | A | SA | Q |
| QuickAction/FileAction Query | A | SA | A |
| QuickAction/FileAction Invoke | A | SA | Q |
| PIM Query summaries | A | SA | Q |
| PIM Read detail | A | SA | Q |
| PIM Read mail body | SA | SA | D/Q |
| Screenshot Capture* | A | SA | Q |
| Screencast session flow | A | SA | Q |
| Notifications Send | A | SA | A |
| Notifications Read history | A | SA | I/D |
| GlobalShortcuts session flow | A | SA | Q |

## 6) Example DBus Interface Skeletons

```xml
<!-- org.desktop.portal.Clipboard -->
<interface name="org.desktop.portal.Clipboard">
  <method name="ReadCurrent">
    <arg name="options" type="a{sv}" direction="in"/>
    <arg name="request_handle" type="o" direction="out"/>
  </method>
  <method name="WriteCurrent">
    <arg name="payload" type="a{sv}" direction="in"/>
    <arg name="result" type="a{sv}" direction="out"/>
  </method>
</interface>

<!-- org.desktop.portal.Search -->
<interface name="org.desktop.portal.Search">
  <method name="QueryFiles">
    <arg name="query" type="s" direction="in"/>
    <arg name="options" type="a{sv}" direction="in"/>
    <arg name="results" type="aa{sv}" direction="out"/>
  </method>
  <method name="ResolveEmailBody">
    <arg name="message_id" type="s" direction="in"/>
    <arg name="options" type="a{sv}" direction="in"/>
    <arg name="request_handle" type="o" direction="out"/>
  </method>
</interface>

<!-- org.desktop.portal.Share -->
<interface name="org.desktop.portal.Share">
  <method name="ShareItems">
    <arg name="items" type="aa{sv}" direction="in"/>
    <arg name="options" type="a{sv}" direction="in"/>
    <arg name="request_handle" type="o" direction="out"/>
  </method>
</interface>

<!-- org.desktop.portal.Screencast -->
<interface name="org.desktop.portal.Screencast">
  <method name="CreateSession">
    <arg name="options" type="a{sv}" direction="in"/>
    <arg name="session_handle" type="o" direction="out"/>
  </method>
  <method name="Start">
    <arg name="session_handle" type="o" direction="in"/>
    <arg name="options" type="a{sv}" direction="in"/>
    <arg name="request_handle" type="o" direction="out"/>
  </method>
  <method name="Stop">
    <arg name="session_handle" type="o" direction="in"/>
    <arg name="result" type="a{sv}" direction="out"/>
  </method>
</interface>
```

Interface lain mengikuti pola sama:
- `org.desktop.portal.OpenWith`
- `org.desktop.portal.Actions`
- `org.desktop.portal.PIM`
- `org.desktop.portal.Screenshot`
- `org.desktop.portal.Notifications`
- `org.desktop.portal.GlobalShortcuts`

## 7) Example Request / Response Payloads

### 7.1 Clipboard preview request
- Input:
```json
{"method":"ReadHistoryPreview","options":{"limit":20}}
```
- Mapped: `Clipboard.ReadHistoryPreview`
- AccessContext: `{resourceType:"clipboard-history", sensitivity:"High", initiatedByUserGesture:true}`
- Decision path: `Deny` default third-party.
- Async response:
```json
{"ok":false,"error":"PermissionDenied","reason":"default-deny"}
```

### 7.2 Share text request
- Input:
```json
{"method":"ShareText","options":{"text":"hello","title":"Share"}}
```
- Mapped: `Share.Invoke`
- Decision path: `AskUser` -> trusted picker -> allow once.
- Response:
```json
{"ok":true,"target":"org.example.ShareTarget","requestPath":"/org/desktop/portal/request/app/r1"}
```

### 7.3 Read mail metadata
- Input:
```json
{"method":"QueryMailMetadata","query":"build report"}
```
- Mapped: `Search.QueryEmailMetadata`
- Flow: direct summary.
- Response rows contain only `id,subject,sender,timestamp,unread`.

### 7.4 Read mail body
- Input:
```json
{"method":"ReadMailBody","messageId":"msg-42"}
```
- Mapped: `Accounts.ReadMailBody`
- Decision: `Deny` or `AskUser` strict.
- Response:
```json
{"ok":false,"error":"PermissionDenied","reason":"high-risk-content"}
```

### 7.5 Screenshot area capture
- Input:
```json
{"method":"CaptureArea","options":{"interactive":true}}
```
- Mapped: `Screenshot.CaptureArea`
- Flow: Request + gesture + picker.
- Response:
```json
{"ok":true,"uri":"file:///home/user/Pictures/Screenshots/shot.png"}
```

### 7.6 Screencast start flow
1. `CreateSession` -> returns `sessionPath`.
2. `SelectSources(sessionPath)` -> Request.
3. `Start(sessionPath)` -> Request (returns stream descriptor).
4. `Stop(sessionPath)` -> closes active stream.

### 7.7 Bind global shortcut
- Input:
```json
{"method":"BindShortcuts","session":"...","bindings":[{"id":"toggle","accelerator":"Ctrl+Shift+Space"}]}
```
- Mapped: `GlobalShortcuts.Register`
- Flow: Session + Request, collision check.
- Response:
```json
{"ok":true,"accepted":[{"id":"toggle","accelerator":"Ctrl+Shift+Space"}],"rejected":[]}
```

## 8) Trusted Internal API vs Portal-facing API

- **Internal-only (trusted)**:
  - raw clipboard history db/content,
  - full system notification history,
  - unrestricted search provider internals,
  - unrestricted action execution.
- **Portal-exposed (third-party)**:
  - mediated summaries,
  - one-shot invoke with request object,
  - long-lived access via revocable session.

## 9) Security Boundary Notes

- Summary/resolve split mencegah exfiltration data massal.
- `initiatedByUserGesture` tidak boleh dipercaya dari caller untrusted tanpa trusted flow marker.
- Persist decision tidak bypass policy; hanya input evaluasi.
- Audit log simpan decision metadata, bukan sensitive payload.

## 10) Recommended Implementation Order

1. **Share** (nilai UX tinggi, risiko moderat, request-only).
2. **Screenshot** (request flow jelas, picker sudah siap).
3. **GlobalShortcuts** (session + collision control).
4. **Screencast** (session lifecycle penuh).
5. **Search summaries** (direct low-risk query).
6. **PIM metadata** (summary contract kuat).
7. **Clipboard mediated access** (tinggi risiko, butuh hardening).
8. **Notification read access** (kemungkinan tetap internal-only).
9. **High-risk full resolves** (`ReadMailBody`, full clipboard content).

Alasan:
- mulai dari flow yang UX-value tinggi dan dependency siap,
- menunda data paling sensitif sampai pipeline request/session + consent + audit matang.
