#!/bin/sh

set -eu


cp -rf /usr/local/bin/slm-shell /usr/local/bin/slm-shell.real


cat > /usr/local/bin/slm-shell <<'EOF'
#!/bin/sh
exec env WAYLAND_DEBUG=client SLM_DISABLE_APPDECK=1 /usr/local/bin/slm-shell.real "$@"
EOF
chmod +x /usr/local/bin/slm-shell
