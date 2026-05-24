#!/usr/bin/env bash
# slm-session-check — Diagnose the current SLM / logind session state.
# Run this inside a logged-in session (or via sudo -u USER) to verify seat,
# runtime dir, DRM access, and D-Bus health.
#
# Install:  sudo install -m755 scripts/login/slm-session-check.sh /usr/local/bin/slm-session-check

set -euo pipefail

RED='\033[0;31m'; YEL='\033[0;33m'; GRN='\033[0;32m'; RST='\033[0m'
ok()   { printf "${GRN}[OK]${RST}  %s\n" "$*"; }
warn() { printf "${YEL}[WARN]${RST} %s\n" "$*"; }
fail() { printf "${RED}[FAIL]${RST} %s\n" "$*"; }

echo "=== SLM Session Check ==="
echo "Date : $(date --iso-8601=seconds)"
echo ""

# ── Identity ──────────────────────────────────────────────────────────────────
USER_NAME=$(id -un)
USER_UID=$(id -u)
USER_GID=$(id -g)
USER_GROUPS=$(id -Gn | tr ' ' ',')
echo "User : $USER_NAME  uid=$USER_UID  gid=$USER_GID"
echo "Groups: $USER_GROUPS"
echo ""

# ── Session environment ───────────────────────────────────────────────────────
echo "--- Session environment ---"
for VAR in XDG_SESSION_TYPE XDG_SESSION_ID XDG_SESSION_CLASS \
           XDG_SEAT XDG_VTNR XDG_RUNTIME_DIR XDG_CURRENT_DESKTOP \
           WAYLAND_DISPLAY DISPLAY DBUS_SESSION_BUS_ADDRESS \
           SLM_OFFICIAL_SESSION LIBSEAT_BACKEND; do
  VAL="${!VAR:-}"
  if [[ -n "$VAL" ]]; then
    ok "$VAR=$VAL"
  else
    warn "$VAR=<unset>"
  fi
done
echo ""

# ── XDG_RUNTIME_DIR validation ────────────────────────────────────────────────
echo "--- XDG_RUNTIME_DIR ---"
RTDIR="${XDG_RUNTIME_DIR:-}"
if [[ -z "$RTDIR" ]]; then
  fail "XDG_RUNTIME_DIR is not set — pam_systemd.so did not run or PAM file is broken"
elif [[ ! -d "$RTDIR" ]]; then
  fail "XDG_RUNTIME_DIR='$RTDIR' does not exist"
else
  RTDIR_OWNER=$(stat -c '%u' "$RTDIR" 2>/dev/null || echo "?")
  RTDIR_PERM=$(stat -c '%a' "$RTDIR" 2>/dev/null || echo "?")
  if [[ "$RTDIR_OWNER" != "$USER_UID" ]]; then
    fail "XDG_RUNTIME_DIR owner=$RTDIR_OWNER expected=$USER_UID (permission problem)"
  elif [[ "$RTDIR_PERM" != "700" ]]; then
    warn "XDG_RUNTIME_DIR permissions=$RTDIR_PERM (expected 700)"
  else
    ok "XDG_RUNTIME_DIR='$RTDIR' owner=$RTDIR_OWNER perm=$RTDIR_PERM"
  fi
fi
echo ""

# ── loginctl session status ───────────────────────────────────────────────────
echo "--- loginctl session ---"
SESSION_ID="${XDG_SESSION_ID:-}"
if [[ -z "$SESSION_ID" ]]; then
  warn "XDG_SESSION_ID not set — cannot query loginctl"
else
  if loginctl show-session "$SESSION_ID" \
       -p Type -p Class -p Seat -p Active -p Remote -p Service 2>/dev/null; then
    # Check specific fields
    SESSION_INFO=$(loginctl show-session "$SESSION_ID" \
       -p Type -p Class -p Seat -p Active -p Remote 2>/dev/null || true)
    [[ "$SESSION_INFO" == *"Seat=seat0"*  ]] && ok "Seat=seat0"  || fail "Seat != seat0 — kwin_wayland will fail to open DRM"
    [[ "$SESSION_INFO" == *"Type=wayland"* ]] && ok "Type=wayland" || warn "Type is not wayland"
    [[ "$SESSION_INFO" == *"Active=yes"*  ]] && ok "Active=yes"  || warn "Session not Active"
    [[ "$SESSION_INFO" == *"Remote=no"*   ]] && ok "Remote=no"   || fail "Remote=yes — compositor cannot access DRM remotely"
  else
    fail "loginctl show-session $SESSION_ID failed"
  fi
fi
echo ""

# ── DRM / GPU access ──────────────────────────────────────────────────────────
echo "--- /dev/dri ---"
if ls /dev/dri/ &>/dev/null; then
  ls -la /dev/dri/
  HAS_CARD=0; HAS_RENDER=0
  ls /dev/dri/card* &>/dev/null && HAS_CARD=1 || true
  ls /dev/dri/renderD* &>/dev/null && HAS_RENDER=1 || true
  [[ "$HAS_CARD"   == "1" ]] && ok "/dev/dri/card* present"   || warn "/dev/dri/card* missing"
  [[ "$HAS_RENDER" == "1" ]] && ok "/dev/dri/renderD* present (needed by kwin_wayland)" \
                               || warn "/dev/dri/renderD* missing — kwin_wayland needs a render node"
else
  fail "/dev/dri/ not found — no GPU devices"
fi
echo ""

# ── Group membership ──────────────────────────────────────────────────────────
echo "--- Group membership ---"
for GRP in video render input; do
  if id -nG | grep -qw "$GRP" 2>/dev/null; then
    ok "member of group '$GRP'"
  else
    warn "NOT in group '$GRP' — may affect DRM/input access"
  fi
done
echo ""

# ── D-Bus session bus ─────────────────────────────────────────────────────────
echo "--- D-Bus session bus ---"
DBUS_ADDR="${DBUS_SESSION_BUS_ADDRESS:-}"
if [[ -z "$DBUS_ADDR" ]]; then
  fail "DBUS_SESSION_BUS_ADDRESS not set"
elif command -v dbus-send &>/dev/null; then
  if dbus-send --session --print-reply \
       --dest=org.freedesktop.DBus /org/freedesktop/DBus \
       org.freedesktop.DBus.ListNames &>/dev/null; then
    ok "D-Bus session bus reachable at $DBUS_ADDR"
  else
    fail "D-Bus session bus unreachable (address=$DBUS_ADDR)"
  fi
else
  warn "dbus-send not found — cannot verify D-Bus"
fi
echo ""

# ── kwin_wayland availability ─────────────────────────────────────────────────
echo "--- kwin_wayland ---"
if command -v kwin_wayland &>/dev/null; then
  ok "kwin_wayland found at $(command -v kwin_wayland)"
else
  warn "kwin_wayland not found on PATH"
fi
echo ""

echo "=== slm-session-check complete ==="
