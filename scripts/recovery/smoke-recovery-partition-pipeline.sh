#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK_DIR="$(mktemp -d)"
BUILD_DIR="$WORK_DIR/build"
OUT_DIR="$WORK_DIR/out"
FIXTURE_DIR="$WORK_DIR/fixture"

cleanup() {
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT

log() { echo "[smoke-recovery-pipeline] $*"; }
warn() { echo "[smoke-recovery-pipeline][WARN] $*" >&2; }
fail() { echo "[smoke-recovery-pipeline][FAIL] $*" >&2; exit 1; }

make_stub_bin() {
  local path="$1"
  cat > "$path" <<'EOF'
#!/usr/bin/env bash
echo "stub $(basename "$0")"
EOF
  chmod +x "$path"
}

mkdir -p "$BUILD_DIR" "$OUT_DIR" "$FIXTURE_DIR"/{loader,grub}

for bin in slm-recovery-app slm-recoveryd; do
  make_stub_bin "$BUILD_DIR/$bin"
done

DETECT_SCRIPT="$ROOT_DIR/scripts/recovery/detect-recovery-boot-entry.sh"
BUILD_SCRIPT="$ROOT_DIR/scripts/recovery/build-recovery-partition-image.sh"
INSTALL_SCRIPT="$ROOT_DIR/scripts/recovery/install-recovery-boot-entry.sh"
DEPLOY_SCRIPT="$ROOT_DIR/scripts/recovery/deploy-recovery-partition.sh"

for s in "$DETECT_SCRIPT" "$BUILD_SCRIPT" "$INSTALL_SCRIPT" "$DEPLOY_SCRIPT"; do
  [[ -x "$s" ]] || fail "missing executable script: $s"
done

log "check script help output"
"$BUILD_SCRIPT" --help >/dev/null
"$INSTALL_SCRIPT" --help >/dev/null
"$DEPLOY_SCRIPT" --help >/dev/null

log "check detector with fixture systemd-boot entries"
cat > "$FIXTURE_DIR/loader/arch.conf" <<'EOF'
title Arch Linux
linux /vmlinuz-linux
EOF
cat > "$FIXTURE_DIR/loader/slm-recovery.conf" <<'EOF'
title SLM Recovery
linux /vmlinuz-linux
EOF
detected="$(
  SLM_RECOVERY_LOADER_ENTRY_DIRS="$FIXTURE_DIR/loader" \
  SLM_RECOVERY_GRUB_CFG_PATHS="$FIXTURE_DIR/grub/grub.cfg" \
  SLM_RECOVERY_BOOTLOADER_HINT=systemd-boot \
  "$DETECT_SCRIPT" recovery
)"
[[ "$detected" == "slm-recovery" ]] || fail "unexpected detected entry: $detected"

if ! command -v mksquashfs >/dev/null 2>&1; then
  warn "mksquashfs not found, skipping image build stage"
  log "done (partial)"
  exit 0
fi

log "build recovery partition artifact"
"$BUILD_SCRIPT" \
  --build-dir "$BUILD_DIR" \
  --out-dir "$OUT_DIR" \
  --root-spec "PARTUUID=DEADBEEF" \
  --image-name "recovery-test.squashfs"

[[ -f "$OUT_DIR/recovery-test.squashfs" ]] || fail "missing image output"
[[ -f "$OUT_DIR/recovery-partition.manifest.json" ]] || fail "missing manifest output"
[[ -f "$OUT_DIR/slm-recovery-systemd-boot.conf.template" ]] || fail "missing systemd-boot template"
[[ -f "$OUT_DIR/slm-recovery-grub.cfg.template" ]] || fail "missing grub template"

grep -q '"format": "slm-recovery-partition/v1"' "$OUT_DIR/recovery-partition.manifest.json" \
  || fail "manifest format mismatch"
grep -q 'root=PARTUUID=DEADBEEF' "$OUT_DIR/slm-recovery-systemd-boot.conf.template" \
  || fail "systemd-boot template root spec mismatch"
grep -q 'root=PARTUUID=DEADBEEF' "$OUT_DIR/slm-recovery-grub.cfg.template" \
  || fail "grub template root spec mismatch"

log "done"
