# Run Modes Matrix

## Supported Modes

1. Host Wayland run (default)

```bash
compositor/scripts/run-nested.sh
```

## Key Environment Variables

- `DS_WINDOWING_BACKEND`: `kwin-wayland` (canonicalized)
- `DS_KWIN_PROFILE`: enable KWin runtime profile logs.
- `DS_SHELL_APP_BIN`: explicit path for `appSlm_Desktop` binary.

## Log Files

- `DS_NESTED_LOG` (default: `~/.desktopshell-nested.log`)
- `DS_APP_LOG` (default: `~/.desktopshell-app.log`)
- `DS_ENV_LOG` (default: `/tmp/desktopshell-env.log`)
- `DS_LSBLK_LOG` (default: `/tmp/desktopshell-lsblk.json`)

## Smoke Check

```bash
scripts/smoke-backends.sh
scripts/smoke-runtime.sh
scripts/test-globalmenu.sh
scripts/smoke-globalmenu-focus.sh --strict --samples 20 --interval 1 --min-unique 2
```

`smoke-runtime.sh` memverifikasi startup app + scan log untuk pola error runtime kritis.

`test-globalmenu.sh` memverifikasi dump + healthcheck global menu (`globalmenuctl`) untuk triage registrar/binding.

`smoke-globalmenu-focus.sh` memverifikasi binding global menu berubah saat fokus pindah antar app (GTK/Qt/KDE).

Opsional strict mode:

```bash
scripts/test-globalmenu.sh --strict
```
