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
- `org.freedesktop.portal.Secret.xml` (draft portal DBus spec)
- `portal-secret-consent.md` (consent/persistence behavior contract + test mapping)

Secret consent contract suite:
- Label: `secret-consent`
- Build: `cmake --build build/toppanel-Debug --target secret_consent_test_suite -j8`
- Run: `ctest --test-dir build/toppanel-Debug -L secret-consent --output-on-failure`
- One-shot runner: `scripts/test-secret-consent-suite.sh build/toppanel-Debug`
- Test-only fast path: `SLM_SECRET_CONSENT_SKIP_BUILD=1 scripts/test-secret-consent-suite.sh build/toppanel-Debug`
- Guard env: `SLM_SECRET_CONSENT_MIN_TESTS` (default: `6`, fail if labeled test count is lower)
- Guard env: `SLM_SECRET_CONSENT_STRICT_NAMES=1|0` (default: `1`)
- Guard env: `SLM_SECRET_CONSENT_EXPECTED_TESTS` (comma-separated expected test names for strict mode)

Coverage map:
- `portal_secret_integration_test`
  - persistent matrix (`Store/Get/Delete` persist, `Clear` non-persist)
  - reprompt behavior for non-persistent `ClearAppSecrets`
  - consent payload matrix for `persistentEligible`
- `portal_consent_dialog_copy_contract_test`
  - microcopy + deep-link + `Always Allow` guard contract
- `portal_consent_dialog_behavior_test`
  - runtime `Always Allow` enablement + non-persistent visibility guard
- `portal_dialog_bridge_payload_test`
  - `PortalDialogBridge` payload field contract and callback resolution

Template for new contract files:
1. Service identity (DBus name/path/interface or API ID).
2. Stable methods/signals/properties and required fields.
3. Error model and degraded/offline behavior.
4. Compatibility notes (since/until/deprecation plan).
5. Test coverage references (provider + consumer).
