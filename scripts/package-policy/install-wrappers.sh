#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-/usr/local/lib/slm-package-policy}"
BIN_DIR="${2:-/usr/local/bin}"
WRAPPER_DIR="$ROOT_DIR/wrappers"
PROFILE_SCRIPT_PATH=""

if [[ $# -ge 3 ]]; then
  shift 2
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --install-profile-script)
        PROFILE_SCRIPT_PATH="${2:-}"
        if [[ -z "$PROFILE_SCRIPT_PATH" ]]; then
          echo "install-wrappers: --install-profile-script requires path argument" >&2
          exit 2
        fi
        shift 2
        ;;
      *)
        echo "install-wrappers: unknown argument: $1" >&2
        exit 2
        ;;
    esac
  done
fi

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

if [[ -n "$PROFILE_SCRIPT_PATH" ]]; then
  mkdir -p "$(dirname "$PROFILE_SCRIPT_PATH")"
  cat > "$PROFILE_SCRIPT_PATH" <<EOF
#!/usr/bin/env sh
# Installed by scripts/package-policy/install-wrappers.sh
case ":\$PATH:" in
  *:"$BIN_DIR":*) ;;
  *) export PATH="$BIN_DIR:\$PATH" ;;
esac
EOF
  chmod 0644 "$PROFILE_SCRIPT_PATH"
fi

check_wrapper_priority() {
  local tool="$1"
  local expected="$BIN_DIR/$tool"
  local resolved
  resolved="$(command -v "$tool" 2>/dev/null || true)"
  if [[ "$resolved" == "$expected" ]]; then
    echo "  [ok] $tool -> $resolved"
    return 0
  fi
  echo "  [warn] $tool resolves to: ${resolved:-<not found>} (expected $expected)" >&2
  return 1
}

priority_ok=1
check_wrapper_priority "apt" || priority_ok=0
check_wrapper_priority "apt-get" || priority_ok=0
check_wrapper_priority "dpkg" || priority_ok=0

echo "Installed SLM package-policy wrappers"
echo "  root dir:    $ROOT_DIR"
echo "  wrapper dir: $WRAPPER_DIR"
echo "  bin dir:     $BIN_DIR"
if [[ -n "$PROFILE_SCRIPT_PATH" ]]; then
  echo "  profile:     $PROFILE_SCRIPT_PATH"
fi
if [[ "$priority_ok" -eq 1 ]]; then
  echo "PATH priority check: OK"
else
  echo "PATH priority check: WARN (ensure $BIN_DIR comes before /usr/bin in PATH)." >&2
fi
