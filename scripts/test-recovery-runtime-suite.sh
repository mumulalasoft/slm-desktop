#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(mktemp -d)"

cleanup() {
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT

log() { echo "[test-recovery-runtime] $*"; }
warn() { echo "[test-recovery-runtime][WARN] $*" >&2; }
fail() { echo "[test-recovery-runtime][FAIL] $*" >&2; exit 1; }

require_file() {
  local path="$1"
  [[ -f "$path" ]] || fail "missing file: $path"
}

require_exec() {
  local path="$1"
  [[ -x "$path" ]] || fail "missing executable file: $path"
}

SCRIPT_FILES=(
  "$ROOT_DIR/scripts/recovery/recoveryctl.sh"
  "$ROOT_DIR/scripts/recovery/bootcount-guard.sh"
  "$ROOT_DIR/scripts/recovery/trigger-recovery-on-boot-failure.sh"
  "$ROOT_DIR/scripts/recovery/restore-snapshot.sh"
  "$ROOT_DIR/scripts/recovery/repair-boot.sh"
  "$ROOT_DIR/scripts/recovery/reinstall-system.sh"
  "$ROOT_DIR/scripts/recovery/prune-btrfs-snapshots.sh"
  "$ROOT_DIR/scripts/recovery/install-system-recovery-services.sh"
  "$ROOT_DIR/scripts/recovery/install-grub-boot-failure-integration.sh"
  "$ROOT_DIR/scripts/package-policy/pre-transaction-snapshot.sh"
  "$ROOT_DIR/scripts/package-policy/post-transaction-health-check.sh"
  "$ROOT_DIR/scripts/package-policy/post-update-verify.sh"
)

UNIT_FILES=(
  "$ROOT_DIR/scripts/systemd/system/slm-boot-attempt.service"
  "$ROOT_DIR/scripts/systemd/system/slm-boot-failure-check.service"
  "$ROOT_DIR/scripts/systemd/system/boot-success.service"
  "$ROOT_DIR/scripts/systemd/system/slm-snapshot-retention.service"
  "$ROOT_DIR/scripts/systemd/system/slm-snapshot-retention.timer"
  "$ROOT_DIR/scripts/systemd/system/slm-post-update-verify.service"
  "$ROOT_DIR/scripts/systemd/system/slm-post-update-verify.path"
)

log "validate required scripts"
for s in "${SCRIPT_FILES[@]}"; do
  require_exec "$s"
done

log "validate required systemd unit files"
for u in "${UNIT_FILES[@]}"; do
  require_file "$u"
done

log "bash -n syntax validation"
bash -n "${SCRIPT_FILES[@]}"

if command -v shellcheck >/dev/null 2>&1; then
  log "shellcheck available, running targeted lint"
  shellcheck -x "${SCRIPT_FILES[@]}"
else
  warn "shellcheck not found, skip lint"
fi

log "recoveryctl command surface smoke"
set +e
"$ROOT_DIR/scripts/recovery/recoveryctl.sh" >/dev/null 2>&1
rc_usage=$?
set -e
[[ "$rc_usage" -eq 2 ]] || fail "recoveryctl no-arg expected exit=2, got $rc_usage"

"$ROOT_DIR/scripts/recovery/recoveryctl.sh" snapshot list >/dev/null

set +e
"$ROOT_DIR/scripts/recovery/recoveryctl.sh" invalid-cmd >/dev/null 2>&1
rc_invalid=$?
set -e
[[ "$rc_invalid" -eq 2 ]] || fail "recoveryctl invalid-cmd expected exit=2, got $rc_invalid"

log "post-update verify script smoke"
SLM_PACKAGE_POLICY_STATE_DIR="$WORK_DIR/state" \
SLM_AUTO_PRUNE_AFTER_VERIFY=0 \
"$ROOT_DIR/scripts/package-policy/post-update-verify.sh" --snapshot-id smoke >/dev/null

if command -v systemd-analyze >/dev/null 2>&1; then
  log "systemd unit verify with local fixture rewrites"
  UNIT_FIXTURE_DIR="$WORK_DIR/unit-fixture"
  mkdir -p "$UNIT_FIXTURE_DIR"

  for u in "${UNIT_FILES[@]}"; do
    cp "$u" "$UNIT_FIXTURE_DIR/$(basename "$u")"
  done

  # Replace runtime-specific paths so verify checks unit syntax only.
  sed -i 's#^ExecStart=.*#ExecStart=/bin/true#g' "$UNIT_FIXTURE_DIR"/*.service
  systemd-analyze verify "$UNIT_FIXTURE_DIR"/*.service "$UNIT_FIXTURE_DIR"/*.timer "$UNIT_FIXTURE_DIR"/*.path >/dev/null
else
  warn "systemd-analyze not found, skip unit verify"
fi

if [[ "${EUID}" -eq 0 ]]; then
  log "root-only functional smoke for bootcount guard"
  STUB_BIN_DIR="$WORK_DIR/stubbin"
  mkdir -p "$STUB_BIN_DIR"

  cat > "$STUB_BIN_DIR/request-recovery.sh" <<'STUB'
#!/usr/bin/env bash
exit 0
STUB
  chmod +x "$STUB_BIN_DIR/request-recovery.sh"

  cat > "$STUB_BIN_DIR/restore-snapshot.sh" <<'STUB'
#!/usr/bin/env bash
exit 0
STUB
  chmod +x "$STUB_BIN_DIR/restore-snapshot.sh"

  BOOTCOUNT_FILE="$WORK_DIR/bootcount"
  LAST_GOOD_FILE="$WORK_DIR/last_good_snapshot"
  RECOVERY_ATTEMPTS_FILE="$WORK_DIR/recovery_attempts"
  RECOVERY_LOCK_FILE="$WORK_DIR/recovery_auto.lock"
  AUTO_DISABLE_FILE="$WORK_DIR/recovery.auto.disable"

  SLM_BOOTCOUNT_FILE="$BOOTCOUNT_FILE" \
  SLM_LAST_GOOD_FILE="$LAST_GOOD_FILE" \
  SLM_RECOVERY_ATTEMPTS_FILE="$RECOVERY_ATTEMPTS_FILE" \
  SLM_RECOVERY_LOCK_FILE="$RECOVERY_LOCK_FILE" \
  SLM_RECOVERY_AUTO_DISABLE_FILE="$AUTO_DISABLE_FILE" \
  SLM_REQUEST_RECOVERY_SCRIPT="$STUB_BIN_DIR/request-recovery.sh" \
  SLM_RESTORE_SNAPSHOT_SCRIPT="$STUB_BIN_DIR/restore-snapshot.sh" \
  SLM_GRUBENV_PATH="$WORK_DIR/grubenv" \
  SLM_BOOTCOUNT_THRESHOLD=3 \
  "$ROOT_DIR/scripts/recovery/bootcount-guard.sh" increment

  SLM_BOOTCOUNT_FILE="$BOOTCOUNT_FILE" \
  SLM_LAST_GOOD_FILE="$LAST_GOOD_FILE" \
  SLM_RECOVERY_ATTEMPTS_FILE="$RECOVERY_ATTEMPTS_FILE" \
  SLM_RECOVERY_LOCK_FILE="$RECOVERY_LOCK_FILE" \
  SLM_RECOVERY_AUTO_DISABLE_FILE="$AUTO_DISABLE_FILE" \
  SLM_REQUEST_RECOVERY_SCRIPT="$STUB_BIN_DIR/request-recovery.sh" \
  SLM_RESTORE_SNAPSHOT_SCRIPT="$STUB_BIN_DIR/restore-snapshot.sh" \
  SLM_GRUBENV_PATH="$WORK_DIR/grubenv" \
  SLM_BOOTCOUNT_THRESHOLD=3 \
  "$ROOT_DIR/scripts/recovery/bootcount-guard.sh" success smoke-snapshot

  [[ "$(cat "$BOOTCOUNT_FILE")" == "0" ]] || fail "bootcount reset failed"
else
  warn "not running as root, skip root-only bootcount functional smoke"
fi

log "done"
