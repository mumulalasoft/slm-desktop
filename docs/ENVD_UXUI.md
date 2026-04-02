# UX/UI Design: Settings > Developer > Environment Variables

> Target audience: developers and advanced users
> Style: clean, minimal, advanced — consistent with desktop shell premium aesthetic
> Status: Design — not yet implemented

---

## Page Structure

```
Settings
└── Developer
    └── Environment Variables               ← this page
        ├── PageHeader + ChangesEffectBanner
        ├── SearchFilterBar
        ├── ScopeTabBar: [Session] [User] [System] [Per-App]
        ├── ScopeInfoBanner (per-tab contextual text)
        ├── EnvVarListView (filtered by active tab)
        ├── BulkActionBar (visible when rows selected)
        ├── AddVariableButton (pinned bottom-right)
        └── EffectiveEnvPreviewPanel (collapsible, right side)
```

---

## Layout

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  Environment Variables                                   [Preview env ▼]    │
│  ─────────────────────────────────────────────────────────────────────────  │
│  ⓘ Changes take effect for apps opened after saving. Running apps           │
│    are not affected.                                                [×]     │
│  ─────────────────────────────────────────────────────────────────────────  │
│  🔍 Search variables…              [Session] [User] [System] [Per-App]      │
│  ─────────────────────────────────────────────────────────────────────────  │
│  ⓘ  User variables — Saved permanently. Apply to all apps you launch.      │
│                                                                             │
│   ●  NAME                 VALUE                    SCOPE   MODIFIED  ···   │
│  ────────────────────────────────────────────────────────────────────────   │
│   ●  MY_TOOL_PATH         /home/user/.local/tools  User    2d ago    ···   │
│   ●  EDITOR               nvim                     User    1w ago    ···   │
│   ○  OLD_API_KEY           ••••••••••••••           User    3w ago    ···   │
│                                                                             │
│                                                      [+ Add Variable]       │
└─────────────────────────────────────────────────────────────────────────────┘
```

Two-panel variant when Preview is open:

```
┌──────────────────────────────────┬──────────────────────────────────────────┐
│  [Main list — narrower]          │  Effective Environment Preview           │
│                                  │  ─────────────────────────────────────   │
│  ●  MY_TOOL_PATH  /home/…        │  App: [Firefox              ▼]           │
│  ●  EDITOR        nvim           │                                          │
│  ○  OLD_API_KEY   ••••           │  KEY               VALUE       SOURCE    │
│                                  │  PATH              /usr/…      user      │
│                                  │  WAYLAND_DISPLAY   wayland-0   default   │
│                                  │  EDITOR            nvim        user      │
│                                  │  MOZ_ENABLE_…      1           per-app   │
│                                  │                                          │
│                                  │  [Copy]  [Export .env]                   │
└──────────────────────────────────┴──────────────────────────────────────────┘
```

---

## Component Inventory

| Component | Type | Notes |
|---|---|---|
| `PageHeader` | Text | "Environment Variables", DSStyle.H2 |
| `ChangesEffectBanner` | Info banner | Sticky top; dismissible per session |
| `SearchFilterBar` | Row: SearchField + ScopeChips | Real-time filter across all visible rows |
| `ScopeTabBar` | DSStyle.TabBar | Session / User / System / Per-App |
| `ScopeInfoBanner` | Contextual banner | Different text per tab; subtle; not alarming |
| `EnvVarListView` | ListView | Delegates: `EnvVarRow` |
| `EnvVarRow` | Rectangle h:44 | Toggle + Key + Value + Scope badge + Modified + Actions |
| `BulkActionBar` | BottomBar | Appears when ≥1 row selected |
| `AddVariableButton` | DSStyle.Button | Pinned bottom-right of list |
| `EffectiveEnvPreviewPanel` | Collapsible right drawer | App picker + table |
| `AddEditEnvVarDialog` | Modal dialog | New variable or edit existing |
| `SensitiveVarWarningDialog` | Modal dialog | PATH, LD_LIBRARY_PATH, etc. |
| `PerAppOverridesView` | Accordion list | Per-app tab content |
| `SessionResetButton` | Destructive button | Session tab header only |

---

## EnvVarRow — Column Specification

| Column | Width | Content | Notes |
|---|---|---|---|
| Select checkbox | 20 px | Appears on hover / when any row selected | Hidden by default |
| Toggle | 36 px | DSStyle.Toggle (enabled/disabled) | Inline — no dialog |
| Key | 220 px | Monospace; truncated | Click → edit dialog |
| Value | flex | Monospace; truncated; masked if sensitive | Click → edit dialog |
| Scope badge | 80 px | "Session" / "User" / "System" / "App" | Color-coded |
| Modified | 90 px | Relative ("2d ago"); hover = ISO 8601 | Color: textDisabled |
| Actions | 64 px | Edit ✏ · Delete 🗑; visible on hover or focused | |

**Scope badge colors:**
- Session → accent blue
- User → green (`#4CAF50` or theme token `scopeUser`)
- System → amber (`#FF9800` or `scopeSystem`)
- Per-App → purple (`#9C27B0` or `scopeApp`)

**Sensitive row:** if key is in risky-variable list, show a subtle amber left border (2 px). No alarming red — just a quiet indicator.

**Disabled row:** full row at 50% opacity; toggle shows off state.

**Value masking:** for keys matching `*KEY*`, `*SECRET*`, `*TOKEN*`, `*PASSWORD*` — mask value as `••••••••` by default; [show] icon on hover.

---

## Add / Edit Variable Dialog

```
┌────────────────────────────────────────────────────┐
│  Add Environment Variable                          │
│                                                    │
│  Key                                               │
│  ┌──────────────────────────────────────────────┐  │
│  │ MY_VARIABLE                                  │  │
│  └──────────────────────────────────────────────┘  │
│  Only A–Z, 0–9, underscore. No spaces.             │
│                                                    │
│  Value                                             │
│  ┌──────────────────────────────────────────────┐  │
│  │ /usr/local/bin                               │  │
│  └──────────────────────────────────────────────┘  │
│                                                    │
│  For PATH-like variables:                          │
│  Merge  ● Replace   ○ Prepend   ○ Append           │
│  (only shown when key ends in PATH or is known)    │
│                                                    │
│  Scope                                             │
│  ○ Session  ● User  ○ System                       │
│  System requires administrator permission.          │
│                                                    │
│  Comment  (optional)                               │
│  ┌──────────────────────────────────────────────┐  │
│  │                                              │  │
│  └──────────────────────────────────────────────┘  │
│                                                    │
│              [Cancel]          [Add Variable]      │
└────────────────────────────────────────────────────┘
```

**Validation (real-time):**
- Key: red underline if contains lowercase, spaces, or starts with digit
- Key: amber warning icon if matches sensitive list (PATH etc.)
- Value: green checkmark if a filesystem path and it exists
- Value: amber warning if path does not exist (for PATH entries)
- Confirm button disabled until key is valid

**Edit mode:** title changes to "Edit Variable". Key field is readonly (cannot rename; must delete + re-add). Value, scope, comment, and merge mode are editable.

---

## Sensitive Variable Warning Dialog

Triggered for: `PATH`, `LD_LIBRARY_PATH`, `DISPLAY`, `WAYLAND_DISPLAY`, `DBUS_SESSION_BUS_ADDRESS`, `QT_PLUGIN_PATH`, `LD_PRELOAD`

```
┌───────────────────────────────────────────────────────┐
│  ⚠  Modifying a critical system variable              │
│                                                       │
│  You are about to change PATH.                        │
│                                                       │
│  PATH controls where the system finds executables.    │
│  An incorrect value may prevent applications from     │
│  launching, including this settings app.              │
│                                                       │
│  Current value                                        │
│  ┌─────────────────────────────────────────────────┐  │
│  │ /usr/local/bin:/usr/bin:/bin                    │  │
│  └─────────────────────────────────────────────────┘  │
│                                                       │
│  New value                                            │
│  ┌─────────────────────────────────────────────────┐  │
│  │ /home/user/.tools:/usr/local/bin:/usr/bin:/bin  │  │
│  └─────────────────────────────────────────────────┘  │
│                                                       │
│  Changes                                              │
│  + /home/user/.tools                                  │
│                                                       │
│  ℹ  This change applies to apps launched after        │
│     saving. Already-running apps are not affected.    │
│                                                       │
│       [Cancel]              [I understand, apply]     │
└───────────────────────────────────────────────────────┘
```

**Severity levels and behavior:**

| Severity | Variables | Behavior |
|---|---|---|
| `critical` | PATH, LD_LIBRARY_PATH, DBUS_SESSION_BUS_ADDRESS | Dialog required; diff view required |
| `high` | WAYLAND_DISPLAY, DISPLAY, QT_PLUGIN_PATH | Dialog required; no mandatory diff |
| `medium` | LD_PRELOAD, PYTHONPATH | Inline amber warning badge; no dialog |

---

## Per-App Overrides Tab

```
┌──────────────────────────────────────────────────────────────────────┐
│  Per-App Overrides                                                   │
│                                                                      │
│  Set variables that apply only to a specific app. These override     │
│  user and session variables for that app only.                       │
│                                                                      │
│  ┌────────────────────────────┐            [+ Add Override for App]  │
│  │ 🔍 Search apps…            │                                      │
│  └────────────────────────────┘                                      │
│                                                                      │
│  ▼ Firefox                                              [+ Add] [⋯]  │
│    org.mozilla.firefox                                               │
│    ┌─────────────────────────────────────────────────────────────┐   │
│    │  ●  MOZ_ENABLE_WAYLAND      1                               │   │
│    │  ●  GTK_THEME               Adwaita:dark                    │   │
│    └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ▶ VSCodium                                             [+ Add] [⋯]  │
│    com.vscodium.codium                                               │
│                                                                      │
│  ▶ Terminal                                             [+ Add] [⋯]  │
│    org.slm.Terminal                                                  │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

- App entries are accordion/expander items
- App picker (for "Add Override for App") reuses the same app chooser component used in open-with
- App IDs sourced from installed `.desktop` files
- Remove entire app: [⋯] → "Remove all overrides for this app"
- No system scope in per-app overrides (per-app is always user-level)

---

## Effective Environment Preview

```
┌──────────────────────────────────────────────────────────────┐
│  Effective Environment Preview                        [×]    │
│                                                              │
│  What environment will this app see when launched now?       │
│                                                              │
│  Application                                                 │
│  ┌───────────────────────────────────────────────────────┐   │
│  │  Firefox                                          ▼   │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                              │
│  🔍 Filter variables…           [All] [User] [App] [Default] │
│                                                              │
│  ┌───────────────────────────────────────────────────────┐   │
│  │  KEY                   VALUE               SOURCE     │   │
│  │  ─────────────────────────────────────────────────    │   │
│  │  DBUS_SESSION_…        unix:path=…         default    │   │
│  │  EDITOR                nvim                user    ●  │   │
│  │  MOZ_ENABLE_WAYLAND    1                   per-app ●  │   │
│  │  PATH                  /home/user/…        user    ●  │   │
│  │  WAYLAND_DISPLAY       wayland-0           default    │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                              │
│  [Copy all to clipboard]         [Export as .env file]       │
│                                                              │
│  33 variables total · 5 from your config · 28 inherited      │
└──────────────────────────────────────────────────────────────┘
```

- SOURCE column uses same badge colors as main table
- Dot `●` next to source = this var was set by you (non-default)
- Filter chips narrow the list by source
- "Copy" puts `KEY=VALUE` pairs, one per line, to clipboard
- "Export" saves a `.env` file (useful for debugging, passing to Docker, etc.)
- App picker is a searchable dropdown of installed apps

---

## Empty, Error, and Loading States

### Empty state (no variables in this scope)

```
          [Icon: terminal / variable icon]

          No variables in User scope.

          Variables you add here will be available
          to all applications you launch.

                   [+ Add Variable]
```

### Loading state

- Show 3 skeleton rows (ghost rectangles) while D-Bus call is in flight
- Spinner in page header subtitle area: "Loading…"
- Skeleton fades in after 200 ms (avoid flash for fast loads)

### Error: service not running

```
          [Icon: warning outline]

          Environment service is unavailable.

          slm-envd is not responding. Some features
          may not work correctly.

               [Retry]     [Open Service Manager…]
```

### Error: polkit denied

Inline, on the row or dialog:
```
  ⊘  Permission denied — system scope requires administrator access.
```

No full-page error for polkit; it is scoped to the action.

### Error: write failed (file permission, disk full)

Inline toast (red, auto-dismiss 8s):
```
  ⚠  Could not save variable. Check disk space or file permissions.
```

### Success states

Toast (bottom-center, auto-dismiss 4s):
- After add: `Variable saved. New apps will use the updated environment.`
- After delete: `Variable removed. Running apps are not affected.`
- After session reset: `Session overrides cleared.`
- After system write: `System variable saved. Changes apply to new apps for all users.`

---

## Micro-copywriting Reference

### Scope Info Banners

**Session tab:**
> Changes apply until you log out. They are not saved to disk and do not affect running apps.

**User tab:**
> Applies to all applications you launch. Saved permanently to your profile.

**System tab:**
> Applies to all users on this machine. Requires administrator permission. Use with care.

**Per-App tab:**
> Overrides for a specific application. Takes priority over user and session variables.

### Change Effect Banner (always visible at top)

> Changes take effect for applications opened after saving. Running apps are not affected.

### Tooltips

| Element | Tooltip |
|---|---|
| Disabled toggle | "This variable is defined but not applied" |
| Scope badge (hover) | "Source: ~/.config/slm/environment.d/user.json" |
| Sensitive key warning icon | "Modifying this variable may affect system stability" |
| Modified time | Full ISO 8601 timestamp |
| Value masked | "Value hidden — hover and click to reveal" |
| Merge mode: prepend | "New value will be added before the lower-priority value, separated by :" |
| Session ResetButton | "Remove all session-scoped variables immediately" |

### Dialog Helper Text

**Add/Edit dialog — scope = System:**
> System scope requires administrator permission when you save.

**Add/Edit dialog — merge mode field:**
> Prepend or append lets you extend PATH-like variables instead of replacing them.

**Sensitive dialog — PATH:**
> PATH controls where the system finds executables. An incorrect value may prevent applications from launching, including this settings app.

**Sensitive dialog — LD_LIBRARY_PATH:**
> This variable controls shared library lookup. Incorrect values can cause application crashes or security risks.

**Sensitive dialog — DBUS_SESSION_BUS_ADDRESS:**
> Changing this may break inter-process communication for your entire desktop session. Only modify if you know what you are doing.

---

## Interaction Details

### Inline edit vs dialog

| Action | Interaction |
|---|---|
| Enable / disable variable | Inline toggle — no dialog |
| Edit non-sensitive value | Click row → opens dialog (not inline; avoids focus/keyboard complexity) |
| Edit sensitive variable (PATH etc.) | Dialog → then SensitiveVarWarningDialog |
| Add new variable | Always dialog |
| Delete variable | Inline × button → small confirm popover ("Delete MY_VAR? [Cancel] [Delete]") |
| Rename variable | Not supported — delete + re-add (keys are identifiers) |

### Filter by scope

- Primary: Scope tabs (Session / User / System / Per-App)
- Secondary: when on All view (future), chips: [All] [Session] [User] [System] [App]
- Active filter persists per session (localStorage/QSettings)

### Search

- `SearchField` filters on key name and value simultaneously
- Match highlighted inline (key substring highlighted in amber)
- Searches across current scope tab only
- Empty state shown if no results: `No variables matching "xyz".`
- Clear [×] button in field

### Bulk disable / enable

1. Hover any row → checkbox appears
2. Click checkbox → `BulkActionBar` slides in from bottom:
   ```
   3 selected  ·  [Enable all]  [Disable all]  [Delete]  [×  Clear selection]
   ```
3. Bulk actions do not cross scope tabs (can only bulk-act on visible scope)
4. System scope entries require polkit for bulk disable/delete (prompt once per batch)

### Reset session overrides

- Button appears in Session tab header, right side: `Clear session overrides`
- Confirm popover: `This will remove all session variables. Running apps are not affected. [Cancel] [Clear]`
- After confirm: list empties, toast shown

---

## Visual Hierarchy (QML/Qt Quick)

### Component Tree

```qml
EnvVariablesPage {         // Item, anchors.fill: parent
  PageHeader { }           // DSStyle.H2 + subtitle
  ChangesEffectBanner { }  // DSStyle.InfoBanner, sticky, dismissible

  Column {
    SearchFilterBar {
      DSStyle.SearchField { }
      // scope filter chips only shown in All view (future)
    }

    ScopeTabBar {          // DSStyle.TabBar
      // Session / User / System / Per-App
    }

    ScopeInfoBanner { }    // DSStyle.SubtleBanner, changes per tab

    StackLayout {          // currentIndex: scopeTabBar.currentIndex
      EnvVarListView { }   // session scope
      EnvVarListView { }   // user scope
      EnvVarListView { }   // system scope
      PerAppOverridesView { }
    }
  }

  BulkActionBar {          // anchors.bottom, visible: selectionModel.hasSelection
    DSStyle.Label { }      // "N selected"
    DSStyle.Button { }     // Enable all
    DSStyle.Button { }     // Disable all
    DSStyle.DestructiveButton { } // Delete
  }

  DSStyle.Button {         // "+ Add Variable", anchors.bottom+right
    anchors.bottom: parent.bottom
    anchors.right: parent.right
  }

  EffectiveEnvPreviewPanel { // Drawer, slides in from right
    visible: previewOpen
  }
}

EnvVarRow {                // Rectangle, height: 44
  CheckBox { }             // visible: anySelected || hovered
  DSStyle.Toggle { }       // enabled/disabled
  DSStyle.MonoLabel { }    // key — truncated
  DSStyle.MonoLabel { }    // value — truncated, masked if sensitive
  ScopeBadge { }           // DSStyle.Badge, color by scope
  DSStyle.Label { }        // modified time, color: textDisabled
  RowActions {             // visible: hovered || activeFocus
    DSStyle.IconButton { } // edit
    DSStyle.IconButton { } // delete
  }
}
```

### Typography & Spacing

- Key + Value columns: `font.family: Theme.fontMono`, `font.pixelSize: Theme.fontSize("sm")`
- Scope badge: `font.pixelSize: Theme.fontSize("xs")`, uppercase, letter-spacing: 0.5
- Row height: 44 px (comfortable click target; consistent with other list rows in app)
- Section header (Per-App expander): 36 px row, slightly heavier weight

### Color Usage

| Token | Usage |
|---|---|
| `textPrimary` | Key name |
| `textSecondary` | Value |
| `textDisabled` | Modified time; disabled row content |
| `scopeSession` | Session badge background |
| `scopeUser` | User badge background |
| `scopeSystem` | System badge background (amber) |
| `scopeApp` | Per-App badge background (purple) |
| `accentText` | Highlighted search match |
| `destructive` | Delete button, destructive actions |
| `warningBorder` | 2 px left border on sensitive-variable rows |

### Animation

- Row hover: background fade 120 ms
- BulkActionBar: `y` slide-in from +height, 200 ms ease-out
- PreviewPanel: `x` slide-in from +width, 220 ms ease-out
- Toast: opacity fade-in 150 ms, auto-dismiss after 4 s with fade-out 300 ms
- Skeleton rows: opacity pulse 1.2 s infinite while loading
