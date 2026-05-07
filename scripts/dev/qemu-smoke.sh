#!/usr/bin/env bash
# scripts/dev/qemu-smoke.sh — Fast build/install/smoke SLM di QEMU guest.
#
# Default path dibuat cepat untuk iterasi login: build target minimum di host,
# rsync binary saja, install runtime sempit di guest, lalu smoke.
# Pakai --full untuk pipeline lama: mount hostshare, sync build tree penuh,
# install runtime penuh, dan verify penuh.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=./qemu-common.sh
source "$SCRIPT_DIR/qemu-common.sh"

STATE_DIR="$(qemu_dev_state_dir)"
SSH_USER="${SLM_QEMU_SSH_USER:-garis}"
SSH_PORT="${SLM_QEMU_SSH_PORT:-2222}"
SSH_IDENTITY_FILE="${SLM_QEMU_SSH_IDENTITY_FILE:-$STATE_DIR/id_ed25519}"
SSH_PASSWORD="${SLM_QEMU_SSH_PASSWORD:-}"
SESSION_USER="${SLM_QEMU_SESSION_USER:-$SSH_USER}"
REPO_DIR="${SLM_QEMU_GUEST_REPO_DIR:-/mnt/hostshare}"
BUILD_DIR="${SLM_QEMU_GUEST_BUILD_DIR:-/home/${SSH_USER}/.cache/slm-qemu/build/dev}"
JOBS="${SLM_QEMU_GUEST_JOBS:-$(nproc)}"
RUN_SMOKE=1
SMOKE_TIMEOUT="${SLM_QEMU_SESSION_SMOKE_TIMEOUT_SEC:-90}"
BUILD_ONLY="${SLM_QEMU_SMOKE_BUILD_ONLY:-auto}"
SKIP_BUILD=0
STRICT_PROCESS=0
FAST_MODE="${SLM_QEMU_SMOKE_FAST:-1}"
HOST_ARTIFACT_ROOT="${SLM_QEMU_SESSION_SMOKE_ARTIFACT_ROOT:-$PWD/artifacts/qemu-session-smoke}"
HOST_REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOST_CMAKE_PREFIX_PATH="${SLM_QEMU_HOST_CMAKE_PREFIX_PATH:-/usr}"
if [[ -n "${SLM_QEMU_HOST_BUILD_DIR:-}" ]]; then
    HOST_BUILD_DIR="$SLM_QEMU_HOST_BUILD_DIR"
elif [[ -d "$HOST_REPO_DIR/build/icu78" ]]; then
    HOST_BUILD_DIR="$HOST_REPO_DIR/build/icu78"
else
    HOST_BUILD_DIR="$HOST_REPO_DIR/build/qemu-smoke"
fi

FAST_TARGETS=(
    slm-greeter
    slm-watchdog
    slm-recovery-app
    slm-session-broker
    slm-desktop
)

FULL_TARGETS=(
    # greeter → broker pipeline (wajib ada, must_install_bin)
    slm-greeter slm-watchdog slm-recovery-app slm-session-broker
    # desktop shell & compositor daemons
    slm-desktop desktopd slm-svcmgrd slm-loggerd
    # portals & system daemons
    slm-portald slm-fileopsd slm-devicesd slm-clipboardd
    slm-envd slm-envd-helper slm-recoveryd slm-polkit-agent
    # settings & context
    slm-settingsd desktop-contextd slm-settings
    # control utilities
    indicatorctl windowingctl workspacectl fileopctl devicectl globalmenuctl
)

RUNTIME_TARGETS=("${FAST_TARGETS[@]}")

usage() {
    cat <<EOF
Usage: bash scripts/dev/qemu-smoke.sh [options]

Options:
  --user USER           SSH user. Default: $SSH_USER
  --port PORT           SSH port. Default: $SSH_PORT
  --identity-file PATH  SSH private key. Default: $SSH_IDENTITY_FILE if present
  --password PASSWORD   SSH password. Uses sshpass when available; otherwise
                        falls back to an interactive SSH prompt when /dev/tty exists.
                        Default: SLM_QEMU_SSH_PASSWORD
  --session-user USER   Desktop session user. Default: $SESSION_USER
  --repo-dir PATH       Repo path in guest. Default: $REPO_DIR
  --build-dir PATH      Build dir in guest (rsync target). Default: $BUILD_DIR
  --host-build-dir PATH Build dir on host. Default: build/icu78 if present,
                        otherwise build/qemu-smoke
  --cmake-prefix PATH   Host CMAKE_PREFIX_PATH. Default: $HOST_CMAKE_PREFIX_PATH
  --jobs N              Parallel build jobs. Default: $JOBS
  --timeout SEC         Smoke wait timeout. Default: $SMOKE_TIMEOUT
  --artifact-root PATH  Host artifact dir. Default: $HOST_ARTIFACT_ROOT
  --fast                Fast path: target minimum + install sempit. Default.
  --full                Pipeline penuh lama: mount + install/verify script.
  --configure           Force cmake configure before build
  --build-only          Skip configure, only cmake --build. Default when
                        host build dir already has CMakeCache.txt.
  --skip-build          Do not build, only sync/install/smoke existing artifacts
  --skip-smoke          Build + install + verify only
  --strict-process      Smoke fails if oracle processes not visible
  --help                Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)           SSH_USER="$2";           shift 2 ;;
        --port)           SSH_PORT="$2";           shift 2 ;;
        --identity-file)  SSH_IDENTITY_FILE="$2";  shift 2 ;;
        --password)       SSH_PASSWORD="$2";       shift 2 ;;
        --session-user)   SESSION_USER="$2";       shift 2 ;;
        --repo-dir)       REPO_DIR="$2";           shift 2 ;;
        --build-dir)      BUILD_DIR="$2";          shift 2 ;;
        --host-build-dir) HOST_BUILD_DIR="$2";     shift 2 ;;
        --cmake-prefix)   HOST_CMAKE_PREFIX_PATH="$2"; shift 2 ;;
        --jobs)           JOBS="$2";               shift 2 ;;
        --timeout)        SMOKE_TIMEOUT="$2";      shift 2 ;;
        --artifact-root)  HOST_ARTIFACT_ROOT="$2"; shift 2 ;;
        --fast)           FAST_MODE=1;             shift   ;;
        --full)           FAST_MODE=0;             shift   ;;
        --configure)      BUILD_ONLY=0;            shift   ;;
        --build-only)     BUILD_ONLY=1;            shift   ;;
        --skip-build)     SKIP_BUILD=1;            shift   ;;
        --skip-smoke)     RUN_SMOKE=0;             shift   ;;
        --strict-process) STRICT_PROCESS=1;        shift   ;;
        --help|-h)        usage; exit 0                    ;;
        *) echo "[qemu-smoke] unknown arg: $1" >&2; usage >&2; exit 1 ;;
    esac
done

if [[ "$FAST_MODE" != "1" ]]; then
    RUNTIME_TARGETS=("${FULL_TARGETS[@]}")
fi

if [[ "$BUILD_ONLY" == "auto" ]]; then
    if [[ -f "$HOST_BUILD_DIR/CMakeCache.txt" ]]; then
        BUILD_ONLY=1
    else
        BUILD_ONLY=0
    fi
fi

echo "[qemu-smoke] Starting"
printf '  %-16s: %s\n' \
    "user"           "$SSH_USER" \
    "session-user"   "$SESSION_USER" \
    "repo-dir"       "$REPO_DIR" \
    "build-dir"      "$BUILD_DIR" \
    "host-build-dir" "$HOST_BUILD_DIR" \
    "host-repo-dir"  "$HOST_REPO_DIR" \
    "cmake-prefix"   "$HOST_CMAKE_PREFIX_PATH" \
    "jobs"           "$JOBS" \
    "fast-mode"      "$FAST_MODE" \
    "build-only"     "$BUILD_ONLY" \
    "skip-build"     "$SKIP_BUILD" \
    "run-smoke"      "$RUN_SMOKE"
echo ""

# ── SSH connection pool (ControlMaster) ────────────────────────────────────────
mkdir -p "$STATE_DIR"
KNOWN_HOSTS="$(qemu_dev_known_hosts)"
SSH_CTL="$STATE_DIR/ctl-$$"
SSH_HOST="$SSH_USER@127.0.0.1"

echo "[qemu-smoke] Waiting for guest SSH on port $SSH_PORT..."
if ! qemu_dev_wait_ssh "$SSH_PORT" 90; then
    echo "[qemu-smoke] ERROR: guest SSH tidak siap setelah 3 menit." >&2
    echo "[qemu-smoke] Pastikan VM sudah berjalan via: bash scripts/dev/qemu-run.sh" >&2
    exit 1
fi
echo "[qemu-smoke] Guest SSH ready"
echo ""

SSH_OPTS=(
    -o ControlMaster=auto
    -o "ControlPath=$SSH_CTL"
    -o ControlPersist=60
    -o StrictHostKeyChecking=accept-new
    -o "UserKnownHostsFile=$KNOWN_HOSTS"
    -p "$SSH_PORT"
)
SCP_OPTS=(
    -o ControlMaster=auto
    -o "ControlPath=$SSH_CTL"
    -o ControlPersist=60
    -o StrictHostKeyChecking=accept-new
    -o "UserKnownHostsFile=$KNOWN_HOSTS"
    -P "$SSH_PORT"
)
SSH_CMD=(ssh)
SCP_CMD=(scp)
SSH_BASE_OPTS=("${SSH_OPTS[@]}")
SCP_BASE_OPTS=("${SCP_OPTS[@]}")
SSH_PASSWORD_MODE=0
if [[ -n "$SSH_PASSWORD" ]] && command -v sshpass >/dev/null 2>&1; then
    SSH_PASSWORD_MODE=1
    SSH_CMD=(env "SSHPASS=$SSH_PASSWORD" sshpass -e ssh)
    SCP_CMD=(env "SSHPASS=$SSH_PASSWORD" sshpass -e scp)
    SSH_OPTS=(
        -o BatchMode=no
        -o NumberOfPasswordPrompts=1
        -o PreferredAuthentications=password,keyboard-interactive,publickey
        "${SSH_OPTS[@]}"
    )
    SCP_OPTS=(
        -o BatchMode=no
        -o NumberOfPasswordPrompts=1
        -o PreferredAuthentications=password,keyboard-interactive,publickey
        "${SCP_OPTS[@]}"
    )
else
    SSH_OPTS=(-o BatchMode=yes -o NumberOfPasswordPrompts=0 -o IdentitiesOnly=yes "${SSH_OPTS[@]}")
    SCP_OPTS=(-o BatchMode=yes -o NumberOfPasswordPrompts=0 -o IdentitiesOnly=yes "${SCP_OPTS[@]}")
fi
if [[ -f "$SSH_IDENTITY_FILE" ]]; then
    SSH_OPTS=(-i "$SSH_IDENTITY_FILE" "${SSH_OPTS[@]}")
    SCP_OPTS=(-i "$SSH_IDENTITY_FILE" "${SCP_OPTS[@]}")
fi

g_ssh()  { "${SSH_CMD[@]}" "${SSH_OPTS[@]}" "$SSH_HOST" "$@"; }
g_scp()  { "${SCP_CMD[@]}" "${SCP_OPTS[@]}" "$1" "$SSH_HOST:$2"; }
g_scpr() { "${SCP_CMD[@]}" "${SCP_OPTS[@]}" -r "$SSH_HOST:$1" "$2"; }

# Buka master connection terlebih dahulu; semua g_ssh/g_scp berikutnya reuse ini.
# ControlPersist=3600 supaya master tidak expire selama build lokal berlangsung.
open_master_connection() {
    "${SSH_CMD[@]}" \
        "${SSH_OPTS[@]}" \
        -o ControlMaster=yes \
        -o ControlPersist=3600 \
        -N -f "$SSH_HOST"
}

interactive_tty_available() {
    [[ -e /dev/tty ]] || return 1
    ( : <>/dev/tty ) >/dev/null 2>&1
}

open_master_connection_interactive() {
    interactive_tty_available || return 1
    "${SSH_CMD[@]}" \
        "${SSH_OPTS[@]}" \
        -o ControlMaster=yes \
        -o ControlPersist=3600 \
        -N -f "$SSH_HOST" \
        </dev/tty >/dev/tty
}

if ! open_master_connection; then
    if interactive_tty_available; then
        echo "[qemu-smoke] Public-key auth gagal; mencoba login password interaktif untuk $SSH_HOST..."
        SSH_CMD=(ssh)
        SCP_CMD=(scp)
        SSH_OPTS=(
            -o BatchMode=no
            -o PreferredAuthentications=publickey,password,keyboard-interactive
            "${SSH_BASE_OPTS[@]}"
        )
        SCP_OPTS=(
            -o BatchMode=no
            -o PreferredAuthentications=publickey,password,keyboard-interactive
            "${SCP_BASE_OPTS[@]}"
        )
        if [[ -f "$SSH_IDENTITY_FILE" ]]; then
            SSH_OPTS=(-i "$SSH_IDENTITY_FILE" "${SSH_OPTS[@]}")
            SCP_OPTS=(-i "$SSH_IDENTITY_FILE" "${SCP_OPTS[@]}")
        fi
        if ! open_master_connection_interactive; then
            echo "[qemu-smoke] ERROR: SSH login gagal untuk $SSH_HOST port $SSH_PORT." >&2
            exit 1
        fi
    else
        echo "[qemu-smoke] ERROR: SSH login non-interaktif gagal untuk $SSH_HOST port $SSH_PORT." >&2
        if [[ -n "$SSH_PASSWORD" && "$SSH_PASSWORD_MODE" != "1" ]]; then
            echo "[qemu-smoke]        SLM_QEMU_SSH_PASSWORD sudah diset, tapi sshpass tidak ditemukan." >&2
            echo "[qemu-smoke]        Install sshpass di host atau jalankan dari terminal interaktif." >&2
        else
            echo "[qemu-smoke]        Tidak ada password/key valid dan /dev/tty tidak tersedia untuk prompt." >&2
            echo "[qemu-smoke]        Pasang public key ke guest atau set SLM_QEMU_SSH_PASSWORD dengan sshpass tersedia." >&2
        fi
        exit 1
    fi
fi

cleanup() {
    "${SSH_CMD[@]}" \
        -o "ControlPath=$SSH_CTL" \
        -p "$SSH_PORT" \
        -O exit "$SSH_HOST" 2>/dev/null || true
}
trap cleanup EXIT

# ── Mount hostshare di guest (hanya perlu untuk pipeline penuh) ───────────────
if [[ "$FAST_MODE" == "0" ]]; then
    echo "[qemu-smoke] Mounting hostshare di guest..."
    g_scp "$SCRIPT_DIR/qemu-guest-bootstrap.sh" /tmp/qemu-guest-bootstrap.sh
    g_ssh -tt "chmod +x /tmp/qemu-guest-bootstrap.sh && sudo /tmp/qemu-guest-bootstrap.sh --mount-only"
else
    echo "[qemu-smoke] Fast mode: skip hostshare mount"
fi

# ── Build di host ────────────────────────────────────────────────────────────
if [[ "$SKIP_BUILD" == "1" ]]; then
    echo "[qemu-smoke] Build skipped (--skip-build)"
else
    echo "[qemu-smoke] Building on host..."
    HOST_BUILD_ARGS=(
        --repo-dir "$HOST_REPO_DIR"
        --build-dir "$HOST_BUILD_DIR"
        --jobs "$JOBS"
        --cmake-prefix "$HOST_CMAKE_PREFIX_PATH"
        --no-run
    )
    for t in "${RUNTIME_TARGETS[@]}"; do
        HOST_BUILD_ARGS+=(--target "$t")
    done
    if [[ "$BUILD_ONLY" == "1" ]]; then
        if [[ -f "$HOST_BUILD_DIR/CMakeCache.txt" ]]; then
            HOST_BUILD_ARGS+=(--build-only)
        else
            echo "[qemu-smoke] Build cache not found; running configure once"
        fi
    fi
    "$SCRIPT_DIR/qemu-guest-build.sh" "${HOST_BUILD_ARGS[@]}"
fi

for t in "${FAST_TARGETS[@]}"; do
    if [[ ! -x "$HOST_BUILD_DIR/$t" ]]; then
        echo "[qemu-smoke] ERROR: missing executable: $HOST_BUILD_DIR/$t" >&2
        exit 1
    fi
done

echo "[qemu-smoke] Host dependency check..."
if ldd "$HOST_BUILD_DIR/slm-desktop" "$HOST_BUILD_DIR/slm-session-broker" | grep -q 'not found'; then
    ldd "$HOST_BUILD_DIR/slm-desktop" "$HOST_BUILD_DIR/slm-session-broker" | grep 'not found' >&2
    exit 1
fi
ldd "$HOST_BUILD_DIR/slm-desktop" "$HOST_BUILD_DIR/slm-session-broker" \
    | grep -E 'libicu(i18n|uc|data)\.so|libQt6Core\.so' \
    | sed 's/^/[qemu-smoke]   /' || true

# ── Sync artifacts ke guest ───────────────────────────────────────────────────
if [[ "$FAST_MODE" == "1" ]]; then
    echo "[qemu-smoke] Syncing minimal artifacts ke guest ($BUILD_DIR)..."
    g_ssh "mkdir -p $(printf '%q' "$BUILD_DIR")"
    rsync -az \
        -e "$(printf '%q ' "${SSH_CMD[@]}" "${SSH_OPTS[@]}")" \
        "$HOST_BUILD_DIR/slm-greeter" \
        "$HOST_BUILD_DIR/slm-watchdog" \
        "$HOST_BUILD_DIR/slm-recovery-app" \
        "$HOST_BUILD_DIR/slm-session-broker" \
        "$HOST_BUILD_DIR/slm-desktop" \
        "$SSH_HOST:$BUILD_DIR/"
else
    echo "[qemu-smoke] Syncing full build tree ke guest ($BUILD_DIR)..."
    rsync -az --delete \
        -e "$(printf '%q ' "${SSH_CMD[@]}" "${SSH_OPTS[@]}")" \
        "$HOST_BUILD_DIR/" \
        "$SSH_HOST:$BUILD_DIR/"
fi

# ── Install → Verify ─────────────────────────────────────────────────────────
echo "[qemu-smoke] Install → verify..."
if [[ "$FAST_MODE" == "1" ]]; then
    RCMD="sudo /bin/bash -lc $(printf '%q' "
set -euo pipefail
session_user='$(printf "%q" "$SESSION_USER")'
session_home=\$(getent passwd \"\$session_user\" | cut -d: -f6)
if [[ -z \"\$session_home\" ]]; then
    echo '[qemu-smoke][guest] cannot resolve session user home' >&2
    exit 1
fi
install -Dm755 '$BUILD_DIR/slm-greeter' /usr/local/bin/slm-greeter
install -Dm755 '$BUILD_DIR/slm-watchdog' /usr/local/bin/slm-watchdog
install -Dm755 '$BUILD_DIR/slm-recovery-app' /usr/local/bin/slm-recovery-app
install -Dm755 '$BUILD_DIR/slm-session-broker' /usr/libexec/slm-session-broker
ln -sfn /usr/libexec/slm-session-broker /usr/local/bin/slm-session-broker
install -Dm755 '$BUILD_DIR/slm-desktop' /usr/local/bin/slm-shell.real
cat > /usr/local/bin/slm-shell <<'SLM_SHELL_WRAPPER'
#!/bin/sh
unset KWIN_COMPOSE LIBGL_ALWAYS_SOFTWARE QSG_RHI_BACKEND
exec env QT_QUICK_BACKEND=software SLM_FAST_FIRST_FRAME=1 SLM_STARTUP_LOG=1 SLM_STARTUP_TRACE=1 /usr/local/bin/slm-shell.real "$@"
SLM_SHELL_WRAPPER
chmod 755 /usr/local/bin/slm-shell
file /usr/local/bin/slm-shell /usr/local/bin/slm-shell.real
/bin/sh -n /usr/local/bin/slm-shell
install -d -m0755 /usr/local/libexec
cat > /usr/local/libexec/slm-session-broker-launch <<'SLM_BROKER_LAUNCH'
#!/usr/bin/env bash
set -u
export SLM_OFFICIAL_SESSION=1
export XDG_SESSION_TYPE=\${XDG_SESSION_TYPE:-wayland}
export XDG_CURRENT_DESKTOP=\${XDG_CURRENT_DESKTOP:-SLM}
export LANG=\${LANG:-C.UTF-8}
export LC_ALL=\${LC_ALL:-C.UTF-8}
if [[ -z \"\${XDG_RUNTIME_DIR:-}\" ]]; then
    export XDG_RUNTIME_DIR=\"/run/user/\${UID}\"
fi
log=/tmp/slm-session-broker-launch.log
{
    echo \"===== \$(date --iso-8601=seconds 2>/dev/null || date) slm-session-broker-launch start =====\"
    echo \"uid=\$(id -u) gid=\$(id -g) user=\$(id -un)\"
    echo \"argv=\$*\"
    echo \"XDG_SESSION_ID=\${XDG_SESSION_ID:-<unset>}\"
    echo \"XDG_RUNTIME_DIR=\${XDG_RUNTIME_DIR:-<unset>}\"
    echo \"XDG_SEAT=\${XDG_SEAT:-<unset>}\"
    echo \"XDG_VTNR=\${XDG_VTNR:-<unset>}\"
    echo \"PATH=\${PATH:-<unset>}\"
    if [[ \"\${SLM_BROKER_LAUNCH_DIAGNOSTICS:-0}\" == \"1\" ]]; then
        if command -v loginctl >/dev/null 2>&1 && [[ -n \"\${XDG_SESSION_ID:-}\" ]]; then
            loginctl show-session \"\${XDG_SESSION_ID}\" --no-pager 2>&1 || true
        fi
        ldd /usr/libexec/slm-session-broker 2>&1 | grep -E 'not found|libQt6Core|libicu' || true
    fi
} >>\"\$log\" 2>&1
exec /usr/libexec/slm-session-broker \"\$@\" >>\"\$log\" 2>&1
SLM_BROKER_LAUNCH
chmod 0755 /usr/local/libexec/slm-session-broker-launch
install -d -m0755 /usr/share/wayland-sessions
cat > /usr/share/wayland-sessions/slm.desktop <<'SLM_DESKTOP_ENTRY'
[Desktop Entry]
Name=SLM Desktop
Comment=Start SLM Desktop
Exec=/usr/local/libexec/slm-session-broker-launch --mode normal
Type=Application
DesktopNames=SLM
X-GDM-SessionRegisters=true
SLM_DESKTOP_ENTRY
install -d -m0755 -o \"\$session_user\" -g \"\$session_user\" \"\$session_home/.config/slm-desktop\"
cat > \"\$session_home/.config/slm-desktop/config.json\" <<'SLM_CONFIG_JSON'
{
  \"compositor\": \"kwin_wayland\",
  \"compositorArgs\": [],
  \"shell\": \"slm-shell\",
  \"shellArgs\": [],
  \"compositorEnv\": {
    \"KWIN_COMPOSE\": \"Q\",
    \"KWIN_FORCE_SW_CURSOR\": \"1\"
  }
}
SLM_CONFIG_JSON
cat > \"\$session_home/.config/slm-desktop/state.json\" <<'SLM_STATE_JSON'
{
  \"config_pending\": false,
  \"crash_count\": 0,
  \"last_boot_status\": \"\",
  \"last_crash_reason\": \"\",
  \"last_good_snapshot\": \"\",
  \"last_mode\": \"normal\",
  \"recovery_reason\": \"\",
  \"safe_mode_forced\": false
}
SLM_STATE_JSON
chown \"\$session_user:\$session_user\" \"\$session_home/.config/slm-desktop/config.json\" \"\$session_home/.config/slm-desktop/state.json\"
echo '[qemu-smoke][guest] installed fast runtime + KWin software config'
ldd /usr/local/bin/slm-shell.real /usr/libexec/slm-session-broker | grep -E 'libicu(i18n|uc|data)\\.so|libQt6Core\\.so|not found' || true
: > /tmp/slm-greeter.log
: > /tmp/slm-greeter-service.log
: > /tmp/slm-session-broker.log
: > /tmp/slm-session-broker-launch.log
: > /tmp/slm-compositor.log
: > /tmp/slm-shell.log
if ! command -v cage >/dev/null 2>&1; then
    echo '[qemu-smoke][guest] cage not found; install cage before greetd greeter mode' >&2
    exit 1
fi
if ! command -v greetd >/dev/null 2>&1; then
    echo '[qemu-smoke][guest] greetd not found; install greetd before fast smoke' >&2
    exit 1
fi
if ! id -u greeter >/dev/null 2>&1; then
    useradd --system --home /var/lib/greetd --create-home --shell /bin/sh greeter
else
    current_shell=\$(getent passwd greeter | awk -F: '{print \$7}')
    if [[ \"\$current_shell\" == */nologin || \"\$current_shell\" == */false ]]; then
        usermod --shell /bin/sh greeter
    fi
fi
passwd -l greeter >/dev/null 2>&1 || true
install -d -m0755 /usr/local/libexec
install -d -m0750 /var/lib/greetd/logs
: > /var/lib/greetd/logs/slm-greeter.log
: > /var/lib/greetd/logs/slm-greeter-cage.log
chown -R greeter:greeter /var/lib/greetd /var/lib/greetd/logs
chmod 0640 /var/lib/greetd/logs/slm-greeter.log /var/lib/greetd/logs/slm-greeter-cage.log
cat > /etc/pam.d/slm <<'SLM_PAM'
#%PAM-1.0
auth       requisite    pam_nologin.so
auth       include      common-auth
account    include      common-account
password   include      common-password
session    required     pam_env.so readenv=1
session    required     pam_limits.so
session    required     pam_unix.so
session    optional     pam_loginuid.so
session    required     pam_systemd.so debug
SLM_PAM
cat > /etc/pam.d/greetd <<'GREETD_PAM'
#%PAM-1.0
auth       requisite    pam_nologin.so
auth       include      common-auth
account    include      common-account
password   include      common-password
session    required     pam_env.so readenv=1
session    required     pam_limits.so
session    required     pam_unix.so
session    optional     pam_loginuid.so
session    required     pam_systemd.so debug
GREETD_PAM
cat > /etc/pam.d/greetd-greeter <<'GREETD_GREETER_PAM'
#%PAM-1.0
auth       include      common-auth
account    include      common-account
session    required     pam_unix.so
session    required     pam_systemd.so debug
GREETD_GREETER_PAM
cat > /usr/local/libexec/slm-greeter-greetd-launch <<'SLM_GREETER_LAUNCH'
#!/usr/bin/env bash
set -u
export LIBSEAT_BACKEND=logind
export QT_QPA_PLATFORM=wayland
export QT_QUICK_BACKEND=software
export LIBGL_ALWAYS_SOFTWARE=1
log=/var/lib/greetd/logs/slm-greeter.log
{
    echo \"===== \$(date --iso-8601=seconds 2>/dev/null || date) slm-greeter start =====\"
    echo \"  GREETD_SOCK=\${GREETD_SOCK:-<unset>}\"
    echo \"  XDG_RUNTIME_DIR=\${XDG_RUNTIME_DIR:-<unset>}\"
    echo \"  WAYLAND_DISPLAY=\${WAYLAND_DISPLAY:-<unset>}\"
} >>\"\$log\" 2>&1
exec /usr/local/bin/slm-greeter >>\"\$log\" 2>&1
SLM_GREETER_LAUNCH
chmod 0755 /usr/local/libexec/slm-greeter-greetd-launch
cat > /usr/local/libexec/slm-greeter-cage-launch <<'SLM_GREETER_CAGE'
#!/usr/bin/env bash
set -u
export LIBSEAT_BACKEND=logind
export WLR_RENDERER=pixman
log=/var/lib/greetd/logs/slm-greeter-cage.log
{
    echo \"===== \$(date --iso-8601=seconds 2>/dev/null || date) cage start =====\"
    echo \"  GREETD_SOCK=\${GREETD_SOCK:-<unset>}\"
    echo \"  XDG_RUNTIME_DIR=\${XDG_RUNTIME_DIR:-<unset>}\"
} >>\"\$log\" 2>&1
exec cage -s -- /usr/local/libexec/slm-greeter-greetd-launch >>\"\$log\" 2>&1
SLM_GREETER_CAGE
chmod 0755 /usr/local/libexec/slm-greeter-cage-launch
install -d -m0755 /etc/greetd
cat > /etc/greetd/config.toml <<'GREETD_CONFIG'
[terminal]
vt = 7

[default_session]
command = \"/usr/local/libexec/slm-greeter-cage-launch\"
user = \"greeter\"
GREETD_CONFIG
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    systemctl disable --now slm-greeter.service 2>/dev/null || true
    greetd_unit=\$(systemctl show -p FragmentPath --value greetd.service 2>/dev/null || true)
    if [[ -n \"\$greetd_unit\" && -f \"\$greetd_unit\" ]]; then
        ln -sfn \"\$greetd_unit\" /etc/systemd/system/display-manager.service
    fi
    systemctl daemon-reload
    systemctl enable greetd.service
    systemctl reset-failed greetd.service || true
    systemctl restart greetd.service
    echo '[qemu-smoke][guest] enabled greetd handoff and disabled direct-PAM slm-greeter.service'
fi
")"
else
    RCMD="sudo SLM_TARGET_USER=$(printf '%q' "$SESSION_USER") bash $(printf '%q' "$REPO_DIR/scripts/login/install-slm-desktop-runtime.sh") $(printf '%q' "$BUILD_DIR")"
    RCMD+=" && sudo SLM_TARGET_USER=$(printf '%q' "$SESSION_USER") bash $(printf '%q' "$REPO_DIR/scripts/login/verify-slm-desktop-runtime.sh")"
    RCMD+=" && sudo bash $(printf '%q' "$REPO_DIR/scripts/login/verify-greetd-slm.sh")"
fi
g_ssh -tt "$RCMD"

# ── Smoke ─────────────────────────────────────────────────────────────────────
if [[ "$RUN_SMOKE" -eq 0 ]]; then
    echo "[qemu-smoke] install+verify selesai (--skip-smoke)."
    exit 0
fi

echo "[qemu-smoke] Running smoke test..."
TS="$(date -u +%Y%m%dT%H%M%SZ)"
REMOTE_ARTIFACT_DIR="/tmp/slm-qemu-session-smoke-${TS}"
HOST_ARTIFACT_DIR="${HOST_ARTIFACT_ROOT}/${TS}"
mkdir -p "$HOST_ARTIFACT_DIR"

# Upload smoke runner ke guest sebelum dipanggil via sudo.
g_scp "$SCRIPT_DIR/qemu-guest-session-smoke.sh" /tmp/qemu-guest-session-smoke.sh
g_ssh "chmod +x /tmp/qemu-guest-session-smoke.sh"

SCMD="sudo /tmp/qemu-guest-session-smoke.sh"
SCMD+=" --session-user $(printf '%q' "$SESSION_USER")"
SCMD+=" --timeout   $(printf '%q' "$SMOKE_TIMEOUT")"
SCMD+=" --artifact-dir $(printf '%q' "$REMOTE_ARTIFACT_DIR")"
[[ "$STRICT_PROCESS" -eq 1 ]] && SCMD+=" --strict-process"

SMOKE_EXIT=0
g_ssh -tt "$SCMD" || SMOKE_EXIT=$?

g_scpr "$REMOTE_ARTIFACT_DIR" "$HOST_ARTIFACT_DIR/"

ARTIFACT_SUBDIR="$HOST_ARTIFACT_DIR/$(basename "$REMOTE_ARTIFACT_DIR")"
echo ""
echo "[qemu-smoke] Summary:"
cat "$ARTIFACT_SUBDIR/summary.log" 2>/dev/null || true
echo ""
echo "[qemu-smoke] Artifacts: $ARTIFACT_SUBDIR"

exit "$SMOKE_EXIT"
