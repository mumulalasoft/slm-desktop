# Modular Structure (Target)

This file defines the target split so Slm Desktop can be reused by other apps.
The current build remains in root for stability; migration is incremental.

## Target layout

```text
apps/
  slm-desktop/
  tools/
    uipreference/
    indicatorctl/

modules/
  slm-desktop-core/
    include/desktop_shell_core/
    src/
  slm-desktop-style/
    Style/
```

## Module responsibilities

### `modules/slm-desktop-core`

Shared runtime APIs for all apps:
- `UIPreferences` C++ API + DBus contract for centralized preference.
- Theme contract (token names, dark/light policy, icon theme mapping).
- Shared service helpers (future): app execution gate, indicator registry client.

### `modules/slm-desktop-style`

Reusable Qt Quick Controls style:
- `Style/qmldir`
- all style controls (`Menu`, `Button`, `Switch`, `ComboBox`, etc.)
- no shell business logic

### `apps/slm-desktop`

Desktop shell app only:
- panel, dock, launchpad, shell surface
- compositing/session integration
- feature managers (network/bluetooth/sound/notification/global menu)

### `apps/tools/*`

Tooling binaries:
- `uipreference`
- `indicatorctl`

## Migration map (current -> target)

- `uipreferences.*` -> `modules/slm-desktop-core/src`
- `themeiconcontroller.*`, theme token contract -> `modules/slm-desktop-core/src`
- `Style/*` -> `modules/slm-desktop-style/Style`
- `uipreference_cli.cpp` -> `apps/tools/uipreference`
- `src/cli/indicatorctl.cpp` -> `apps/tools/indicatorctl`
- `main.cpp`, `Qml/*`, shell managers -> `apps/slm-desktop`

## Compatibility policy

- Keep existing root build green while migrating.
- Move one subsystem at a time and keep public API names stable.
- Add thin wrappers only when needed to avoid large breakage.
