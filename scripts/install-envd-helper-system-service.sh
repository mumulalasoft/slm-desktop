#!/usr/bin/env bash
# install-envd-helper-system-service.sh
# Install slm-envd-helper (system D-Bus activation + policy artifacts).
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "[install-envd-helper][FAIL] run as root (sudo)." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

DBUS_SERVICE_SRC="$ROOT_DIR/scripts/dbus/org.slm.EnvironmentHelper1.service"
DBUS_CONF_SRC="$ROOT_DIR/scripts/dbus/org.slm.EnvironmentHelper1.conf"
POLKIT_POLICY_SRC="$ROOT_DIR/scripts/polkit/org.slm.environment.policy"

DBUS_SERVICE_DST_DIR="/usr/share/dbus-1/system-services"
DBUS_CONF_DST_DIR="/etc/dbus-1/system.d"
POLKIT_POLICY_DST_DIR="/usr/share/polkit-1/actions"

HELPER_DST="/usr/libexec/slm-envd-helper"
SKIP_RELOAD=0

usage() {
  cat <<'EOF'
Usage:
  sudo scripts/install-envd-helper-system-service.sh [path-to-slm-envd-helper] [--skip-reload]

Examples:
  sudo scripts/install-envd-helper-system-service.sh
  sudo scripts/install-envd-helper-system-service.sh build/Desktop_Qt_6_10_2-Debug/slm-envd-helper
EOF
}

resolve_binary() {
  local explicit_path="$1"
  if [[ -n "$explicit_path" ]]; then
    printf '%s\n' "$explicit_path"
    return
  fi

  local candidates=(
    "$ROOT_DIR/build/Desktop_Qt_6_10_2-Debug/slm-envd-helper"
    "$ROOT_DIR/build/toppanel-Debug/slm-envd-helper"
    "$ROOT_DIR/build/slm-envd-helper"
  )
  local c
  for c in "${candidates[@]}"; do
    if [[ -x "$c" ]]; then
      printf '%s\n' "$c"
      return
    fi
  done

  if command -v slm-envd-helper >/dev/null 2>&1; then
    command -v slm-envd-helper
    return
  fi
  printf '\n'
}

EXPLICIT_BINARY=""
for arg in "$@"; do
  case "$arg" in
    --skip-reload)
      SKIP_RELOAD=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --*)
      echo "[install-envd-helper] unknown option: $arg" >&2
      usage
      exit 1
      ;;
    *)
      if [[ -z "$EXPLICIT_BINARY" ]]; then
        EXPLICIT_BINARY="$arg"
      else
        echo "[install-envd-helper] unexpected argument: $arg" >&2
        usage
        exit 1
      fi
      ;;
  esac
done

HELPER_BIN="$(resolve_binary "$EXPLICIT_BINARY")"
if [[ -z "$HELPER_BIN" ]]; then
  echo "[install-envd-helper][FAIL] cannot find slm-envd-helper binary" >&2
  echo "[install-envd-helper][HINT] build first: cmake --build build/Desktop_Qt_6_10_2-Debug --target slm-envd-helper -j8" >&2
  usage
  exit 1
fi
HELPER_BIN="$(readlink -f "$HELPER_BIN")"
if [[ ! -x "$HELPER_BIN" ]]; then
  echo "[install-envd-helper][FAIL] binary not executable: $HELPER_BIN" >&2
  exit 1
fi

install -Dm755 "$HELPER_BIN" "$HELPER_DST"
install -Dm644 "$DBUS_SERVICE_SRC" "$DBUS_SERVICE_DST_DIR/org.slm.EnvironmentHelper1.service"
install -Dm644 "$DBUS_CONF_SRC" "$DBUS_CONF_DST_DIR/org.slm.EnvironmentHelper1.conf"
install -Dm644 "$POLKIT_POLICY_SRC" "$POLKIT_POLICY_DST_DIR/org.slm.environment.policy"

echo "[install-envd-helper] installed binary: $HELPER_DST"
echo "[install-envd-helper] installed: $DBUS_SERVICE_DST_DIR/org.slm.EnvironmentHelper1.service"
echo "[install-envd-helper] installed: $DBUS_CONF_DST_DIR/org.slm.EnvironmentHelper1.conf"
echo "[install-envd-helper] installed: $POLKIT_POLICY_DST_DIR/org.slm.environment.policy"

if [[ "$SKIP_RELOAD" != "1" ]]; then
  systemctl reload dbus.service >/dev/null 2>&1 || systemctl restart dbus.service >/dev/null 2>&1 || true
  systemctl reload polkit.service >/dev/null 2>&1 || systemctl restart polkit.service >/dev/null 2>&1 || true
  echo "[install-envd-helper] reloaded dbus/polkit (best effort)"
else
  echo "[install-envd-helper] skipped dbus/polkit reload"
fi

echo "[install-envd-helper] done"
