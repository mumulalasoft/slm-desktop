#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-/usr/local/lib/slm-package-policy}"
BIN_DIR="${2:-/usr/local/bin}"
WRAPPER_DIR="$ROOT_DIR/wrappers"

mkdir -p "$ROOT_DIR" "$WRAPPER_DIR" "$BIN_DIR"
install -m 0755 "$(dirname "$0")/pre-transaction-snapshot.sh" "$ROOT_DIR/pre-transaction-snapshot.sh"
install -m 0755 "$(dirname "$0")/post-transaction-health-check.sh" "$ROOT_DIR/post-transaction-health-check.sh"
install -m 0755 "$(dirname "$0")/recover-last-snapshot.sh" "$ROOT_DIR/recover-last-snapshot.sh"
install -m 0755 "$(dirname "$0")/disable-external-repos.sh" "$ROOT_DIR/disable-external-repos.sh"
install -m 0755 "$(dirname "$0")/trigger-safe-mode-recovery.sh" "$ROOT_DIR/trigger-safe-mode-recovery.sh"
install -m 0755 "$(dirname "$0")/wrappers/apt" "$WRAPPER_DIR/apt"
install -m 0755 "$(dirname "$0")/wrappers/apt-get" "$WRAPPER_DIR/apt-get"
install -m 0755 "$(dirname "$0")/wrappers/dpkg" "$WRAPPER_DIR/dpkg"

ln -sf "$WRAPPER_DIR/apt" "$BIN_DIR/apt"
ln -sf "$WRAPPER_DIR/apt-get" "$BIN_DIR/apt-get"
ln -sf "$WRAPPER_DIR/dpkg" "$BIN_DIR/dpkg"

echo "Installed SLM package-policy wrappers"
echo "  root dir:    $ROOT_DIR"
echo "  wrapper dir: $WRAPPER_DIR"
echo "  bin dir:     $BIN_DIR"
echo "Ensure $BIN_DIR comes before /usr/bin in PATH."
