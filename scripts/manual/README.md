# Desktop Manual Debug Tools

Tools for desktop icon sync/add/render verification.

## 1) Smoke Checklist

```bash
scripts/manual/desktop_add_smoke_checklist.sh
```

Prints the end-to-end manual scenarios for:
- Launchpad add/drag to desktop
- Dock drag to desktop
- Arrange/Snap/Clean up actions
- Sync + persistence checks

## 2) Quick Check

```bash
scripts/manual/desktop_sync_quick_check.sh
scripts/manual/desktop_sync_quick_check.sh /path/to/shell.log
```

Shows:
- Desktop file count
- Slot-map location/count
- Recent `[desktop-sync]` and `[desktop-add]` lines (if log provided)

## 3) Health Doctor

```bash
scripts/manual/desktop_health_doctor.sh
scripts/manual/desktop_health_doctor.sh /path/to/shell.log
```

Returns PASS/WARN/FAIL with exit code:
- `0` PASS
- `1` WARN
- `2` FAIL

## 4) Trace Watch / Summary

```bash
# realtime
scripts/manual/desktop_log_watch.sh
scripts/manual/desktop_log_watch.sh /path/to/shell.log

# summary only
scripts/manual/desktop_log_watch.sh --summary
scripts/manual/desktop_log_watch.sh --summary /path/to/shell.log
```

Filters and summarizes:
- `[desktop-sync]`
- `[desktop-add]`
- `[desktop-render]`

## 5) Run All (Suite)

```bash
scripts/manual/desktop_debug_suite.sh
scripts/manual/desktop_debug_suite.sh /path/to/shell.log
```

Runs checklist + quick-check + doctor + summary in one command.
