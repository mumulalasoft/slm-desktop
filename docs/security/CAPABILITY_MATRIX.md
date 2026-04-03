# SLM Capability Matrix

Matriks ini merangkum **default policy** saat ini pada `PolicyEngine` untuk tiap capability berdasarkan trust level:

- `CoreDesktopComponent`
- `PrivilegedDesktopService`
- `ThirdPartyApplication`

Legenda keputusan:

- `Allow`
- `AskUser`
- `Deny`
- `*` = bergantung gesture / official UI / kondisi khusus

## 1. Clipboard

| Capability | Core | Privileged | Third-party |
|---|---:|---:|---:|
| `Clipboard.ReadCurrent` | Allow | Allow | Deny |
| `Clipboard.WriteCurrent` | Allow | Allow | Allow |
| `Clipboard.ReadHistoryPreview` | Allow | Allow | Deny |
| `Clipboard.ReadHistoryContent` | AskUser* | Allow | Deny* |
| `Clipboard.DeleteHistory` | Allow | Allow | Deny |
| `Clipboard.PinItem` | Allow | Allow | Deny |
| `Clipboard.ClearHistory` | Allow | Allow | Deny |

## 2. Search

| Capability | Core | Privileged | Third-party |
|---|---:|---:|---:|
| `Search.QueryApps` | Allow | Allow | Allow |
| `Search.QueryFiles` | Allow | Allow | Allow |
| `Search.QueryClipboardPreview` | Allow | Allow | Deny |
| `Search.ResolveClipboardResult` | Allow | Allow | Deny |
| `Search.QueryContacts` | Allow | Allow | Deny |
| `Search.QueryEmailMetadata` | Allow | Allow | Deny |
| `Search.QueryEmailBody` | Allow | Allow | Deny |

## 3. Actions

| Capability | Core | Privileged | Third-party |
|---|---:|---:|---:|
| `Share.Invoke` | AskUser* | Allow | AskUser* |
| `OpenWith.Invoke` | AskUser* | Allow | AskUser* |
| `QuickAction.Invoke` | AskUser* | Allow | AskUser* |
| `FileAction.Invoke` | AskUser* | Allow | AskUser* |

## 4. Accounts

| Capability | Core | Privileged | Third-party |
|---|---:|---:|---:|
| `Accounts.ReadContacts` | Allow | Allow | AskUser |
| `Accounts.ReadCalendar` | Allow | Allow | AskUser |
| `Accounts.ReadMailMetadata` | Allow | Allow | AskUser |
| `Accounts.ReadMailBody` | Allow | AskUser* | Deny |

## 5. Screen

| Capability | Core | Privileged | Third-party |
|---|---:|---:|---:|
| `Screenshot.CaptureScreen` | AskUser* | Allow | AskUser* |
| `Screenshot.CaptureWindow` | AskUser* | Allow | AskUser* |
| `Screenshot.CaptureArea` | AskUser* | Allow | AskUser* |
| `Screencast.CreateSession` | AskUser* | Allow | AskUser* |
| `Screencast.SelectSources` | AskUser* | Allow | AskUser* |
| `Screencast.Start` | AskUser* | Allow | AskUser* |
| `Screencast.Stop` | Allow | Allow | Allow |

## 6. Notifications

| Capability | Core | Privileged | Third-party |
|---|---:|---:|---:|
| `Notifications.Send` | Allow | Allow | Allow |
| `Notifications.ReadHistory` | Allow | Allow | Deny |

## 7. Global Shortcuts

| Capability | Core | Privileged | Third-party |
|---|---:|---:|---:|
| `GlobalShortcuts.CreateSession` | AskUser* | Allow | AskUser* |
| `GlobalShortcuts.Register` | AskUser* | Allow | AskUser* |
| `GlobalShortcuts.List` | Allow | Allow | AskUser |
| `GlobalShortcuts.Activate` | AskUser* | Allow | AskUser* |
| `GlobalShortcuts.Deactivate` | Allow | Allow | Allow |

## 8. Important Conditions

- Third-party pada capability yang `requiresUserGesture=true`:
  - tanpa gesture -> `Deny`
  - dengan gesture -> umumnya `AskUser` untuk capability yang masuk bucket mediation.
- Core component pada capability gesture-gated:
  - jika bukan gesture dan bukan official UI flow -> `AskUser`.
- Privileged service:
  - mayoritas `Allow`,
  - pengecualian saat ini: `Accounts.ReadMailBody` untuk non-official privileged -> `AskUser`.

## 9. Source of Truth

- Enum capability: `src/core/permissions/Capability.h`
- Rule default policy: `src/core/permissions/PolicyEngine.cpp`
- Test matrix: `tests/policyengine_test.cpp`
