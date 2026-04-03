# Portal Secret Consent Contract

Scope:
- Consent mediation behavior for `org.freedesktop.portal.Secret` request methods.
- Stability baseline for policy-to-UI payload and persistence behavior.

Service identity:
- Provider: `PortalBackendService` + `PortalDialogBridge`
- Portal methods in scope:
  - `StoreSecret`
  - `GetSecret`
  - `DeleteSecret`
  - `ClearAppSecrets`

Contract invariants:
1. Consent payload capability mapping:
   - `StoreSecret` -> `Secret.Store`
   - `GetSecret` -> `Secret.Read`
   - `DeleteSecret` -> `Secret.Delete`
   - `ClearAppSecrets` -> `Secret.Delete`
2. `persistentEligible` payload policy:
   - `StoreSecret`, `GetSecret`, `DeleteSecret` => `true`
   - `ClearAppSecrets` => `false`
3. Persistence behavior:
   - For `Store/Get/Delete`, `allow_always` (or `deny_always`) may persist only when policy/spec allows.
   - For `ClearAppSecrets`, decisions must not persist even if UI returns `persist=true`.
4. Re-prompt behavior:
   - `ClearAppSecrets` must continue to request consent on subsequent calls (no stored auto-allow/auto-deny).
5. UI guard behavior:
   - When `persistentEligible=false`, `Always Allow` control is hidden.
   - For `Secret.Delete` with persistent consent allowed, `Always Allow` remains gated by explicit confirm checkbox.

Error/degraded behavior:
- If consent UI bridge is unavailable, requests fail closed (deny/cancel path).
- This contract does not guarantee specific user-facing copy beyond copy-contract tests.

Compatibility policy:
- Additive payload fields are allowed.
- Existing payload keys used by tests (`capability`, `persistentEligible`) are stable.
- Any behavioral change in invariants above requires:
  - contract doc update
  - test updates
  - changelog note for consumers

Test coverage references:
- `portal_secret_integration_test`
  - `storeGetDescribeDelete_roundtrip`
  - `clearAppSecrets_consentAlwaysAllowDoesNotPersist_contract`
  - `deleteSecret_consentAlwaysAllowPersists_contract`
  - `getSecret_consentAlwaysAllowPersists_contract`
  - `storeSecret_consentAlwaysAllowPersists_contract`
  - `clearAppSecrets_denyAlwaysDoesNotPersistAndReprompts_contract`
  - `secretConsentPayload_persistentEligible_matrix_contract`
- `portal_consent_dialog_copy_contract_test`
- `portal_consent_dialog_behavior_test`
- `portal_dialog_bridge_payload_test`
- `settingsapp_secret_consent_test` (non-DBus settings-side contract for summary/revoke/clear flow)

Suite execution:
- Build: `cmake --build build/toppanel-Debug --target secret_consent_test_suite -j8`
- Run: `ctest --test-dir build/toppanel-Debug -L secret-consent --output-on-failure`
- Unified runner: `scripts/test.sh secret-consent build/toppanel-Debug`
- Test-only fast path: `SLM_SECRET_CONSENT_SKIP_BUILD=1 scripts/test-secret-consent-suite.sh build/toppanel-Debug`
- Runner guard: `SLM_SECRET_CONSENT_MIN_TESTS` (default `6`) to detect label/registration drift
- Runner guard: `SLM_SECRET_CONSENT_STRICT_NAMES` (default `1`) to enforce exact expected test set
- Runner guard: `SLM_SECRET_CONSENT_EXPECTED_TESTS` (comma-separated expected test names)
