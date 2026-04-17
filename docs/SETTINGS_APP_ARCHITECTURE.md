# System Settings Architecture (SLM)

## Folder structure

```text
Qml/apps/settings/
  Main.qml
  Sidebar.qml
  ContentPanel.qml
  components/
    SettingCard.qml
    SettingGroup.qml
    SettingToggle.qml

src/apps/settings/
  main.cpp
  settingsapp.{h,cpp}
  moduleloader.{h,cpp}
  searchengine.{h,cpp}
  globalsearchprovider.{h,cpp}
  include/
    settingbinding.h
    gsettingsbinding.{h,cpp}
    networkmanagerbinding.{h,cpp}
    mockbinding.h
  modules/
    appearance/
    network/
    bluetooth/
    sound/
    display/
    power/
    keyboard/
    mouse/
    applications/
    about/
```

## Module contract

Each module directory contains:
- `metadata.json`
- `<Module>Page.qml`
- optional backend adapter files

`metadata.json` supports:
- `id`, `name`, `icon`, `group`, `description`, `keywords`, `main`
- `settings[]` with:
  - `id`, `label`, `description`, `keywords`, `type`, `backend_binding`

## Search contracts

### In-app search
- Implemented by `SearchEngine`.
- Indexes category/module metadata + `settings[]`.
- Produces grouped results and filtered sidebar modules.

Result shape:
- `id`, `type` (`module|setting`), `moduleId`, `settingId`
- `title`, `subtitle`, `icon`, `score`, `action`

### Global search provider
- DBus service: `org.slm.Settings.Search`
- DBus object: `/org/slm/Settings/Search`
- Interface: `org.slm.Settings.Search.v1`
- Methods:
  - `Query(text, options)` -> `[(id, metadata)]`
  - `Activate(id)`

`action` metadata carries deep link:
- `settings://<module>`
- `settings://<module>/<setting>`

## Navigation and deep links

`SettingsApp` provides:
- `openModule(id)`
- `openModuleSetting(moduleId, settingId)`
- `openDeepLink(url)`
- `back()` and `recentHistory`
- `breadcrumb` and `currentSettingId`

## UX capabilities implemented

- Sidebar grouped categories
- Detail panel with local search
- Command palette (`Ctrl+K`)
- Search result jump to module/setting
- Recent navigation stack
- Lazy module page loading via `Loader`

## Integration notes

- Backends are abstracted through `SettingBinding`.
- Current bindings:
  - `GSettingsBinding` (GIO-based)
  - `NetworkManagerBinding` (DBus)
  - `MockBinding` (testing/dev)
- Runtime requires GIO development headers for `GSettingsBinding`.
