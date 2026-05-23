# SLM Installer & OOBE — UX/UI Design Plan

---

## 1. Design Philosophy & Tone

### Layer 1 — slm-installer (Calamares shell)

**North star:** A composure machine. The installer communicates that something irreversible and consequential is happening — rewriting a disk — and it meets that weight with stillness, not animation. Every screen is a decision point or a status report. Nothing decorates, nothing distracts. The visual language borrows from surgical instruments: high contrast, zero ornament, immediate legibility. The user should feel *informed confidence* — "I understand exactly what this system is going to do to my machine." The moment the progress bar completes, they should feel the quiet pride of a procedure completed without incident.

Divergence from typical Linux installers: Ubiquity and Calamares defaults arrive with translucent sidebars listing completed steps, live log tickers, and illustrated mascots. SLM rejects all three. There is no sidebar. There is no log visible by default. There is no mascot. What remains is a centered card, one action per screen, and copy written by someone who respects the user's intelligence.

### Layer 2 — slm-welcome (OOBE)

**North star:** The first morning in a new apartment — everything is yours to arrange, everything is clean, the light is good. This is the moment that justifies every constraint the installer imposed. The OOBE is allowed to be expressive, kinetic, and personal precisely because the installer already guaranteed the foundation is sound. The user should feel *welcomed into capability* — "This desktop was built around me." Motion is not decoration here; it narrates. Every transition is a sentence in a story that ends with a fully configured, personal, alive desktop.

### The Emotional Arc

| Stage | Surface | Target Feeling |
|---|---|---|
| Boot from live USB | Installer — Welcome | Calm reassurance ("I am in good hands") |
| Hardware check | Installer — Validation | Trust ("It checked everything") |
| Disk selection | Installer — Disk | Informed confidence ("I understand the layout") |
| Installation running | Installer — Progress | Patience earned ("Something real is being built") |
| Restart | Installer — Complete | Anticipation |
| First login, OOBE | OOBE — Welcome | Delight, recognition ("This is different") |
| Personalization | OOBE — Preferences | Agency, pleasure |
| Final screen | OOBE — Done | Arrival |

---

## 2. Information Architecture — Installer

Each screen is a contained card centered on a neutral matte background (`#f5f5f7` light / `#1a1a1c` dark). No sidebar. No step-count breadcrumb visible during flow — the user does not need to know there are seven steps, they need to make one decision at a time.

---

### Screen 1 — Welcome & Language

**Purpose:** Orient and gate-check. The very first impression of SLM's character.

**On screen:**
- SLM wordmark, centered, 28px, weight 600, letter-spacing -0.4
- Headline: "Welcome to SLM Desktop." — 36px display, weight 700
- Language list: scrollable, single-selection, 44px row height (touch-friendly), current locale pre-selected based on firmware locale
- Primary button: "Continue" — disabled until language confirmed

**Not on screen:** Installation type selection, disk information, any technical terminology, progress indicators, step counter.

**Primary action:** Select language → Continue.

**Exit condition:** Language committed. Locale written to installer context. Proceed to Screen 2.

---

### Screen 2 — Hardware & Compatibility

**Purpose:** Validate silently, surface only what requires the user's attention. This is a gate — a UEFI failure here is a hard block, not a warning.

**On screen:**
A vertical checklist of hardware checks, each resolving in real time with a 120ms entry animation per row (staggered 40ms):

```
  ✓  UEFI firmware         Modern boot system detected
  ✓  Memory                16 GB available (4 GB required)
  ◌  Display               Checking graphics compatibility...
  ⚠  Secure Boot           Disabled — optional, can be enabled after install
```

Icons: 16px, stroke weight 1.5px. Color: `success` (#28c840) for pass, `warning` (#ff9f0a) for advisory, `error` (#d14b4b) for block. No color used for the neutral "checking" state — only a spinner.

**Blocking condition (UEFI not detected):**
The check row expands inline — it does not navigate away:

> **"This computer cannot run SLM Desktop."**
> SLM requires UEFI firmware. This computer uses an older BIOS system that is not supported.
> *If you believe this is an error, restart and check your firmware settings. [Quit Installer]*

The word "BIOS" appears because it is the word the user will find in their firmware menu. We do not hide it — we contextualize it.

**Not on screen:** Raw firmware strings, DMI table output, hex codes, kernel messages.

**Primary action:** All checks pass or advisory-only → Continue.

**Exit condition:** No hard blocks. Advisory warnings persist as a summary line on the Confirmation screen.

---

### Screen 3 — Disk Selection

**Purpose:** The single most consequential decision in the installer. The UI must make the outcome legible, not just the input.

**On screen:**
Each detected disk is a card (radius 12, border `#dde2e7`, shadow `shadowSm`). Card width: 100% of content area minus 48px margin each side.

```
┌─────────────────────────────────────────────────────────┐
│  ◉  Samsung 970 EVO 1TB                   /dev/nvme0n1  │
│     NVMe · 931 GB free · SMART: Healthy                 │
│                                                         │
│  Layout after installation:                             │
│  ┌──────┬────────────────────────────┬────────┐         │
│  │ EFI  │      SLM Desktop           │Recovery│         │
│  │ 1 GB │         ~920 GB            │  10 GB │         │
│  └──────┴────────────────────────────┴────────┘         │
│                                                         │
│  Your files on this disk will be erased.                │
└─────────────────────────────────────────────────────────┘
```

The partition diagram assembles itself when the card is selected: the three segments grow from left to right over 280ms, `easingDecelerate`. This is the one intentional motion in the installer — it makes the abstract layout concrete, not decorative.

**The Btrfs explanation** appears below the diagram on selection, as inline secondary text (12px, `textSecondary`):

> "SLM uses a modern filesystem that enables instant system recovery and safe updates. No configuration needed."

That is the complete explanation the user needs. "Btrfs" does not appear on this screen. It appears in the Confirmation screen's technical summary (collapsed by default, expandable for curious users).

"Recovery partition" is called "Recovery" — not "recovery-staging", not "@recovery-staging". The 10 GB figure appears but is not explained; users understand reserved system space from every mobile OS they own.

**Not on screen:** Manual partition editor (available via "Advanced…" link, small, bottom-right, 11px — discoverable but not tempting), filesystem selector, subvolume names, mount options, bootloader selector.

**Primary action:** Select a disk → Continue.

**Exit condition:** Disk selection confirmed. If no disks detected: blocking error card replaces the disk list.

---

### Screen 4 — Recovery Setup

**Purpose:** Confirm the recovery system is included and give the user a single, meaningful choice.

This is the screen most installers omit. SLM surfaces it because recovery is a product promise, not a footnote.

**On screen:**
- Brief headline: "Your safety net is included."
- Three-line body: "SLM automatically creates a recovery partition. If something goes wrong, you can restore your system without reinstalling. This runs even when your desktop can't start."
- Single toggle: "Include factory snapshot" — on by default. Label: "Save a copy of the fresh system for future restoration." This is the only optional choice on this screen.
- Recovery partition size shown: "10 GB reserved"

**Not on screen:** Snapshot names, Btrfs subvolume layout, recovery kernel details.

**Primary action:** Toggle (optional) → Continue.

---

### Screen 5 — Summary & Confirmation

**Purpose:** Final checkpoint. The user sees everything that is about to happen, stated plainly. This is the last moment to go back.

**On screen:**
Two-column summary (label: value):

```
Disk         Samsung 970 EVO 1TB (/dev/nvme0n1)
Filesystem   Modern (snapshot-enabled)
Recovery     Included (10 GB)
Factory snapshot   Enabled
Language     English (United States)
Timezone     Auto-detected from network (can be changed after install)
```

"Advanced technical details" — collapsed disclosure, chevron-right, 11px. Expands to show: Btrfs, subvolume layout, systemd-boot, EFI partition size. This is for the curious user who wants to verify — not the default path.

Warning banner if any Screen 2 advisories were recorded: amber left-border card, plain copy.

**Destructive confirmation:** The Continue button reads "Install SLM Desktop" — not "Next", not "Continue". The label names the irreversible action. This follows macOS's convention for destructive confirmations: the verb is explicit. A secondary line below the button: "This will erase everything on the selected disk." 11px, `textTertiary`.

**Not on screen:** Back-navigation to disk (allowed via top-left back chevron, always present in non-progress screens), cancel button (always available).

---

### Screen 6 — Installation Progress

*(Addressed in full in Section 6.)*

---

### Screen 7 — Completion & Restart

**Purpose:** Signal unambiguous success. Short. No clutter.

**On screen:**
- A filled circle checkmark: 48px, `success` color, no animation (stillness communicates finality better than celebration here)
- Headline: "SLM Desktop is ready."
- Body (2 lines): "Remove your installation drive and restart. You'll set up your preferences when you sign in for the first time."
- Single primary button: "Restart Now"
- Secondary text link: "Quit Installer" (for scripted/enterprise deployments)

No "what's next" marketing copy. No feature list. The OOBE owns that conversation.

---

## 3. Information Architecture — OOBE (slm-welcome)

The OOBE is a full-screen application, not a windowed dialog. It occupies the entire screen with a rich background treatment (described in Section 4). Each pane slides in from the right, 320ms, spring physics (`physicsSpringGentle`).

### Dependency Graph & Flow

```
[Welcome Animation]
        │
        ▼
[Region & Keyboard] ─── skippable: yes, offline-OK: yes
        │
        ▼
[Accessibility]  ◄── front-loaded, NOT buried at the end
        │           skippable: yes, offline-OK: yes
        │           deferred-OK: yes (re-accessible from Settings)
        ▼
[Network] ─────────── skippable: YES (offline-capable desktop)
        │
        ├─── skipped ──► [Account] skipped ──► [Privacy] skipped
        │
        ▼ (if connected)
[Account / Sign-In] ── skippable: yes (local account always available)
        │
        ▼
[Privacy Choices] ──── skippable: no (must acknowledge, even to decline)
        │               offline-OK: yes
        ▼
[AI Assistant Intro] ── skippable: yes
        │               deferred-OK: yes (accessible from Settings > Assistant)
        ▼
[Nearby & Devices] ─── skippable: yes, requires network
        │               deferred-OK: yes (Settings > Nearby)
        ▼
[Theme & Wallpaper] ── skippable: yes, offline-OK: yes
        │
        ▼
[Tutorial / Tips] ──── skippable: yes (single "Skip" clears entire tutorial)
        │
        ▼
[Done / Arrive]
```

**Rules:**
- Skipping Network collapses Account and Nearby to "deferred" — they are not shown but a Settings reminder badge persists for 7 days.
- Privacy cannot be skipped outright but "Use default settings" is a valid single-tap completion.
- Accessibility is pane 3 (not pane 9) because a user who needs reduced motion or large text needs those settings *active during the rest of the OOBE*.
- The OOBE can be quit at any point by pressing Escape. A confirmation sheet appears: "You can always continue setup from Settings > About This Desktop." Desktop launches normally.

---

## 4. Visual Language

### Typography

**System font stack:** Open Sans (already `fontFamilyUi` and `fontFamilyDisplay` in Theme.qml). This is the correct call for Ubuntu 26.04 — it is available, renders cleanly at all sizes on both HiDPI and 96dpi, and carries modest personality without imposing a custom brand typeface that requires bundling.

**Installer type ramp** (conservative — the installer is a utility):

| Role | Token | Size | Weight | Tracking |
|---|---|---|---|---|
| Screen headline | `fontPxDisplay` | 28px | 700 | `letterSpacingDisplay` (-0.6) |
| Section label | `fontPxSubtitle` | 15px | 600 | `letterSpacingTitle3` (-0.2) |
| Body / description | `fontPxBody` | 14px | 400 | `letterSpacingNormal` (0) |
| Helper / secondary | `fontPxSm` | 12px | 400 | `letterSpacingWide` (+0.1) |
| Technical details | `fontPxXs` | 11px | 400 | `letterSpacingWider` (+0.2) |

**OOBE type ramp** (expressive — the OOBE is an experience):

| Role | Token | Size | Weight | Notes |
|---|---|---|---|---|
| Hero welcome | `fontPxJumbo` | 36px | 700 | Tracking -0.6. Only appears once. |
| Pane headline | `fontPxHero` | 22px | 700 | Per-pane title |
| Pane subhead | `fontPxTitle` | 16px | 600 | One-line elaboration |
| Body | `fontPxBody` | 14px | 400 | Descriptive paragraphs |
| Caption | `fontPxXs` | 11px | 400 | Legal / footnotes |

### Color

**Installer:** Near-monochromatic. The palette is `windowBg` (`#f5f5f7` / `#1a1a1c`), `textPrimary`, `textSecondary`, and `borderSubtle`. The only chromatic colors that appear are the three semantic tokens — success, warning, error — and the single accent used for the selected disk card border and the primary button fill. No gradients. No tinted surfaces.

**OOBE:** The OOBE background is a soft radial gradient built from the user's eventual accent color (defaulting to `accent` `#0a84ff`). On the welcome pane, this gradient is fully saturated and animated — it breathes slowly (scale 1.0→1.04→1.0, 8s loop, `easingStandard`). As the user progresses through practical panes (region, accessibility, privacy), the background settles to a quieter, desaturated version. The Theme & Wallpaper pane shows the selected wallpaper bleeding through as a live preview behind the card.

**Semantic tokens** (shared, from existing `Theme.qml`):

| Token | Value (light) | Installer use | OOBE use |
|---|---|---|---|
| `success` | `#28c840` | Hardware pass checkmark | Completion screen |
| `warning` | `#ff9f0a` | Advisory hardware item | Offline warning, skipped items |
| `error` | `#d14b4b` | Blocking hardware fail | Form validation |
| `accent` | `#0a84ff` | Primary button only | Pervasive — buttons, selection, gradient |

### Iconography

**Installer:** SF Symbols equivalent via system icon theme, or simple geometric shapes. Maximum 3 icon contexts: hardware check items (16px, stroke 1.5), disk type indicator (20px), and the completion checkmark (48px, filled). No decorative icons. Corner radius on all custom-drawn shapes: `radiusSm` (4px) for chips, `radiusControl` (8px) for cards.

**OOBE:** Richer. Each pane has a single 64px hero illustration — not a photograph, not a flat icon, but a simple isometric or perspective glyph that represents the concept (a globe for region, a ear for accessibility, a wifi arc for network). These are the moments where SLM's personality is visible. Weight: 1.5px stroke, filled accent where appropriate.

### Surfaces

**Installer:** One surface type — the centered card. Max width 560px, radius `radiusCard` (12px), background `surface` (`#ebecef` / `#2a2a2e`), `shadowMd`. The background behind the card is always `windowBg`. No blur, no vibrancy — the installer runs before the compositor is active.

**OOBE:** Three surface types:
1. **The pane card** — 640px wide, radius `radiusWindow` (14px), `popupSurfaceOpacity` (0.985) with backdrop blur 20px. This is the primary interaction surface.
2. **The background layer** — full-screen gradient, animated.
3. **Choice chips** — for wallpaper/theme selection: 80×80px thumbnails, `radiusCard` (12px), 2px selected border in `accent`.

### Motion

**Installer:** Motion exists in exactly two moments:
1. The hardware check rows entering (staggered, 120ms per row, `easingDecelerate`) — it shows the system is actively checking.
2. The partition diagram assembling on disk selection (280ms, `easingDecelerate`).

That is all. Every other transition in the installer is a 0ms cut or a `durationFast` (120ms) opacity crossfade. Motion in the installer communicates *process*, not personality.

**OOBE:** Motion is structural. Key moments:

- **Launch:** SLM wordmark fades in from 0→1 over 400ms (`easingDecelerate`), then scales 1.0→1.05 over 200ms, then the welcome headline rises from y+24 to y+0 over 320ms while fading in. Total: 920ms before the first interactive element appears.
- **Pane transition:** Current pane slides out to x-40px while fading to opacity 0 (220ms, `easingAccelerate`). Next pane enters from x+40px to x+0 at opacity 0→1 (280ms, `easingDecelerate`). Gap between exit and entry: 40ms. Net perceived duration: ~340ms.
- **Wallpaper preview:** When a wallpaper thumbnail is tapped, the background cross-dissolves to show it at full screen behind the card. Duration: `durationNormal` (220ms).
- **Done / Arrive:** The pane card scales from 1.0 to 0.9 and fades to 0 (260ms, `easingAccelerate`). The desktop reveals through — no additional transition needed, because the desktop itself becoming visible is the payoff.

Reduced motion (`animationMode === "reduced"` or `"minimal"`): All OOBE transitions collapse to `durationFast` opacity crossfades. The breathing background gradient stops. The welcome animation compresses from 920ms to 200ms.

---

## 5. Error & Recovery Surfaces

### Three Error Levels

**Inline error** — something on the current screen is wrong, but the user can fix it without leaving.
- Visual: 1px left-border in `error` color on the relevant card section. Error message appears below the field/control, 12px, `error` color. No icon needed — the color and position are sufficient.
- Copy structure: [What happened] + [What to do]. Never "Error:" prefix.

**Blocking error** — the user cannot proceed past this screen without resolving.
- Visual: The normal card is replaced by an error state card. Same container, different content. Top of card: a 20px circle with an exclamation, `error` fill. Headline. Body. Action.
- No red screen flash, no modal overlay — the card surface is stable and readable.

**Fatal error** — installation cannot continue and may have left the disk in an indeterminate state.
- Visual: As above, but includes a "What happened" disclosure (collapsed, chevron), which expands to show the structured log summary — never raw stderr.
- Always offers: "Quit Installer" and "Save Error Report."

### Error Copy Examples

**UEFI not detected (blocking):**
> **This computer cannot run SLM Desktop**
> SLM requires UEFI firmware to work correctly. This computer appears to use an older system.
> Check your computer's firmware settings, or contact your hardware manufacturer. If you recently changed firmware settings, restart and try again.
> [Quit Installer]

**Disk too small (blocking, inline):**
> **Not enough space on this disk**
> SLM Desktop needs at least 40 GB. This disk has 28 GB available.
> Select a different disk, or free up space on this one before installing.

**mkfs failure (fatal):**
> **The disk could not be prepared**
> SLM was unable to set up the filesystem on your selected disk. Your disk's contents are unchanged.
> This may indicate a hardware issue with the disk. Try a different disk, or run a disk health check from the troubleshooting menu.
> [Save Error Report]  [Quit Installer]

**Network unavailable (advisory, OOBE only):**
> **You're offline**
> No problem — SLM Desktop is fully installed. You can connect to a network later in Settings to download optional features.
> [Continue Offline]  [Try Again]

### Recovery & Safe Mode Visual Identity

Safe Mode must feel **calm and reduced**, not broken. The distinction: normal desktop uses the full visual language; Safe Mode uses a stripped, intentionally minimal variant.

Safe Mode indicators:
- The shell's top bar renders without blur/vibrancy — solid `#1d1d1f` (dark) or `#f0f0f0` (light), no translucency.
- A persistent amber indicator strip at the very bottom of the screen, 24px tall: "Safe Mode · Graphics effects disabled · [Exit Safe Mode]" — 11px, `warning` text on `#fff5e0` background.
- Window decorations: flat, no shadow, no radius animation. The chrome is functional, not expressive.

The goal: the user can see something is different without feeling the system is damaged. The amber strip is the only indicator — it does not repeat, does not animate, does not alarm.

---

## 6. Progress & Feedback

The installation progress screen is where the installer's character is most tested. Eight to fifteen minutes of user time that most installers fill with a scrolling log or a stuck progress bar.

**The stage model.** Installation is broken into five human-readable stages. Each stage has a headline (what is happening), a one-sentence description (why it matters), and a determinate sub-progress bar. The overall bar is indeterminate until Stage 2 completes (at which point remaining time can be estimated).

```
Stage 1 — Preparing your disk         (~45s)
Stage 2 — Installing SLM Desktop      (~6–10min, largest)
Stage 3 — Setting up recovery         (~90s)
Stage 4 — Configuring boot system     (~30s)
Stage 5 — Finishing up                (~30s)
```

**Visual treatment:**

- Active stage: headline 15px weight 600, description 13px `textSecondary`, sub-progress bar 4px tall, `accent` fill, `radiusPill`.
- Completed stages: headline `textSecondary`, checkmark (14px, `success`), no description. They shrink in height from 64px to 32px over 200ms when completing — this creates a natural visual rhythm as the list compresses upward.
- Upcoming stages: headline `textDisabled`, no description.

**What is never visible:** The log output. There is a "Show Details" link (11px, `textTertiary`, bottom of card). Clicking it opens a secondary panel that shows the structured log in a monospace view — for power users and support. Default state: collapsed.

**The empty 8–14 minutes:** Below the stage list, a quietly animated illustration. Not a video, not a feature carousel. A single ambient composition — think a simplified isometric rendering of the SLM desktop in blueprint style, with layers appearing as stages complete. The animation loops gently. It communicates: "something real is being assembled." Duration of the loop: 12 seconds, `easingStandard`. Reduced motion: still image only.

**Determinate vs indeterminate strategy:**
- Stage 1 and 4: indeterminate spinners (duration unknown, hardware-dependent).
- Stage 2: determinate, updated by Calamares progress signals. If progress stalls at a value for > 30s without movement, the bar pulses gently (opacity 0.7→1.0, 2s loop) to indicate activity without false precision.
- Stage 3 and 5: deterministic short tasks, shown as quickly-completing bars.

**The cancel question:** Installation cannot be safely cancelled after Stage 2 begins. Before Stage 2: a "Cancel" button (11px, `textTertiary`) is visible. After Stage 2 starts: it disappears. If the user presses Escape, a sheet appears: "Installation is in progress. Quitting now may leave your disk in an unusable state." with a single "Continue Installation" action — no destructive option offered.

---

## 7. Accessibility

### Reduced Motion
The OOBE checks `animationMode` before any animation starts. At `"reduced"`: all entry animations collapse to 120ms opacity fades, the background gradient is static, the welcome sequence compresses. At `"minimal"`: 0ms cuts throughout, only focus rings animate.

Accessibility is pane 3 in the OOBE — crucially, before Network and Account — because users who depend on these settings should have them active before encountering the more complex preference panes.

### Keyboard Navigation
Both flows are fully keyboard-navigable. Focus model:
- `Tab` advances through interactive elements in document order.
- `Space` or `Enter` activates the focused control.
- `Escape` triggers back-navigation (with confirmation on destructive screens).
- Arrow keys navigate within groups (disk list, language list, wallpaper chips).
- The primary action button is always reachable via `Return` from any state — a clear focus ring (`focusRingStrong`: `#0a84ff`, 2px, offset 2px, `radiusControl`) ensures it is visible.

No mouse traps. Every modal or sheet is dismissible by keyboard.

### Screen Reader Narrative (Installer)

Each screen announces itself on focus arrival. Suggested ARIA-equivalent QML `Accessible.name` strings:

- Welcome screen: "Welcome to SLM Desktop. Language selection."
- Hardware check, while running: "Checking hardware compatibility. Checking display…"
- Hardware check, complete: "Hardware check complete. All requirements met. Continue button available."
- Disk card: "Samsung 970 EVO 1TB, 931 gigabytes free, disk health good. Installing here will erase all data on this disk."
- Progress stage change: "Installing SLM Desktop. Stage two of five."

### High-Contrast & Large Text

Both modes must be available in OOBE pane 3 (Accessibility) and applied immediately — not queued for after setup. The OOBE must re-render itself within the same session when these are toggled.

High-contrast mode in the installer: border widths increase to `borderWidthThick` (2px), all `textSecondary` promotes to `textPrimary` weight, the semantic color fills darken by 15%. The installer reads `highContrast` from `Theme.qml` — which sources from `DesktopSettings` if available, falling back to false. For the installer context (pre-session), this means the Calamares QML host must inject the value before engine load, matching the pattern already used for `SafeModeActive`.

---

## 8. Component Inventory

| Component | Purpose | Key Props | Reuse from shell? |
|---|---|---|---|
| `InstallerCard` | Centered container for every installer screen | `maxWidth: int`, `headline: string` | No — new. The shell's `DSStyle.Card` assumes a compositor is running. This must be standalone. |
| `InstallerPrimaryButton` | Single primary action per screen | `label: string`, `enabled: bool`, `loading: bool` | Wrap `DSStyle.Button` with `loading` state added |
| `InstallerHardwareRow` | One hardware check item | `label`, `status` (checking/pass/warn/fail), `detail: string` | No — new |
| `InstallerDiskCard` | Selectable disk card with partition diagram | `diskName`, `diskSize`, `health`, `selected: bool` | No — new |
| `PartitionDiagram` | Animated 3-segment bar showing EFI / Root / Recovery | `efiSize`, `rootSize`, `recoverySize` | No — new |
| `InstallerProgressStage` | One stage in the progress flow | `title`, `description`, `state` (pending/active/complete), `progress: real` | No — new |
| `InstallerProgressScreen` | Full progress screen: stage list + ambient illustration | — | No — orchestrator component |
| `InstallerErrorCard` | Error state replacement for the normal card | `level` (inline/blocking/fatal), `headline`, `body`, `primaryAction`, `secondaryAction` | No — new |
| `OOBEPane` | Full-screen OOBE pane with slide transition | `title`, `subtitle`, `heroIcon`, `skippable: bool` | No — new |
| `OOBEWallpaperChip` | Thumbnail chip for wallpaper/theme selection | `source`, `selected: bool`, `label` | No — new |
| `OOBEProgressDots` | Pane position indicator | `total: int`, `current: int` | No — new |
| `SafeModeBanner` | Persistent amber strip for Safe Mode sessions | — | Inject into shell's root window via `Loader` when `SafeModeActive === true` |

**Integration note:** `SafeModeBanner` is the one component that must live inside the existing shell tree. All installer components are isolated to the Calamares QML context — they do not share the SlmStyle module path at install time. The OOBE components run inside the desktop session and should import `SlmStyle as DSStyle` normally.

---

## 9. Open Questions & Risks

**Decisions needed before mockups:**

1. **OOBE product name:** ✅ **RESOLVED** — committed to `slm-welcome`. This is the binary name, window title, and `.desktop` entry across all SLM components.

2. **Dark-mode first vs light-mode first:** The installer and OOBE should share a default mode decision. On first boot, `DesktopSettings` does not yet exist — the installer must pick a fallback. macOS ships light-by-default on hardware that does not report ambient light sensor data. Recommendation: follow the live environment's system preference (already available via `Theme.systemPrefersDark`). Risk: the live ISO may always boot into a specific mode.

3. **Custom brand typeface:** Open Sans is already in Theme.qml and is the right call for engineering speed. If a custom typeface (e.g., a variable-weight humanist sans) is planned for the SLM brand, it must be bundled in the live ISO and installer squashfs. This decision has distribution-size implications. Recommendation: defer until post-1.0, commit to Open Sans now.

4. **OOBE hero illustrations:** Are these designed in-house, sourced from an icon library, or AI-generated? The visual quality of the OOBE depends heavily on these 8–10 spot illustrations. This cannot be a last-minute decision — it shapes the entire tone. Recommendation: commission a minimal set of ~10 simple isometric glyphs, 64×64 SVG, before OOBE development begins.

5. **Calamares branding entry point:** Calamares supports a `branding` directory for QML overrides and a `viewmodule` for custom QML screens. Confirm which approach the backend agent intends to use — it determines how deeply the installer UI components integrate with Calamares's own navigation model.

6. **Accessibility pane timing:** Placing Accessibility at pane 3 (before Network) is a deliberate departure from most OOBE conventions. If stakeholders expect the conventional late placement, this needs explicit sign-off before the OOBE flow is built. The case for front-loading it is strong (see Section 7) but it is a meaningful product decision.

7. **Offline OOBE completion definition:** If the user skips Network and Account, is the OOBE "complete" at the Theme pane? Or does completion require at least acknowledging the Privacy pane? The Privacy pane being non-skippable (per Section 3) answers this — but the question is whether a fully offline user who dismisses OOBE early is considered to have a "complete" setup state or a "partial" one that triggers re-launch on next login.

---

*This plan is ready for mockup translation. The component inventory, type ramp, color tokens, and motion specifications are all grounded in existing `Theme.qml` values — no new tokens need to be invented for the installer. The OOBE requires three new tokens: `durationOOBEWelcome` (920ms scaled), `easingOOBEExit` (alias for `easingAccelerate`), and `oobePanelBlur` (20px) — these should be added to Theme.qml before OOBE development starts.*
