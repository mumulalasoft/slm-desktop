# Tothespot End-to-End Flow

Dokumen ini merinci jalur data Tothespot dari input query sampai action dijalankan.

## Component Diagram

```mermaid
flowchart LR
    UI["Tothespot UI (QML)"] --> GS["GlobalSearchService"]
    GS --> DBus["org.freedesktop.SLMCapabilities"]
    DBus --> CapSvc["SlmCapabilityService / SlmCapabilityFrameworkService"]
    CapSvc --> Framework["SlmActionFramework"]
    Framework --> Registry["ActionRegistryIndex + ACL + Builders"]
    Framework --> Ranker["SearchActionRanker"]
    UI --> Activate["ActivateResult / Open action"]
    Activate --> GS
    GS --> Router["AppCommandRouter"]
    Router --> Gate["AppExecutionGate"]
```

## Query Sequence

```mermaid
sequenceDiagram
    participant U as Tothespot UI
    participant G as GlobalSearchService
    participant S as SlmCapabilityService
    participant F as SlmActionFramework
    participant R as SearchActionRanker

    U->>G: Query(text, options{scope=tothespot,...})
    G->>S: SearchActions(query, context)
    S->>F: searchActions(query, context)
    F->>F: capability filter + ACL evaluation
    F->>R: rank(results)
    R-->>F: scored actions
    F-->>S: results aa{sv}
    S-->>G: results aa{sv}
    G-->>U: normalized results model
```

## Activation Sequence

```mermaid
sequenceDiagram
    participant U as Tothespot UI
    participant G as GlobalSearchService
    participant S as SlmCapabilityService
    participant F as SlmActionFramework
    participant I as ActionInvoker
    participant A as AppCommandRouter
    participant E as AppExecutionGate

    U->>G: ActivateResult(id, activate_data)
    G->>S: InvokeAction(action_id, context)
    S->>F: invokeAction(...)
    F->>I: validate + expand placeholders
    I->>A: route/dispatch
    A->>E: launchDesktopEntry / launchEntryMap / launchCommand
    E-->>A: launch result
    A-->>I: dispatch result
    I-->>F: invoke result
    F-->>S: a{sv}
    S-->>G: a{sv}
    G-->>U: success/failure state
```

## Notes

- Scope canonical wajib `tothespot`.
- Hasil dari capability framework dapat digabung dengan provider lain (mis. recent files) di level `GlobalSearchService`.
- Eksekusi akhir tetap satu pintu melalui `AppExecutionGate`.

