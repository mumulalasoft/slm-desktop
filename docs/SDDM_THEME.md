# SDDM Theme (SLM)

This repository ships adapted SDDM themes under:

- `sddm/theme/SLM` (Theme-Id: `SLM`, QtQuick-native, no `org.kde.*` imports)
- `sddm/theme/Light` (Theme-Id: `SLM-Light`)
- `sddm/theme/Dark` (Theme-Id: `SLM-Dark`)

## What was adapted for SLM

- Metadata rebranded from MacTahoe to SLM.
- Hardcoded icon paths like `/usr/share/sddm/themes/MacTahoe-*/assets/...` replaced with relative `assets/...`.
- Accent color aligned to SLM (`#0a84ff`).
- Default logo and background replaced using repository SLM assets:
  - `icons/logo_white.svg`
  - `images/wallpaper.jpeg`

## Install

Use helper script:

```bash
scripts/install-sddm-theme.sh slm
# or
scripts/install-sddm-theme.sh light
# or
scripts/install-sddm-theme.sh dark
```

What it does:

1. Copies theme directories to:
   - `/usr/share/sddm/themes/SLM`
   - `/usr/share/sddm/themes/SLM-Light`
   - `/usr/share/sddm/themes/SLM-Dark`
2. Writes `/etc/sddm.conf.d/10-slm-theme.conf` with:

```ini
[Theme]
Current=SLM-Light
```

(or `SLM` / `SLM-Dark` based on selected variant)

3. You then restart SDDM:

```bash
sudo systemctl restart sddm
```

## Notes

- `SLM` is the recommended native theme for SLM desktop stack.
- `SLM-Light`/`SLM-Dark` are compatibility variants adapted from KDE-oriented base.
- Native `SLM` theme currently includes:
  - realtime clock update,
  - user chooser (from `userModel`) with avatar/initial fallback + manual username fallback,
  - Caps Lock indicator,
  - styled controls aligned to SLM visual direction.
