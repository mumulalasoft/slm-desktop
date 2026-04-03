# SLM Capability Service Architecture

Dokumen ini menjelaskan jalur service untuk framework capability (`org.freedesktop.SLMCapabilities` dan facade internal terkait).

## Service Component Diagram

```mermaid
flowchart LR
    Client["Clients
FileManager / Tothespot / ShareSheet / Launcher / Dock"] --> DBus["DBus Service
SlmCapabilityService / SlmCapabilityFrameworkService"]
    DBus --> Framework["SlmActionFramework"]
    Framework --> Scanner["DesktopEntryScanner (GIO)"]
    Framework --> Parser["DesktopEntryParser + SlmMetadataParser + ACL"]
    Framework --> Registry["ActionRegistryIndex + Cache"]
    DBus --> Resolver["CapabilityResolverLayer + Builders"]
    Resolver --> Invoker["ActionInvoker"]
    Invoker --> Gate["AppExecutionGate"]
```

## Request Flow: `ListActions(capability, context)`

```mermaid
sequenceDiagram
    participant Client
    participant DBus as SlmCapabilityService
    participant FW as SlmActionFramework
    participant R as ActionRegistryIndex
    participant C as ACL/Capability Resolver

    Client->>DBus: ListActions(capability, context)
    DBus->>FW: listActionsWithContext(...)
    FW->>R: query by capability
    R-->>FW: candidate actions
    FW->>C: evaluate ACL + target/scope/mime filters
    C-->>FW: resolved actions
    FW-->>DBus: aa{sv}
    DBus-->>Client: aa{sv}
```

## Request Flow: `InvokeAction(action_id, context)`

```mermaid
sequenceDiagram
    participant Client
    participant DBus as SlmCapabilityService
    participant FW as SlmActionFramework
    participant I as ActionInvoker
    participant G as AppExecutionGate

    Client->>DBus: InvokeAction(action_id, context)
    DBus->>FW: invokeAction(...)
    FW->>I: expand placeholders + validate input
    I->>G: launchDesktopEntry/launchEntryMap/launchCommand
    G-->>I: launch result
    I-->>FW: invoke result map
    FW-->>DBus: a{sv}
    DBus-->>Client: a{sv}
```

## Contract Notes

- Registry shared untuk semua capability.
- ACL engine shared untuk semua capability.
- Resolver spesifik capability hanya memuat filtering/ranking behavior.
- Jalur eksekusi tetap satu pintu melalui `AppExecutionGate`.

