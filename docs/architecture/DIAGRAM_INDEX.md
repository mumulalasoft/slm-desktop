# SLM Architecture Diagram Index

Dokumen ini menjadi entry point tunggal untuk diagram arsitektur SLM.

## 1) Execution Flow (One Gate)

- File: `docs/ARCHITECTURE.md`
- Section: `Execution flow (one gate)`
- Fokus:
  - Semua launch app melalui `AppExecutionGate`.
  - Jalur UI -> Router -> Gate -> Dock/runtime state reconciliation.

## 2) AppExecutionGate Detailed Diagram

- File: `docs/architecture/APP_EXECUTION_GATE.md`
- Fokus:
  - Component boundary `UI -> Router -> Gate -> Runtime/Dock/Process`.
  - Sequence launch dari action route sampai runtime refresh.

## 3) SLM Action Framework Module Graph

- File: `docs/architecture/SLM_ACTION_FRAMEWORK_MODULES.md`
- Section: `Module Graph`
- Fokus:
  - Scanner/Parser/Metadata/Registry/Resolver/Invoker.
  - Integrasi `ActionInvoker` dengan `AppExecutionGate`.

## 4) SLM Capability Service (DBus + Framework)

- File: `docs/architecture/SLM_CAPABILITY_SERVICE.md`
- Fokus:
  - Boundary client -> DBus service -> framework/resolver/invoker.
  - Sequence `ListActions` dan `InvokeAction`.

## 5) Tothespot End-to-End Flow

- File: `docs/architecture/TOTHESPOT_FLOW.md`
- Fokus:
  - Sequence query (`Query` -> `SearchActions` -> ranking -> results).
  - Sequence activation (`ActivateResult` -> `InvokeAction` -> execution gate).

## 6) Motion / Animation Framework

- File: `docs/architecture/ANIMATION_FRAMEWORK.md`
- Fokus:
  - Layered motion pipeline (`Input -> Gesture -> Scheduler -> Engine -> Scene -> Compositor`).
  - Interruptible spring/physics/gesture model untuk shell-scale transitions.

## 7) Portal Adapter Layer

- File: `docs/architecture/PORTAL_ADAPTER_LAYER.md`
- Fokus:
  - Boundary app-facing portal request/session vs internal trusted API.
  - Mediation flow `Portal -> Mapper -> PolicyEngine -> consent/persist -> service`.

## 8) Capability-to-Portal Mapping Spec

- File: `docs/architecture/CAPABILITY_PORTAL_MAPPING.md`
- Fokus:
  - Kontrak kanonis method portal -> capability internal.
  - Flow classification (`Direct/Request/Session`), sensitivity, policy default, backend target.

## 9) Clipboard + Search Integration Contract

- File: `docs/architecture/CLIPBOARD_SEARCH_CONTRACT.md`
- Fokus:
  - Boundary security antara Clipboard history dan Global Search.
  - Kontrak `summary query` vs `full resolve` + trust/policy split.

## 10) Workspace Hybrid Plan (Overview -> Workspace)

- File: `docs/architecture/WORKSPACE_HYBRID_PLAN.md`
- Fokus:
  - Audit komponen reusable untuk implementasi workspace-strip hybrid.
  - Mapping reused vs komponen baru minimal (`WindowWorkspaceBinding`, `WorkspaceStripModel`, `DockWorkspaceIntegration`).
  - Urutan patch non-invasif untuk migrasi production.

## 11) Recommended Reading Order

1. `docs/ARCHITECTURE.md`
2. `docs/architecture/APP_EXECUTION_GATE.md`
3. `docs/architecture/SLM_ACTION_FRAMEWORK_MODULES.md`
4. `docs/architecture/SLM_CAPABILITY_SERVICE.md`
5. `docs/architecture/TOTHESPOT_FLOW.md`
6. `docs/architecture/ANIMATION_FRAMEWORK.md`
7. `docs/architecture/PORTAL_ADAPTER_LAYER.md`
8. `docs/architecture/CAPABILITY_PORTAL_MAPPING.md`
9. `docs/architecture/CLIPBOARD_SEARCH_CONTRACT.md`
10. `docs/architecture/WORKSPACE_HYBRID_PLAN.md`
11. `docs/architecture/PERMISSION_FOUNDATION.md`
12. `docs/security/PERMISSION_PLAYBOOK.md`
13. `docs/DESKTOP_DAEMON_ARCHITECTURE.md`
14. `docs/DBUS_API_CHANGELOG.md`
