# SLM Installer

Calamares-based system installer for SLM Desktop. Scaffolding only — modules contain no logic yet.

## Documents

- `docs/SLM_INSTALLER.md` — binding spec
- `docs/SLM_INSTALLER_BACKEND.md` — backend architecture (this tree)
- `docs/SLM_INSTALLER_DESIGN.md` — UX/UI plan (Calamares branding + slm-welcome OOBE)

## Layout

```
src/installer/
├── CMakeLists.txt              gated by SLM_BUILD_INSTALLER (default OFF)
├── settings.conf               Calamares pipeline ordering
├── branding/slm/               branding.desc + show.qml
├── shared/                     SlmErrorTranslator and other helpers
└── calamares-modules/          11 C++ job plugins (one per phase)
```

## Building

```bash
cmake -B build -DSLM_BUILD_INSTALLER=ON
cmake --build build
```

Requires `calamares-dev` (>= 3.3) on the build host.
