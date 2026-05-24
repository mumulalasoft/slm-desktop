# Dev Scripts

`scripts/dev/` is the entrypoint for local development and QEMU guest workflows.

## Entry Points

- `desktop-run-dev.sh`: launch the desktop shell in a local dev session.
- `filemanager-dev-rebuild.sh`: hot rebuild filemanager pieces and relink the shell.
- `workspace.env`: shared environment for local desktop/filemanager dev.

## QEMU Workflows

- `qemu-create-disk.sh`
- `qemu-run.sh`
- `qemu-ssh.sh`
- `qemu-scp.sh`
- `qemu-bootstrap-remote.sh`
- `qemu-install-deps-remote.sh` - wrapper compatibility untuk `qemu-bootstrap-remote.sh --apt-only`
- `qemu-build-remote.sh` - canonical host->guest build entrypoint
- `qemu-session-smoke-remote.sh`
- `qemu-login-smoke-pipeline.sh` - build + runtime install + verify + optional smoke
- `qemu-guest-bootstrap.sh`
- `qemu-guest-build.sh`
- `qemu-guest-session-smoke.sh`
- `qemu-smoke.sh` - build + install + verify + smoke, memakai build entrypoint canonical

## Usage

Run these from the repo root unless a script says otherwise.

Examples:

```bash
bash scripts/dev/desktop-run-dev.sh
bash scripts/dev/filemanager-dev-rebuild.sh --fm-only
bash scripts/dev/qemu-run.sh --with-iso
```

The older names `run-dev.sh` and `dev-rebuild.sh` are kept as wrappers for compatibility.
