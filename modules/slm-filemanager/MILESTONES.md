# Milestones

## M1 Stabilize v0.1.x
- Duration: 2 weeks
- Target tag: `v0.1.0`

Checklist:
- [ ] CI: standalone configure/build/install green.
- [ ] CI: package-configure smoke (`find_package`) green.
- [ ] CI: DBus stable gate green.
- [ ] CI: strict recovery gate (nightly/manual) green.
- [ ] Freeze public headers and DBus contract notes.
- [ ] Publish `CHANGELOG.md`.
- [ ] Release `v0.1.0`.

## M2 Desktop Integration Pinning
- Duration: 1-2 weeks
- Depends on: M1

Checklist:
- [ ] Desktop shell pinned to `v0.1.0`.
- [ ] Shell compatibility CI against pinned tag green.
- [ ] External package mode green in shell.
- [ ] Compatibility matrix updated.

## M3 Ownership Split
- Duration: 1 week
- Depends on: M2

Checklist:
- [ ] CODEOWNERS and review policy active.
- [ ] Issue/PR ownership moved to standalone repo.
- [ ] Monorepo reduced to bridge + contract tests.

## M4 Hardening & Packaging
- Duration: 2-3 weeks
- Depends on: M3

Checklist:
- [ ] Extended regression suite green.
- [ ] Performance baseline captured.
- [ ] Packaging/release artifact flow documented.
- [ ] Patch release cadence established.

## M5 Deprecation & Cleanup
- Window: after one stable cycle
- Depends on: M4 + downstream confirmation

Checklist:
- [ ] Legacy QML alias shims removed.
- [ ] Deprecated header shims removed.
- [ ] Migration docs finalized.
- [ ] Release `v0.2.0` if breaking changes applied.
