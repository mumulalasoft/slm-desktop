# SLM Package Policy Service - Phase 1

Status: implemented in monorepo as baseline protection layer.

## Components

- Daemon / checker binary: `slm-package-policy-service`
- Core modules:
  - `src/services/packagepolicy/packagepolicyconfig.*`
  - `src/services/packagepolicy/aptsimulator.*`
  - `src/services/packagepolicy/packagepolicyengine.*`
  - `src/services/packagepolicy/packagepolicyservice.*`
  - `src/services/packagepolicy/packagepolicylogger.*`
- Wrapper scripts:
  - `scripts/package-policy/wrappers/apt`
  - `scripts/package-policy/wrappers/apt-get`
  - `scripts/package-policy/wrappers/dpkg`

## Policy files

Installed to:

- `/etc/slm/package-policy/protected-capabilities.yaml`
- `/etc/slm/package-policy/package-mappings/debian.yaml`

Default files are stored in repository under `config/package-policy/`.

## Phase 1 rules

Blocked when transaction touches protected package by:

- remove
- replace
- downgrade

Allowed when protected package is untouched.

## Simulation

APT and apt-get are evaluated using:

- `/usr/bin/apt-get -s <args>`

Parser extracts:

- `Remv` -> `remove[]`
- `Inst` -> `install[]`
- `Upgr` -> `upgrade[]`

Result shape:

```json
{
  "install": [],
  "remove": [],
  "upgrade": [],
  "replace": [],
  "downgrade": []
}
```

## Logging

Primary log target:

- `/var/log/slm-package-policy.log`

Fallback when not writable:

- `~/.local/state/slm/package-policy.log`

## Ubuntu hardening (implemented)

- Pre-transaction snapshot hook:
  - `scripts/package-policy/pre-transaction-snapshot.sh`
  - Stores package/version list, apt source config snapshot, and desktop-critical config tar.
- Post-transaction health gate:
  - `scripts/package-policy/post-transaction-health-check.sh`
  - Runs `dpkg --audit`, `apt-get -s check`, and validates each capability still has at least one installed provider from mapping.
- Recovery helper:
  - `scripts/package-policy/recover-last-snapshot.sh`
  - Dry-run by default, `--apply` (root) restores latest snapshot baseline.
- Rollback config + force safe mode:
  - `scripts/package-policy/trigger-safe-mode-recovery.sh`
  - On health failure, sets `safe_mode_forced=true` and attempts `config.safe.json` (fallback `config.prev.json`) as new active config.
- Auto-disable external repo on health failure:
  - `scripts/package-policy/disable-external-repos.sh`
  - Called by post-transaction health gate (default enabled via `SLM_PACKAGE_POLICY_AUTO_DISABLE_EXTERNAL_REPOS=1`).

## Wrapper flow

`command -> wrapper -> slm-package-policy-service --check -> allow/block -> real binary`

Bypass switch for emergency:

- `SLM_PACKAGE_POLICY_BYPASS=1`

## Install wrappers

```
sudo scripts/package-policy/install-wrappers.sh
```

Ensure wrapper `bin` directory is ahead of `/usr/bin` in `PATH`.

## Systemd unit

Unit file:

- `scripts/systemd/system/slm-package-policy.service`

Example install (system):

```
sudo install -m 0644 scripts/systemd/system/slm-package-policy.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now slm-package-policy.service
```
