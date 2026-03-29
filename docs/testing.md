# Testing Guide

Panduan singkat untuk menjalankan test utama proyek SLM.

## 1) Full test runner (default)

Jalankan lint + suite fileops + full `ctest`:

```bash
scripts/test.sh
```

Dengan build dir custom:

```bash
scripts/test.sh /path/to/build-dir
```

## 2) Unlock security test flow (interactive password)

Untuk test unlock path yang butuh password user saat ini:

```bash
scripts/test.sh unlock
```

Mode ini akan:
- meminta password via `read -s` (tidak echo ke terminal),
- set `SLM_TEST_UNLOCK_PASSWORD` hanya untuk proses test,
- menjalankan `sessionstateservice_dbus_test` (default regex).

Dengan build dir + regex custom:

```bash
scripts/test.sh unlock build/toppanel-Debug sessionstateservice_dbus_test
```

Non-interaktif (mis. CI/dev env khusus):

```bash
SLM_TEST_UNLOCK_PASSWORD='your-password' \
scripts/test.sh unlock build/toppanel-Debug sessionstateservice_dbus_test
```

## 3) Direct helper unlock test

Jika ingin bypass wrapper `scripts/test.sh`:

```bash
scripts/test-session-unlock.sh --build
```

Help:

```bash
scripts/test-session-unlock.sh --help
```

## 4) Secret consent contract suite

Untuk regression check khusus consent/persistence `org.freedesktop.portal.Secret`:

```bash
scripts/test.sh secret-consent build/toppanel-Debug
scripts/test.sh policy-core build/toppanel-Debug

# Run baseline-flaky lane explicitly
scripts/test.sh baseline-flaky build/toppanel-Debug

# Run stable settings policy core lane
ctest --test-dir build/toppanel-Debug -L policy-core --output-on-failure
```

Fast path (tanpa configure/build ulang):

```bash
SLM_SECRET_CONSENT_SKIP_BUILD=1 \
scripts/test-secret-consent-suite.sh build/toppanel-Debug

SLM_POLICY_CORE_SKIP_BUILD=1 \
scripts/test-policy-core-suite.sh build/toppanel-Debug
```

Guard env penting:
- `SLM_SECRET_CONSENT_MIN_TESTS` (default `6`): fail jika jumlah test label berkurang.
- `SLM_SECRET_CONSENT_STRICT_NAMES` (default `1`): enforce exact expected test set.
- `SLM_SECRET_CONSENT_EXPECTED_TESTS`: override daftar expected test names (comma-separated).
- `SLM_POLICY_CORE_MIN_TESTS` (default `1`): fail jika jumlah test label `policy-core` berkurang.
- `SLM_POLICY_CORE_STRICT_NAMES` (default `1`): enforce exact expected test set untuk `policy-core`.
- `SLM_POLICY_CORE_EXPECTED_TESTS` (default `settingsapp_policy_core_test`): override daftar expected test names `policy-core`.
- `SLM_POLICY_CORE_SKIP_BUILD` (default `0`): skip configure/build untuk runner `policy-core`.
- `SLM_TEST_FULL_EXCLUDE_LABELS` (default `baseline-flaky`): label test yang di-exclude dari full suite `scripts/test.sh` default/nightly.
- `SLM_TEST_SKIP_UI_LINT` / `SLM_TEST_SKIP_CAPABILITY_MATRIX_LINT`: skip lint di runner full-suite jika lint sudah dijalankan di stage CI terpisah.
- `SLM_TEST_NIGHTLY_POLICY_CORE_MODE` (`required|auto|skip`, default `required`): kontrol lane `policy-core` saat menjalankan `scripts/test.sh nightly`.
- `SLM_TEST_NIGHTLY_POLICY_CORE_SKIP_BUILD` (`1|0`, default `1`): fast-path policy-core di nightly (lewati configure/build ulang).
- `SLM_TEST_NIGHTLY_FULL_EXCLUDE_LABELS`: exclude label khusus full-suite nightly (contoh CI: `baseline-flaky;secret-consent;policy-core` untuk hindari double-run).

## 5) Test yang relevan dengan lockscreen unlock hardening

- `sessionunlockthrottle_test`
  - Unit test helper throttle: escalation, decay, persistence, reset-on-success.
- `sessionstateservice_dbus_test`
  - DBus contract test untuk unlock:
    - auth fail non-rate-limited,
    - rate-limited reply dengan `retry_after_sec`,
    - escalation 5 fail,
    - optional success-path reset (aktif jika `SLM_TEST_UNLOCK_PASSWORD` valid).

## 6) Catatan environment

- Beberapa DBus test akan `SKIP` jika:
  - session bus tidak tersedia,
  - service name test tidak bisa diregister di environment saat itu.
- Ini disengaja agar test stabil lintas environment lokal/CI.
