# SLM Desktop Session State

Last updated: 2026-03-29 (WIB)

## Scope (Current)

- Focus utama: desktop stabilization + unbreakable policy.
- Stream aktif:
  - FileManager archive service integration
  - package policy protection layer
  - runtime/service hardening and smoke coverage
- Login stack tetap dijaga di jalur maintenance, bukan fokus perubahan besar harian.

## What Was Completed Recently

1. Desktop target standardization:
   - build/runtime script dan CI diselaraskan ke target `slm-desktop`.
   - legacy alias masih ditoleransi sebagai fallback di beberapa script runtime.
2. Archive service baseline:
   - daemon `slm-archived` + D-Bus API aktif.
   - libarchive backend untuk list/extract/compress.
   - default UX: double-click extract, preview read-only, menu konteks minimal.
   - progress/cancel terhubung ke indicator FileManager.
3. Package policy phase-1 baseline:
   - simulator apt + engine block protected capability removal/replacement/downgrade.
   - wrappers `apt/apt-get/dpkg` + logging + snapshot/recovery hooks.
4. Missing component policy:
   - global missing-component controller + severity (`required/recommended`) dan gate API.
   - flow install dependency via polkit pipeline sudah tersedia di level contract/UI.
5. Style migration:
   - style shared dipindah ke `third_party/slm-style`.
   - direktori legacy `Style/` di repo utama sudah tidak dipakai lagi.

## Current Priorities

1. Hardening archive security/resource limits + UX threshold progress.
2. Package policy phase-2 (source trust classification + risk messaging).
3. Stabilization test lanes:
   - promote runtime smoke lanes ke gate lebih ketat bertahap.
4. Release prep:
   - continue repo split boundary cleanup and contract alignment.

## Guardrails

- Semua aksi berisiko (package/remove/recovery) wajib lewat policy + simulation path.
- Jangan tambahkan logic archive ad-hoc di QML; tetap lewat `slm-archived`.
- Pertahankan fallback session/login path yang sudah terbukti aman.

## Next Session Bootstrap

Read in order:
1. `docs/SESSION_STATE.md`
2. `docs/TODO.md`
3. `docs/PROJECT_STRUCTURE.md`
4. Task-specific architecture doc:
   - archive: `docs/architecture/ARCHIVE_SERVICE_ARCHITECTURE.md`
   - package policy: `docs/architecture/SLM_PACKAGE_POLICY_PHASE1.md`
