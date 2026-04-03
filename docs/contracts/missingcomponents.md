# Contract: MissingComponents Controller

API identity:
- QML context property: `MissingComponents`
- C++ type: `Slm::System::MissingComponentController`

## Stable invokables (baseline)
- `missingComponentsForDomain(domain: string) -> list`
- `blockingMissingComponentsForDomain(domain: string) -> list`
- `hasBlockingMissingForDomain(domain: string) -> bool`
- `installComponent(componentId: string) -> map`
- `installComponentForDomain(domain: string, componentId: string) -> map`

## Domain semantics
- Supported domains include at least:
  - `greeter`
  - `recovery`
  - `filemanager`
  - `settings`
- Unknown domains:
  - must not crash
  - should return empty result or structured error for install paths

## Missing issue row shape
- Required fields:
  - `componentId` (string)
  - `displayName` (string)
  - `packageName` (string, when known)
  - `severity` (`required` or `recommended`)
  - `installed` (bool)

## Install result map shape
- Required fields:
  - `ok` (bool)
  - `error` (string when `ok == false`)
- Recommended fields:
  - `componentId` (string)
  - `domain` (string for domain-scoped installer)

## Blocking semantics
- `required` issues are blocking.
- `recommended` issues are non-blocking warnings.
- `hasBlockingMissingForDomain(domain)` must be consistent with
  `blockingMissingComponentsForDomain(domain).size() > 0`.

## Compatibility policy
- Additive row fields are allowed.
- Existing required keys and severity semantics are stable for v1.0.

## Test coverage
- `tests/missingcomponentcontroller_test.cpp`
- `tests/settings_missing_components_ui_test.cpp`
