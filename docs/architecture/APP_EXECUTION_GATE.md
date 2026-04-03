# AppExecutionGate Architecture

Dokumen ini merinci arsitektur jalur eksekusi aplikasi satu pintu (`AppExecutionGate`).

## Component Diagram

```mermaid
flowchart LR
    UI["UI Surfaces
Dock / Launchpad / FileManager / Tothespot / Global Menu"] --> Router["AppCommandRouter (optional)"]
    Router --> Gate["AppExecutionGate"]
    Gate --> Runtime["AppRuntimeRegistry"]
    Gate --> DockModel["DockModel"]
    Gate --> Process["QProcess / launcher path"]
    Gate --> Observability["Diagnostics + RoutedDetailed signal"]
```

## Launch Sequence

```mermaid
sequenceDiagram
    participant UI as UI Entry Point
    participant Router as AppCommandRouter
    participant Gate as AppExecutionGate
    participant Runtime as AppRuntimeRegistry
    participant Dock as DockModel
    participant Proc as Launcher/Process

    UI->>Router: route(action, payload, source)
    Router->>Gate: launchDesktopEntry / launchEntryMap / launchCommand
    Gate->>Gate: validate payload + resolve target
    Gate->>Runtime: register launch hint
    Gate->>Dock: activateOrLaunch(...)
    Dock-->>Gate: launch request accepted
    Gate->>Proc: start detached/process
    Gate->>Dock: noteLaunchedEntry(...)
    Gate->>Dock: refreshRunningStates()
    Gate-->>Router: result {ok,error,...}
    Router-->>UI: routedDetailed(resultMap)
```

## Design Rules

- Semua jalur launch baru wajib masuk ke `AppExecutionGate`.
- UI tidak boleh mengeksekusi proses aplikasi langsung.
- `AppCommandRouter` dipakai untuk normalisasi action lintas modul.
- Status runtime dan launch-hint wajib disinkronkan ke `DockModel` dan `AppRuntimeRegistry`.

