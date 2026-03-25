#!/usr/bin/env bash
# install-envd-user-service.sh — install slm-envd as a systemd user service

set -euo pipefail

BINARY="${1:-$(command -v slm-envd 2>/dev/null || echo '')}"
UNIT_SRC="$(dirname "$0")/systemd/slm-envd.service"
UNIT_DIR="$HOME/.config/systemd/user"
BIN_DIR="$HOME/.local/bin"

if [[ -z "$BINARY" ]]; then
    echo "Usage: $0 <path-to-slm-envd>" >&2
    exit 1
fi

if [[ ! -f "$BINARY" ]]; then
    echo "Error: binary not found: $BINARY" >&2
    exit 1
fi

install -Dm755 "$BINARY" "$BIN_DIR/slm-envd"
echo "Installed binary: $BIN_DIR/slm-envd"

mkdir -p "$UNIT_DIR"
cp "$UNIT_SRC" "$UNIT_DIR/slm-envd.service"
echo "Installed unit: $UNIT_DIR/slm-envd.service"

systemctl --user daemon-reload
systemctl --user enable --now slm-envd.service

echo "slm-envd service started."
