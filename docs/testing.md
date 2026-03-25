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

## 4) Test yang relevan dengan lockscreen unlock hardening

- `sessionunlockthrottle_test`
  - Unit test helper throttle: escalation, decay, persistence, reset-on-success.
- `sessionstateservice_dbus_test`
  - DBus contract test untuk unlock:
    - auth fail non-rate-limited,
    - rate-limited reply dengan `retry_after_sec`,
    - escalation 5 fail,
    - optional success-path reset (aktif jika `SLM_TEST_UNLOCK_PASSWORD` valid).

## 5) Catatan environment

- Beberapa DBus test akan `SKIP` jika:
  - session bus tidak tersedia,
  - service name test tidak bisa diregister di environment saat itu.
- Ini disengaja agar test stabil lintas environment lokal/CI.

