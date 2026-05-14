# SLM Desktop Runtime Compatibility Checklist

This checklist defines SLM runtime compatibility gates for Linux desktop app ecosystems.

## Scope

SLM keeps branding in UX/shell layers, while preserving freedesktop, logind/PAM, DBus, Wayland, XWayland, and portal compatibility.

## Startup Validators (session-broker)

Validator entrypoint:

- `src/login/libslmlogin/slmplatformcheck.cpp`

Startup report output:

- `~/.config/slm-desktop/compatibility-report.json`

Validated domains:

- Environment validator
  - `XDG_RUNTIME_DIR` exists and is usable.
  - `SLM_OFFICIAL_SESSION=1` and required binaries are present.
- Session validator
  - `XDG_SESSION_TYPE`, `XDG_SESSION_CLASS`, and `loginctl show-session` compatibility hints.
- Wayland/X11 validator
  - `wayland-0` compatibility path sanity.
  - Detects non-standard exported Wayland display values without a valid compatibility alias.
  - `DISPLAY` and `XAUTHORITY` preconditions for XWayland fallback.
- Portal/DBus validator
  - User bus accessibility checks (`busctl --user`).
  - Portal DBus tree visibility (`org.freedesktop.portal.Desktop`).
  - Portal unit health hints (`systemctl --user is-active xdg-desktop-portal`).
  - Detects global portal-disable env (`QT_NO_XDG_DESKTOP_PORTAL=1`, `GTK_USE_PORTAL=0`, `GIO_USE_PORTALS=0`).

Failure behavior:

- Hard startup blockers go to `issues` (platform not OK).
- Compatibility degradations go to `warnings`.
- Session broker logs both explicitly and prints report path.

## Runtime Validator (QEMU smoke)

Runtime validator entrypoint:

- `scripts/dev/qemu-guest-session-smoke.sh`

Runtime artifacts:

- `compatibility-checklist.log`
- `compatibility-report.json`
- `slm-compatibility-report.json` (copied broker report when available)

Runtime gates validated:

- `loginctl` session properties (`Type=wayland`, `Active=yes`, `Remote=no`, `State=active`, `Class=user`, `Seat=seat0`)
- Runtime dir ownership/permissions/path (`/run/user/<uid>`, `0700`)
- Wayland compatibility socket (`/run/user/<uid>/wayland-0`)
- XWayland socket (`/tmp/.X11-unix/X0`)
- XAuthority availability
- User DBus socket
- Portal service and bus name visibility
- Kernel deny signal detection (`DENIED`, `apparmor`)

## Manual Compatibility Matrix

Must be validated regularly on integration builds:

- GTK3 app
- GTK4 app
- Qt6 app
- Electron app
- Chromium
- VSCode
- Steam
- Snap Firefox
- Flatpak app
- X11 legacy app

Each target must pass:

- launch
- render
- input
- clipboard
- file picker
- drag and drop
- notifications

## Mandatory Diagnostic Commands

- `loginctl show-session $XDG_SESSION_ID`
- `busctl --user`
- `systemctl --user status xdg-desktop-portal xdg-desktop-portal-gtk`
- `ls -l /run/user/$UID/wayland-0`
- `echo DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY`
- `journalctl -b -k | grep -E 'DENIED|apparmor'`
