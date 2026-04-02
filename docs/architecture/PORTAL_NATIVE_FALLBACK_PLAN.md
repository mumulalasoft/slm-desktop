# SLM Portal Native vs Fallback Rollout Plan

Dokumen ini mengunci keputusan implementasi backend `xdg-desktop-portal-slm` per milestone distro.

Legenda strategy:
- `Native`: diimplementasikan langsung oleh `slm-portald`/`impl portal bridge`.
- `Fallback`: didelegasikan ke backend portal lain (gtk/kde/wlr) lewat `portals.conf`.
- `Stub`: endpoint ada tapi balas `NotSupported` terstruktur.

## 1) Milestone Targets

- `MVP (sandbox basic usability)`: aplikasi Flatpak bisa buka/simpan file, open-uri, notifikasi, settings dasar, screenshot dasar.
- `Beta (desktop-ready daily use)`: tambah document mediation, trash, screencast session stabil, open-with mediated.
- `Production (hardened)`: policy/interop stabil lintas GTK/Qt/Electron, input/control portal matang.

## 2) Canonical Matrix

| Portal / Interface | MVP | Beta | Production | Rationale |
|---|---|---|---|---|
| FileChooser | Native | Native | Native | Core compatibility + UX signature desktop |
| OpenURI | Native | Native | Native | Core app launch flow |
| Settings / Appearance | Native | Native | Native | Toolkit adaptation & theme coherence |
| Notification | Native | Native | Native | Existing service already mature |
| Inhibit | Native | Native | Native | Power/session correctness |
| Screenshot | Native | Native | Native | Already integrated with shell capture flow |
| ScreenCast | Fallback | Native | Native | MVP avoid fragile stream stack; promote after session flow stabilizes |
| RemoteDesktop | Fallback | Fallback | Native (target) | High risk; depends on Screencast + input capture maturity |
| Documents | Fallback | Native | Native | Start simple, then tokenized store in SLM |
| Trash | Native | Native | Native | Reuse file operation daemon |
| OpenWith | Native | Native | Native | Already tied to SLM file manager flow |
| GlobalShortcuts | Stub | Native (limited) | Native | Needs conflict resolution + persistence policy |
| Clipboard (portal-facing) | Stub | Stub/limited preview | Native (conservative) | Privacy risk; expose after strict policy + UX mediation |
| InputCapture | Fallback | Stub | Native (target) | Requires compositor-level guarantees |
| Camera | Fallback | Fallback | Native/Fallback hybrid | Prefer PipeWire camera portal backend initially |
| Location | Fallback | Fallback | Native/Fallback hybrid | Often tied to geoclue stack |
| Account / PIM | Stub | Stub/metadata only | Native (phased) | Sensitive; metadata-first strategy |
| Background | Fallback | Native (basic) | Native | Needs integration with session manager |
| PowerProfileMonitor | Fallback | Native | Native | Low-medium complexity after settings daemon contract |
| NetworkMonitor | Fallback | Native | Native | Needs network service surface hardening |
| Wallpaper | Stub | Native | Native | Low compat value, desktop UX value |
| Email / Print | Fallback | Fallback | Native/Fallback hybrid | Leverage existing toolchain; low immediate value |
| GameMode | Stub | Stub | Fallback/Native optional | Optional niche |
| PermissionStore (shared infra) | Native | Native | Native | Foundational for request/session policy persistence |
| Request / Session (shared infra) | Native | Native | Native | Foundational contract |

## 3) portals.conf Strategy by Milestone

### MVP
- `default=gtk` (atau backend distro default) untuk portal yang belum siap.
- explicit override ke `slm` untuk:
  - `FileChooser`, `OpenURI`, `Settings`, `Notification`, `Inhibit`, `Screenshot`.

### Beta
- pindahkan `Documents`, `ScreenCast`, `OpenWith`, `Trash`, `GlobalShortcuts` (limited) ke `slm`.
- sisakan `RemoteDesktop`, `Camera`, `Location`, `InputCapture` pada fallback.

### Production
- `default=slm` untuk portal yang sudah certified.
- fallback targeted untuk portal yang memang lebih aman/kompat lewat stack eksternal (mis. Location via geoclue ecosystem).

## 4) Promotion / Demotion Criteria

- Promote `Fallback -> Native` jika:
  - DBus contract tests hijau,
  - interop tests (GTK/Qt/Electron Flatpak) lulus,
  - race/cancel/multi-session tests stabil,
  - policy + audit log terverifikasi.

- Demote `Native -> Fallback` sementara jika:
  - regressi UX mayor (dialog loop, permission mismatch),
  - crash/race pada request-session lifecycle,
  - leak capability lintas app/session.

## 5) Open Questions

- RemoteDesktop: apakah akan sepenuhnya native SLM atau hybrid dengan backend wlroots/KDE path untuk fase awal production?
- Camera: apakah SLM perlu camera broker sendiri atau tetap mengandalkan portal PipeWire camera ecosystem?
- Clipboard portal exposure: apakah cukup metadata/preview-only untuk third-party, atau tetap internal-only hingga policy matured?
