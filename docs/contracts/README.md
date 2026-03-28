# SLM Contract Baseline

Purpose:
- Keep cross-component behavior stable during release hardening and repo split.
- Make provider/consumer expectations explicit and testable.

Rules:
- Every externally consumed service must have a contract doc in this folder.
- Contract updates must be accompanied by:
  - provider test updates
  - consumer compatibility check updates
  - changelog note when behavior changes
- Backward compatibility:
  - additive fields/methods are preferred
  - removals/renames require one release-cycle deprecation period

Current baseline contracts:
- `workspacemanager.md`
- `missingcomponents.md`
- `polkit-agent.md`

Template for new contract files:
1. Service identity (DBus name/path/interface or API ID).
2. Stable methods/signals/properties and required fields.
3. Error model and degraded/offline behavior.
4. Compatibility notes (since/until/deprecation plan).
5. Test coverage references (provider + consumer).
