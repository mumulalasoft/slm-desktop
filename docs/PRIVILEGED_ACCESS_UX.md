# Privileged Access UX — SLM Files
**Version:** 0.1 · **Status:** Design Specification  
**Author:** Senior Designer (Apple HI methodology)  
**Scope:** File manager front-end only. Backend D-Bus daemon (`slm-fsd`) is specified separately.

---

## 1. Design Philosophy

The traditional Linux privileged-access model is a context collapse: you're "normal user", then suddenly "root", and the system communicates this shift through alarm signals — red terminals, skull icons, `[sudo] password for user:`, phrasing that implies you are about to break something. This is not how calm, professional software handles authority.

The reference model is Finder on macOS, plus the iOS pattern for accessing Health or Location data: the system says, matter-of-factly, "this action needs a little more authority — here's how to proceed," and when you grant it, the UI simply continues. No mode change. No ominous chrome. The Finder doesn't turn red when you authenticate to modify a system volume; it just unlocks and lets you proceed. SLM Files follows the same philosophy: elevation is a capability state on a tab, not a personality change on the application. The user should feel *assisted*, not *interrogated*.

---

## 2. Visual Lexicon

All tokens come from `Theme.qml` via `Theme.color("…")`, `Theme.fontSize("…")`, and `Theme.fontWeight("…")`. Where a required token is absent, this section flags a **Design System Gap** with a proposed name and value.

### 2.1 Shield / Lock Icon

The "privilege indicator" uses a shield or lock glyph from the system icon set (`image://themeicon/`). Two states exist:

| State | Glyph source | Opacity | Notes |
|---|---|---|---|
| Protected (locked) | `security-medium-symbolic` or `changes-prevent-symbolic` | `Theme.opacityIconMuted` (~0.45) | Quiet. Not alarming. |
| Elevated (unlocked) | `security-high-symbolic` or `changes-allow-symbolic` | `0.72` | Slightly more present but not bright. |
| Relocking / fading | Animated from `0.72` → `Theme.opacityIconMuted` | Animated | 480 ms `easingDecelerate` |

- **Icon size in tab strip:** 12 × 12 px (inside a 16 × 16 hit target)
- **Icon size in breadcrumb:** 10 × 10 px (inline, optical baseline alignment)
- **Icon size in sidebar badge:** 10 × 10 px, positioned at bottom-right corner of the folder icon, offset −2/−2 px
- **Icon size in footer pill:** 10 × 10 px

**Design System Gap — `iconSizeTabIndicator`:** No token exists for 12 px inline tab icons. Propose: `Theme.iconSizeTabIndicator = 12`. Until added, hardcode only inside `FileManagerTabLockIndicator.qml` with a comment marking it for promotion.

### 2.2 Lock Indicator (tab strip)

When a tab is in the `Locked` state (current path is inside a protected prefix and no elevation has been granted):

- A lock glyph appears at the trailing-right edge of the tab pill, inside the existing tab `Rectangle`, to the left of the close button.
- Color: `Theme.color("textSecondary")` at `Theme.opacityIconMuted`
- No colored background pill — it is literally just the icon. The tab itself does not change color.

When `Unlocked`:

- The lock glyph morphs (via `Behavior on source` + opacity cross-fade over `Theme.durationSm`) to an open-lock / shield glyph.
- Color: `Theme.color("accent")` at `0.72` opacity — the only accent-colored element in the tab strip. It should read as "active" without shouting.

### 2.3 Protected-Path Badge in Sidebar

On a sidebar `FileManagerFavoriteItem` or storage row whose path falls under a protected prefix (`/etc`, `/usr`, `/var`, etc.):

- A 10 × 10 px `Image` positioned `anchors.bottom: parent.bottom; anchors.right: parent.right` relative to the folder icon item, with offsets `bottomMargin: -2; rightMargin: -2`. This is a badge overlay — not a separate list item column.
- Source: `security-medium-symbolic`
- Opacity: `Theme.opacitySubtle` (i.e. ~0.38–0.45, very quiet)
- No background pill. The badge should be discoverable, not decorative.

**Design System Gap — `color("protectedBadge")`:** No semantic color token exists for protected-state UI chrome. Propose:
- Light mode: `rgba(0, 0, 0, 0.42)` for the icon tint  
- Dark mode: `rgba(255, 255, 255, 0.38)` for the icon tint  
Named `protectedBadgeTint`. Add to `Theme.qml` under the Generic area-token family.

### 2.4 Protected-Path Banner Background, Border, and Text

The `FileManagerProtectedBanner` uses these tokens:

| Role | Token |
|---|---|
| Background | `Theme.color("windowCard")` at `Theme.cardSurfaceOpacity` |
| Border | `Theme.color("windowCardBorder")` at `Theme.borderWidthThin` |
| Icon tint (locked) | `Theme.color("textSecondary")` |
| Title text | `Theme.color("textPrimary")`, `Theme.fontSize("body")`, `fontWeight("medium")` |
| Body text | `Theme.color("textSecondary")`, `Theme.fontSize("small")`, `fontWeight("normal")` |
| Primary button | `DSStyle.Button` with `highlighted: true` (uses `accent` fill) |
| Secondary button | `DSStyle.Button` default style |
| Dismiss area | No explicit dismiss button; banner auto-dismisses on navigation away from the protected tree |

---

## 3. Tab Elevation State Machine

```
Locked ──[user attempts mutation]──► Authenticating ──[success]──► Unlocked
  ▲                                        │                           │
  │                                   [cancel/fail]              [10 min timer]
  │                                        │                           │
  └────────────────────────────────────────┘                       Relocking
                                                                        │
                                                               [animation done]
                                                                        │
                                                                     Locked
```

### 3.1 `Locked` State

**Tab strip:** Lock icon (12 px, `textSecondary` at `opacityIconMuted`) appears at trailing end of tab text area, to the left of the close button. Tab background, text color: no change from normal.

**Sidebar:** Protected folder icon shows the 10 × 10 badge.

**Toolbar / actions:** Read actions (open, preview, copy-from, properties) fully enabled. Write actions (rename, delete, create, paste-into, move-into) render at `opacityFaint` but remain visible; clicking them triggers the auth flow rather than being disabled. Do NOT use `enabled: false` which hides affordance — use a checked state that routes to auth.

**Breadcrumb:** Lock icon (10 px) appears immediately after the path component that is the protected root (e.g., the `etc` crumb has a lock badge). Icon color: `textSecondary`.

**Animation entering Locked from Relocking:** 480 ms `easingDecelerate`. Unlocked-shield fades to locked-lock via opacity 0.72 → `opacityIconMuted`. No layout shift.

### 3.2 `Authenticating` State

**Tab strip:** The lock icon pulses gently — `opacity` oscillates 0.45 → 0.70 → 0.45, period 1.2 s, `SequentialAnimation` with `easingDefault`. This is the only motion on the tab. It communicates "in progress" without spinning or alarming.

**Toolbar:** Write actions stay at `opacityFaint` (not restored yet). The action that triggered auth shows a small spinner replacing its icon, same pattern as `okButton` spinner in `AuthDialog.qml`.

**Polkit dialog:** The existing `slm-polkit-agent` `AuthDialog.qml` appears centered relative to the `FileManagerWindow` root rectangle (not the screen), using `x: Math.round((hostRoot.width - width) * 0.5)` and `y: Math.round((hostRoot.height - height) * 0.5)` — identical to `FileManagerRenameDialog` and `FileManagerClearRecentsDialog` positioning. The dialog's existing animation (`showAnim`: scale 0.97 → 1.0, opacity 0 → 1, `Theme.durationSm` + `Theme.durationMd`) is unchanged.

**The slm-polkit-agent `infoMessage` field** should be populated by the SLM Files caller with the context item name, e.g.: "`/etc/hosts` requires administrator password." This shows in the existing blue info banner inside `AuthDialog.qml`.

### 3.3 `Unlocked` State

**Tab strip:** Lock icon transitions to unlocked-shield glyph in `Theme.color("accent")` at 0.72 opacity. Transition: 200 ms `easingDefault` opacity cross-fade + icon swap (achieved by showing/hiding two `Image` items, not `source` binding which cannot animate).

**Sidebar:** Badge changes from locked icon to unlocked-shield at same 10 px size, same opacity pattern.

**Toolbar / actions:** All write actions restore to full opacity (`1.0`), 160 ms `easingDefault`. No "unlocked" toast or banner — the icon change is the only feedback. Trust the user to see it.

**Footer:** Elevation pill appears in `FileManagerFooterBar` (to the right of the item-count label, before any share pill). See Section 8 and Section 9.

**Breadcrumb:** Lock icon swaps to unlocked-shield, same color as tab: `accent` at 0.72.

### 3.4 `Relocking` State

Triggered by timer expiry or explicit "Lock" action. Duration: 480 ms `easingDecelerate`.

- Unlocked-shield on tab fades back to locked-lock icon (`accent` 0.72 → `textSecondary` `opacityIconMuted`)
- Footer elevation pill fades out (opacity 1.0 → 0, 320 ms `easingAccelerate`)
- Toolbar write actions return to `opacityFaint`
- If a file operation was in progress when relock fires: the operation completes (the token is valid until the current kernel call chain finishes). The tab transitions to `Locked` only after the in-flight operation resolves. If the user tries another write action after relock, they are returned to the `Authenticating` state silently (no "session expired" dialog — just the auth dialog again, with `infoMessage` saying the previous session ended).

---

## 4. Protected-Folder Entry Banner

Shown when a tab navigates *into* a path that falls under a protected prefix. It appears between the tab bar and the content view, inside `FileManagerMainPane`.

### 4.1 Layout

```
┌───────────────────────────────────────────────────────────────────────────────┐
│  [🔒 12px]  Administrator content                    [View Only]  [Authenticate…]  │
│             Changes to this folder require your password.                     │
└───────────────────────────────────────────────────────────────────────────────┘
```

- **Height:** `implicitHeight` driven by content + `Theme.spacingMd` top/bottom padding. Minimum ~52 px.
- **Left margin:** `Theme.spacingMd` for icon, `Theme.spacingXs` gap between icon and title.
- **Right margin:** Buttons right-aligned with `Theme.spacingMd` from edge.
- **Horizontal rule separator:** A 1 px line (`Theme.opacitySeparator` opacity) at the bottom edge of the banner, above the content view.
- **Radius:** `Theme.radiusMd` on the banner `Rectangle` itself; sits inside the content column with `Layout.leftMargin` / `Layout.rightMargin` of `Theme.spacingMd` giving it breathing room from the window walls.
- **No close/dismiss button.** The banner auto-dismisses when the tab navigates out of the protected tree. It also dismisses when elevation is granted.

### 4.2 Copy

| String key | English | Indonesian |
|---|---|---|
| `banner_title` | "Administrator content" | "Konten administrator" |
| `banner_body` | "Changes to this folder require your password." | "Perubahan pada folder ini memerlukan kata sandi Anda." |
| `btn_view_only` | "View Only" | "Lihat Saja" |
| `btn_authenticate` | "Authenticate…" | "Autentikasi…" |
| `btn_mount_readonly` | "Mount Read-Only" | "Pasang Hanya-Baca" |

"View Only" is the **default focus** target (`KeyNavigation` should land here first, Tab moves to "Authenticate…"). Clicking "View Only" dismisses the banner and leaves the tab in `Locked` state — browse access continues unchanged.

"Authenticate…" triggers the inline auth flow (see Section 5). It is styled as a `DSStyle.Button` with `highlighted: true`.

"Mount Read-Only" appears only when the current path is a mountable volume that supports a read-only mount option. It is a secondary `DSStyle.Button`, placed after "Authenticate…" in the layout.

### 4.3 Dismissal Behavior

- Navigate away from the protected tree: banner exits with 200 ms `easingAccelerate` (height collapses + opacity fades).
- Authentication succeeds: banner exits with same transition.
- "View Only" clicked: banner exits immediately, same transition.
- Navigate deeper within the same protected tree: banner persists. It is path-prefix scoped, not path-exact.

---

## 5. Inline Authentication Flow

### 5.1 Pre-Auth Visual State (Locked → Authenticating)

When the user clicks a write-action button (e.g., rename, delete, new folder) while in `Locked` state:

1. The action's toolbar icon briefly scales down and back (95% → 100%, 120 ms `easingDefault`) — a minimal haptic-analog feedback.
2. The tab lock icon begins pulsing (see Section 3.2).
3. The `AuthDialog.qml` window appears, centered on the file manager window, with `infoMessage` populated: e.g., `"Renaming '/etc/hosts' requires your password."` / `"Mengganti nama '/etc/hosts' memerlukan kata sandi Anda."`
4. The file manager content remains fully visible behind the dialog. No scrim, no dimming of the background.

### 5.2 Dialog Placement

The polkit `AuthDialog.qml` is an `ApplicationModal` `Window`. Its `x` and `y` must be set to position it relative to the file manager window's screen position. The caller should set:

```js
// In FileManagerWindow.qml when triggering privileged action:
if (authDialogController) {
    var win = hostWindow
    authDialogController.infoMessage = qsTr("Renaming '%1' requires your password.").arg(contextEntryName)
    // Position is handled internally by slm-polkit-agent via screen-center
    // or: request the agent to center relative to our window rect
}
```

The existing `AuthDialog.qml` uses `Window` with no explicit screen position; the window manager centers it. The SLM Files caller can optionally request the polkit agent to target the file manager's screen geometry via a D-Bus call parameter (backend architect should expose this as an optional `windowId` hint in the polkit agent's activation interface).

### 5.3 Post-Success Transition

On authentication success:

1. `AuthDialog.qml` runs its `hideAnim` (scale + opacity out, `Theme.durationFast`).
2. The tab transitions `Authenticating → Unlocked` (Section 3.3): icon swaps + accent color fades in.
3. The originally-triggered write action executes immediately — no extra confirmation, no "you are now admin" banner. The action just proceeds.
4. If the action was a rename: the existing `FileManagerRenameDialog` opens as normal, now without obstruction.
5. If the action was a delete: the `FileManagerSoftConfirmDialog` (Section 7) appears.

### 5.4 Failure / Cancel Transition

On authentication cancel or failure:

1. `AuthDialog.qml`'s shake animation plays for wrong password (already implemented). On cancel: normal `hideAnim`.
2. Tab returns to `Locked` state: pulse stops, icon returns to static locked-lock at `opacityIconMuted`.
3. No error toast, no banner. The UI returns exactly to where the user was before clicking the action.
4. **Specifically no:** errno codes, "Authentication failed" popups, "Access denied" notifications. The UI simply did not unlock. The user understands. Trust them.

---

## 6. Error Message Rewrites

All file operation errors from the backend should pass through a translation layer before display. No raw errno or system error strings should reach the user.

Error messages appear in the existing `FileManagerRenameDialog` error label (`Theme.color("error")`, `Theme.fontSize("small")`) or, for non-rename operations, as a subtitle in the relevant dialog or as a footer-bar flash (200 ms fade-in, 3 s hold, 400 ms fade-out — using the existing `FileManagerFooterBar` left label with a color shift to `Theme.color("error")` and back).

| errno | English | Indonesian |
|---|---|---|
| `EACCES` | "You need administrator permission to modify this item." | "Anda perlu izin administrator untuk mengubah item ini." |
| `EROFS` | "This location is read-only on this system." | "Lokasi ini bersifat read-only pada sistem ini." |
| `EBUSY` | "This item is in use by another app." | "Item ini sedang digunakan oleh aplikasi lain." |
| `ENOSPC` | "There isn't enough storage space to complete this." | "Tidak cukup ruang penyimpanan untuk menyelesaikan ini." |
| `EDQUOT` | "You've reached your storage quota for this volume." | "Anda telah mencapai kuota penyimpanan untuk volume ini." |
| `ELOOP` | "A symbolic link loop was detected." | "Terdeteksi loop tautan simbolik." |
| `ENAMETOOLONG` | "That name is too long for this file system." | "Nama tersebut terlalu panjang untuk sistem file ini." |
| `ETXTBSY` | "This file is open in another app and can't be modified right now." | "File ini terbuka di aplikasi lain dan tidak dapat diubah saat ini." |
| `EXDEV` | "Items can't be moved between different volumes in one step. A copy will be made instead." | "Item tidak dapat dipindahkan antar volume yang berbeda sekaligus. Salinan akan dibuat sebagai gantinya." |
| `EPERM` | "You don't have permission to do that here." | "Anda tidak memiliki izin untuk melakukan itu di sini." |
| `ENOENT` | "The item no longer exists at this location." | "Item tidak lagi ada di lokasi ini." |
| `EEXIST` | "An item with that name already exists here." | "Item dengan nama tersebut sudah ada di sini." |
| Generic/unknown | "Something went wrong. Try again." | "Terjadi kesalahan. Coba lagi." |

Rules:
- Never show `errno=N`.
- Never show "fatal", "error code", "operation failed with", or "unexpected".
- Never start a message with the path — the user can see it. Start with what they can do or what happened.
- EXDEV is the one case where the message explains and proposes a resolution. Keep it short.

---

## 7. Destructive-Action Soft-Confirm

When the user (already `Unlocked`) attempts to delete or overwrite an item under a protected path:

### 7.1 Dialog

Uses the `AppDialog` pattern from `FileManagerClearRecentsDialog.qml` and `FileManagerRenameDialog.qml`.

```
┌─────────────────────────────────────────────────────┐
│  Delete "/etc/cron.d/backup"?                       │
│                                                     │
│  A snapshot of the system configuration will be     │
│  saved automatically before this change.            │
│                                                     │
│  [ Cancel ]                        [ Delete ]       │
└─────────────────────────────────────────────────────┘
```

- `dialogWidth: 460`
- Title: `Theme.fontSize("title")`, `fontWeight("semibold")`
- Body: `Theme.fontSize("body")`, `fontWeight("normal")`, `Theme.color("textSecondary")`
- Snapshot hint line: `Theme.fontSize("small")`, `Theme.color("textSecondary")`, italic if `fontStyle` is supported; otherwise normal weight. This line is **reassurance**, not a warning. Its presence implies safety.
- "Delete" button: `DSStyle.Button` with `highlighted: true` — **not** a red destructive button. We are in a matter-of-fact mode, not a danger mode. The snapshot note already reassures.
- "Cancel" button: `DSStyle.Button` default style, positioned left.

**Copy — English:**
- Title: `"Delete \"%1\"?"`.arg(itemName)
- Body: `"A snapshot of the system configuration will be saved automatically before this change."`
- Secondary (when snapshot is unavailable — flag via backend signal): `"This change cannot be undone."` — still no red, still calm.
- Cancel: `"Cancel"` · Confirm: `"Delete"`

**Copy — Indonesian:**
- Title: `"Hapus \"%1\"?"`.arg(itemName)
- Body: `"Snapshot konfigurasi sistem akan disimpan otomatis sebelum perubahan ini."`
- Secondary: `"Perubahan ini tidak dapat dibatalkan."`
- Cancel: `"Batal"` · Confirm: `"Hapus"`

### 7.2 Move-to-Protected-Path Confirm

For paste/move operations where the *destination* is protected:

- Title: `"Move items to \"%1\"?"`.arg(destFolderName)
- Body: `"A snapshot will be saved automatically."` (same reassurance)
- Buttons: `"Cancel"` / `"Move"`

---

## 8. Subtle Indicators Inventory

### 8.1 Path Breadcrumb (in `FileManagerHeaderBar.qml`)

The `DSStyle.Label` that shows the current path (around line 115 of `FileManagerHeaderBar.qml`) should be annotated to support an inline icon injection.

**Where to insert:** After the path label `text` binding, add a sibling `Image` item inside the same `RowLayout`:

```qml
// Inside the path RowLayout in FileManagerHeaderBar.qml
// Immediately after the path DSStyle.Label:
Image {
    visible: hostRoot.activeTabElevationState !== "none"
             && hostRoot.currentPathIsProtected
    source: hostRoot.activeTabElevationState === "unlocked"
            ? "image://themeicon/changes-allow-symbolic"
            : "image://themeicon/changes-prevent-symbolic"
    width: 10; height: 10
    opacity: hostRoot.activeTabElevationState === "unlocked"
             ? 0.72
             : Theme.opacityIconMuted
    Layout.alignment: Qt.AlignVCenter
    Behavior on opacity {
        NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
    }
}
```

`hostRoot.activeTabElevationState` is a new string property on `FileManagerWindow` (values: `"none"`, `"locked"`, `"authenticating"`, `"unlocked"`, `"relocking"`). The backend architect must expose per-tab elevation state via the `slm-fsd` D-Bus API.

### 8.2 Sidebar Item Badge (`FileManagerSidebar.qml`)

In the `ListView` delegate that renders folder items (around line 120+ in `FileManagerSidebar.qml`), inside the row delegate that contains the folder icon `Image`:

```qml
// Overlay badge on the folder icon Image item
Image {
    id: protectedBadge
    visible: typeof row !== "undefined"
             && !!row.isProtected
             && !row.elevated
    source: "image://themeicon/changes-prevent-symbolic"
    width: 10; height: 10
    anchors.bottom: parent.bottom
    anchors.right: parent.right
    anchors.bottomMargin: -2
    anchors.rightMargin: -2
    opacity: Theme.opacitySubtle
}
```

The `row.isProtected` and `row.elevated` booleans are new fields to be added to the sidebar model rows (populated by `FileManagerSidebarOps.js` using `fileManagerApiRef.isProtectedPath()`).

### 8.3 Tab Title Indicator (`FileManagerTabBar.qml`)

Inside the `ListView` delegate `Rectangle`, before the close button:

```qml
// In FileManagerTabBar.qml delegate, before tabCloseButton:
FileManagerTabLockIndicator {
    id: tabLockIndicator
    visible: elevationState !== "none"
    elevationState: {
        // bound to hostRoot's per-tab elevation state map
        var map = hostRoot.tabElevationStates || {}
        return String(map[String(index)] || "none")
    }
    anchors.right: tabCloseButton.visible ? tabCloseButton.left : parent.right
    anchors.rightMargin: tabCloseButton.visible ? Theme.spacingXxs : Theme.spacingXs
    anchors.verticalCenter: parent.verticalCenter
}
```

### 8.4 Footer Status Bar (`FileManagerFooterBar.qml`)

The elevation pill inserts into the `RowLayout` in `FileManagerFooterBar.qml`, after the item-count label and before the share pill:

```qml
// In FileManagerFooterBar.qml RowLayout, after the item count DSStyle.Label:
FileManagerElevationFooter {
    visible: hostRoot.activeTabElevationState === "unlocked"
             || hostRoot.activeTabElevationState === "relocking"
    elevationState: hostRoot.activeTabElevationState
    relockSecondsRemaining: hostRoot.relockSecondsRemaining  // new property
}
```

The pill shows: `"Admin · 9:42"` (time-remaining in mm:ss). Format: `Theme.fontSize("small")`, `Theme.color("textSecondary")`. Not accented. Not alarming. Just metadata.

---

## 9. Auto-Relock UX

### 9.1 Timer Strategy

The 10-minute relock window is managed by the `slm-fsd` daemon. The daemon broadcasts:

1. `ElevationWarning(tabId, secondsRemaining)` — at T−60 s and T−30 s. The footer pill begins a subtle opacity pulse (0.72 → 0.45 → 0.72, 2 s period) to draw attention without alarming.
2. `ElevationExpired(tabId)` — the token has lapsed.

The frontend responds to `ElevationExpired` by transitioning the tab to `Relocking` state (Section 3.4).

### 9.2 Visual Transition

No toast. No dialog. No "Session expired!" anywhere.

- Unlocked-shield → locked-lock: icon crossfade, 480 ms `easingDecelerate`
- Footer pill: opacity fade-out, 320 ms `easingAccelerate`
- Toolbar write actions: opacity drop from 1.0 → `opacityFaint`, 240 ms `easingDefault`
- Breadcrumb icon: accent → `textSecondary` `opacityIconMuted`, 480 ms `easingDecelerate`

### 9.3 Mid-Operation Relock

If the user has initiated a write operation (e.g. a batch copy into `/etc`) and the token expires while the operation is in flight:

1. The `FileManagerBatchOverlayPopup` continues showing progress normally (the kernel-level operation holds the token until completion).
2. After the batch completes, the tab silently transitions to `Locked`.
3. If the user tries another write action immediately after, they receive the auth dialog again — same flow as the initial auth. No explanation needed; the lock icon on the tab communicates the state.

### 9.4 User-Initiated Lock

A "Lock" action should be accessible via the tab context menu (right-click on an elevated tab). Label: `"Lock This Tab"` / `"Kunci Tab Ini"`. This immediately triggers `Relocking → Locked`.

---

## 10. Accessibility

### 10.1 Focus Order

Banner (`FileManagerProtectedBanner`):
```
[View Only button] → [Authenticate… button] → [Mount Read-Only button, if visible]
```
Tab order matches visual left-to-right. "View Only" receives initial focus when the banner appears (`forceActiveFocus()` on `onVisibleChanged`).

`FileManagerSoftConfirmDialog`:
```
[Cancel button] → [Delete/Move button]
```
Cancel receives initial focus (safer default). User must explicitly Tab to confirm destructive action or click it.

`AuthDialog.qml` (existing):
```
[Password field] → [Authenticate button] → [Cancel button] → [Password field]
```
Unchanged. Already correctly implemented.

### 10.2 Keyboard Shortcuts

| Action | Shortcut | Rationale |
|---|---|---|
| Dismiss banner (View Only) | `Escape` when banner has focus | Consistent with escape-to-cancel everywhere in SLM |
| Trigger authenticate from banner | `Return` / `Enter` when "Authenticate…" focused | Standard |
| Lock elevated tab | `Ctrl+Shift+L` | Memorable. `Ctrl+L` is already navigate-to-path. |
| No dedicated "elevate" shortcut | — | Elevation is always context-triggered, not a mode toggle |

### 10.3 Screen-Reader Announcements

Each state transition should update an accessible name or description. Since Qt Quick uses `Accessible.name` and `Accessible.description`:

- Tab lock indicator: `Accessible.name: elevationState === "unlocked" ? "Elevated tab" : "Protected tab"` / Indonesian: `"Tab ditingkatkan"` / `"Tab terproteksi"`
- Banner: `Accessible.name: "Administrator content banner"` + `Accessible.description: bannerBodyText`
- Footer pill: `Accessible.name: "Admin session active, " + timeRemainingText + " remaining"` / Indonesian: `"Sesi admin aktif, " + … + " tersisa"`
- State transitions: use `Accessible.Notification` role on a hidden `Item` that changes its `Accessible.name` on each state change (the "live region" pattern in Qt Accessibility)

### 10.4 Color Contrast

All indicator states must pass WCAG AA (4.5:1 for small text, 3:1 for UI components) in both light and dark mode. The `accent` color at 0.72 opacity over a light surface must be verified by the design-system team when adding `protectedBadgeTint` — this is flagged as a **required accessibility check** before shipping.

---

## 11. Motion Specifications

All durations and easing curves use `Theme.*` tokens. Do not hardcode ms values.

| Animation | Duration token | Easing token | Notes |
|---|---|---|---|
| Banner enter (height + opacity) | `Theme.durationSm` | `Theme.easingDefault` | Height 0 → implicit, opacity 0 → 1, parallel |
| Banner exit | `Theme.durationFast` | `Theme.easingAccelerate` | Height implicit → 0, opacity 1 → 0, parallel |
| Tab icon crossfade (any state change) | `Theme.durationSm` | `Theme.easingDefault` | Opacity 0 → 1 on incoming icon, simultaneous |
| Tab icon pulse (Authenticating) | 600 ms per half-cycle | `Theme.easingLight` | `SequentialAnimation` in/out loop |
| Auth dialog enter | `Theme.durationSm` + `Theme.durationMd` | `Theme.easingDefault` | Already in AuthDialog.qml — do not change |
| Auth dialog exit | `Theme.durationFast` | `Theme.easingAccelerate` | Already in AuthDialog.qml — do not change |
| Toolbar action opacity restore | `Theme.durationFast` | `Theme.easingDefault` | `Behavior on opacity` |
| Footer pill appear | `Theme.durationSm` | `Theme.easingDefault` | Opacity 0 → 1 |
| Footer pill disappear (relock) | `Theme.durationFast` | `Theme.easingAccelerate` | Opacity 1 → 0 |
| Relocking icon transition | `Theme.durationWorkspace` × 0.48 | `Theme.easingDecelerate` | ~480 ms approx, cross-fade |
| Sidebar badge appear/disappear | `Theme.durationFast` | `Theme.easingDefault` | Opacity only |
| Breadcrumb icon swap | `Theme.durationSm` | `Theme.easingDefault` | Same crossfade as tab icon |
| Soft-confirm dialog enter | `Theme.durationSm` + `Theme.durationMd` | `Theme.easingDefault` | Match AppDialog standard animation |

**Design System Gap — motion token for 480 ms relock:** `Theme.durationWorkspace` is ~1000 ms (inferred from `AuthDialog.qml` spinner: `Theme.durationWorkspace * 2` for a 360° spin feels ~2000 ms). The relock transition wants ~480 ms. Use `Theme.durationMd` if it resolves to ~300–400 ms, or define `Theme.durationRelocking = Theme.durationMd * 1.4` as a named semantic token. **Verify `Theme.durationMd` value before shipping.**

---

## 12. QML Component Specifications

### 12.1 `FileManagerProtectedBanner.qml`

**File path:** `Qml/apps/filemanager/FileManagerProtectedBanner.qml`

**Embedded by:** `FileManagerMainPane.qml` — insert as the first child of the `ColumnLayout`, before `FileManagerTabBar`, using `visible: root.hostRoot.currentPathIsProtected && !root.hostRoot.bannerDismissed`

**Props / signals:**

```qml
// FileManagerProtectedBanner.qml (spec — ~30 lines of core structure)
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Rectangle {
    id: root

    required property var hostRoot
    // "locked" | "unlocked" | "authenticating"
    property string elevationState: "locked"
    // show the third button only when the path is a mountable volume
    property bool mountReadOnlyAvailable: false

    signal viewOnlyRequested()
    signal authenticateRequested()
    signal mountReadOnlyRequested()

    // Layout
    implicitHeight: bannerRow.implicitHeight + Theme.spacingMd * 2
    radius: Theme.radiusMd
    color: Theme.color("windowCard")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("windowCardBorder")

    RowLayout {
        id: bannerRow
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingXs

        Image {
            source: "image://themeicon/changes-prevent-symbolic"
            width: 12; height: 12
            opacity: Theme.opacityIconMuted
            Layout.alignment: Qt.AlignVCenter
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            DSStyle.Label {
                text: qsTr("Administrator content")  // banner_title
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("medium")
            }
            DSStyle.Label {
                text: qsTr("Changes to this folder require your password.")  // banner_body
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.Wrap
            }
        }

        DSStyle.Button {
            id: viewOnlyBtn
            text: qsTr("View Only")  // btn_view_only
            activeFocusOnTab: true
            KeyNavigation.tab: authenticateBtn
            onClicked: root.viewOnlyRequested()
        }

        DSStyle.Button {
            id: authenticateBtn
            text: qsTr("Authenticate…")  // btn_authenticate
            highlighted: true
            activeFocusOnTab: true
            KeyNavigation.tab: mountReadOnlyBtn.visible ? mountReadOnlyBtn : viewOnlyBtn
            KeyNavigation.backtab: viewOnlyBtn
            onClicked: root.authenticateRequested()
        }

        DSStyle.Button {
            id: mountReadOnlyBtn
            visible: root.mountReadOnlyAvailable
            text: qsTr("Mount Read-Only")  // btn_mount_readonly
            activeFocusOnTab: root.mountReadOnlyAvailable
            KeyNavigation.backtab: authenticateBtn
            KeyNavigation.tab: viewOnlyBtn
            onClicked: root.mountReadOnlyRequested()
        }
    }

    // Enter/exit animation
    Behavior on implicitHeight {
        NumberAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
    }
}
```

**Insertion point in `FileManagerMainPane.qml`:**

```qml
// In FileManagerMainPane.qml ColumnLayout, BEFORE FileManagerTabBar:
FileManagerProtectedBanner {
    Layout.fillWidth: true
    Layout.leftMargin: Theme.metric("spacingMd")
    Layout.rightMargin: Theme.metric("spacingMd")
    Layout.topMargin: Theme.metric("spacingXs")
    visible: root.hostRoot.currentPathIsProtected && !root.hostRoot.protectedBannerDismissed
    elevationState: root.hostRoot.activeTabElevationState
    hostRoot: root.hostRoot
    onViewOnlyRequested: root.hostRoot.protectedBannerDismissed = true
    onAuthenticateRequested: root.hostRoot.requestElevationForCurrentTab()
}
```

`hostRoot.currentPathIsProtected`, `hostRoot.protectedBannerDismissed`, `hostRoot.activeTabElevationState`, and `hostRoot.requestElevationForCurrentTab()` are new properties/methods on `FileManagerWindow.qml`.

---

### 12.2 `FileManagerTabLockIndicator.qml`

**File path:** `Qml/apps/filemanager/FileManagerTabLockIndicator.qml`

**Embedded by:** `FileManagerTabBar.qml` delegate — see Section 8.3

**Props:**

```qml
// FileManagerTabLockIndicator.qml (spec — ~35 lines)
import QtQuick 2.15
import SlmStyle

Item {
    id: root

    // "none" | "locked" | "authenticating" | "unlocked" | "relocking"
    property string elevationState: "none"

    width: 16; height: 16
    visible: elevationState !== "none"

    Image {
        id: lockedIcon
        anchors.centerIn: parent
        width: 12; height: 12
        source: "image://themeicon/changes-prevent-symbolic"
        visible: elevationState === "locked" || elevationState === "relocking"
        opacity: elevationState === "relocking"
                 ? relockFadeAnim.currentOpacity
                 : Theme.opacityIconMuted
    }

    Image {
        id: unlockedIcon
        anchors.centerIn: parent
        width: 12; height: 12
        source: "image://themeicon/changes-allow-symbolic"
        visible: elevationState === "unlocked" || elevationState === "authenticating"
        opacity: elevationState === "authenticating" ? pulseAnim.currentOpacity : 0.72
        // Tint using ColorOverlay or color property depending on Qt image type
        // For SVG symbolic icons that respect theme color:
        // color: elevationState === "unlocked" ? Theme.color("accent") : Theme.color("textSecondary")
    }

    // Pulse animation for Authenticating state
    SequentialAnimation {
        id: pulseAnim
        property real currentOpacity: 0.45
        running: elevationState === "authenticating"
        loops: Animation.Infinite
        NumberAnimation { target: pulseAnim; property: "currentOpacity"; to: 0.70; duration: 600; easing.type: Theme.easingDefault }
        NumberAnimation { target: pulseAnim; property: "currentOpacity"; to: 0.45; duration: 600; easing.type: Theme.easingDefault }
    }

    // Relock fade animation
    NumberAnimation {
        id: relockFadeAnim
        property real currentOpacity: 0.72
        target: relockFadeAnim; property: "currentOpacity"
        running: elevationState === "relocking"
        from: 0.72; to: Theme.opacityIconMuted
        duration: Theme.durationWorkspace  // ~480 ms; verify token value
        easing.type: Theme.easingDecelerate
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: elevationState === "unlocked" ? qsTr("Elevated tab")
                   : elevationState !== "none"     ? qsTr("Protected tab")
                   : ""
}
```

---

### 12.3 `FileManagerElevationFooter.qml`

**File path:** `Qml/apps/filemanager/FileManagerElevationFooter.qml`

**Embedded by:** `FileManagerFooterBar.qml` RowLayout — see Section 8.4

**Props:**

```qml
// FileManagerElevationFooter.qml (spec — ~25 lines)
import QtQuick 2.15
import QtQuick.Controls 2.15
import SlmStyle as DSStyle

Rectangle {
    id: root

    property int relockSecondsRemaining: 600
    // "unlocked" | "relocking"
    property string elevationState: "unlocked"

    implicitWidth: elevLabel.implicitWidth + Theme.spacingMd * 2
    implicitHeight: 20
    radius: Theme.radiusPill
    color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05)
    border.width: Theme.borderWidthThin
    border.color: Theme.color("panelBorder")

    opacity: elevationState === "relocking" ? 0.0 : 1.0
    Behavior on opacity {
        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingAccelerate }
    }

    function formatCountdown(secs) {
        var s = Math.max(0, Number(secs || 0))
        var m = Math.floor(s / 60)
        var r = s % 60
        return "Admin · " + String(m) + ":" + (r < 10 ? "0" : "") + String(r)
    }

    DSStyle.Label {
        id: elevLabel
        anchors.centerIn: parent
        text: root.formatCountdown(root.relockSecondsRemaining)
        color: Theme.color("textSecondary")
        font.pixelSize: Theme.fontSize("small")
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Admin session active, ")
                     + root.relockSecondsRemaining + qsTr(" seconds remaining")
}
```

---

### 12.4 `FileManagerSoftConfirmDialog.qml`

**File path:** `Qml/apps/filemanager/FileManagerSoftConfirmDialog.qml`

**Embedded by:** `FileManagerDialogs.qml` — add alongside other dialog declarations, expose via `softConfirmDialogRef` alias.

**Props / signals:**

```qml
// FileManagerSoftConfirmDialog.qml (spec — ~40 lines)
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

AppDialog {
    id: root

    required property var hostRoot

    // Populate before calling open()
    property string itemName: ""
    property string actionLabel: "Delete"    // "Delete" | "Move" | "Overwrite"
    property bool snapshotAvailable: true

    signal actionConfirmed()

    function openForItem(name, action, hasSnapshot) {
        itemName = String(name || "")
        actionLabel = String(action || "Delete")
        snapshotAvailable = !!hasSnapshot
        open()
    }

    title: qsTr("%1 “%2”?").arg(actionLabel).arg(itemName)
    standardButtons: Dialog.NoButton
    dialogWidth: 460
    x: Math.round((hostRoot.width  - width)  * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)

    bodyComponent: Component {
        ColumnLayout {
            spacing: 10
            implicitHeight: 88

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.snapshotAvailable
                      ? qsTr("A snapshot of the system configuration will be saved automatically before this change.")
                      : qsTr("This change cannot be undone.")
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("body")
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: Theme.lineHeight("normal")
            }
        }
    }

    footerComponent: Component {
        RowLayout {
            spacing: 8
            implicitHeight: Math.max(32, Theme.metric("controlHeightRegular"))

            Item { Layout.fillWidth: true }

            DSStyle.Button {
                id: cancelBtn
                text: qsTr("Cancel")
                activeFocusOnTab: true
                KeyNavigation.tab: confirmBtn
                onClicked: root.close()
                Component.onCompleted: forceActiveFocus()  // Cancel gets initial focus
            }

            DSStyle.Button {
                id: confirmBtn
                text: qsTr(root.actionLabel)
                highlighted: true
                activeFocusOnTab: true
                KeyNavigation.backtab: cancelBtn
                onClicked: { root.actionConfirmed(); root.close() }
            }
        }
    }
}
```

---

### 12.5 Sidebar Shield Badge — Annotation (no new component)

In `FileManagerSidebar.qml`, within the existing `ListView` delegate that renders folder/bookmark rows, the folder icon `Image` item should gain a badge overlay child. The exact insertion depends on the existing item structure; the pattern is:

```qml
// Wrap or annotate the folder icon Image in FileManagerSidebar.qml
// The parent Item of the folder icon gets:
Image {
    id: protectedShieldBadge
    // Position at bottom-right of folder icon
    anchors.bottom: folderIconImage.bottom
    anchors.right: folderIconImage.right
    anchors.bottomMargin: -2
    anchors.rightMargin: -2
    width: 10; height: 10
    source: row.elevated
            ? "image://themeicon/changes-allow-symbolic"
            : "image://themeicon/changes-prevent-symbolic"
    visible: !!row.isProtected
    opacity: row.elevated ? 0.72 : Theme.opacitySubtle
    Behavior on opacity {
        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
    }
    z: 2  // above the folder icon
}
```

`row.isProtected` and `row.elevated` are new boolean fields in the sidebar model (populated in `FileManagerSidebarOps.js`). The `slm-fsd` API must expose `isProtectedPath()` (already exists: `fileManagerApiRef.isProtectedPath()` is used in `FileManagerWindow.qml:294`).

---

## 13. ASCII Reference Mockup — Elevated Tab, `/etc` Selected File, Footer Countdown

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  SLM Files                                                          [ - ] [ □ ] [ × ] │
├───────────────┬──────────────────────────────────────────────────────────────────────┤
│ Favorites     │  [ ← ] [ → ]   etc   /   ◈ (12px accent shield)         [ Search ]  │
│  Home         ├──────────────────────────────────────────────────────────────────────┤
│  Desktop      │  [ Home ]  [ Downloads ]  [ etc 🛡 ×  ]  [ + ]                      │
│  Downloads    │                              ^^^^^^^^^^^                              │
│  Documents    │           (active tab: shield in accent, lock→unlocked-shield icon   │
│               │            at right edge of tab, before ×)                           │
│ Locations     │──────────────────────────────────────────────────────────────────────│
│  /etc  🔒     │  Name            Size        Modified                                │
│  /usr  🔒     │  ──────────────────────────────────────────────────────────────────  │
│               │  cron.d          —           Yesterday                               │
│               │  fstab           512 B       3 days ago                              │
│               │  ▶ hosts         312 B       Today 14:23          ← selected         │
│               │  hostname        12 B        2 weeks ago                             │
│               │  passwd          1.4 KB      Today 09:01                             │
│               │  sudoers         740 B       Last month                              │
│               │                                                                      │
│               │                                                                      │
│               │                                                                      │
├───────────────┴──────────────────────────────────────────────────────────────────────┤
│  6 items · 1 selected            [ Admin · 8:47 ]                    [ Shared    ]   │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

Legend:
- `🛡` on tab title: `FileManagerTabLockIndicator` in `unlocked` state (accent color, open-shield icon)
- `🔒` on sidebar `/etc` row: protected badge (10 px, `opacitySubtle`, bottom-right of folder icon)  
- `◈` in breadcrumb: inline unlocked-shield icon after `/etc` crumb (accent, 10 px)
- `[ Admin · 8:47 ]` in footer: `FileManagerElevationFooter` pill (small, muted, pill-shaped)
- Selected `hosts` file: standard content-view selection, no elevation-specific treatment

---

## 14. Design-System Gaps Summary

| Gap ID | Token name proposed | Location | Proposed value |
|---|---|---|---|
| DSG-01 | `iconSizeTabIndicator` | `Theme.qml` metric/icon tokens | `12` (px) |
| DSG-02 | `color("protectedBadgeTint")` | `Theme.qml` Generic family | Light: `rgba(0,0,0,0.42)` · Dark: `rgba(255,255,255,0.38)` |
| DSG-03 | `durationRelocking` or verify `durationMd` | Motion section of `Theme.qml` | ~480 ms — verify `durationMd` current value |
| DSG-04 | `borderWidthHairline` | Already referenced in `FileManagerFooterBar.qml` (existing code uses it for the "Shared" pill) — may already exist | Verify in `Theme.qml` |

**Note on DSG-04:** `Theme.borderWidthHairline` is referenced in `FileManagerFooterBar.qml:108` but not listed in `STYLE_GUIDE.md` Border Tokens section. Either the style guide is outdated or the usage is a violation. Resolve before proceeding.

---

## 15. Backend API Requirements (for the `slm-fsd` architect)

The UX design depends on the following backend capabilities. These are **not optional** — the visual design cannot function without them:

1. **Per-tab elevation state signal:** `ElevationStateChanged(tabId: string, state: string)` — D-Bus signal on the `slm-fsd` interface. States: `"locked"`, `"authenticating"`, `"unlocked"`, `"relocking"`. The frontend maps this to the `hostRoot.tabElevationStates` map.

2. **Countdown pre-warning signals:** `ElevationWarning(tabId: string, secondsRemaining: int)` — broadcast at T−60 s and T−30 s. The footer pill uses `secondsRemaining` to display `"Admin · mm:ss"` and begins pulsing at T−60.

3. **Token-expiry signal with grace period:** `ElevationExpired(tabId: string)` — fired when the session truly ends. Importantly, operations already in-flight must be allowed to complete before this signal fires. Do **not** fire it mid-operation.

4. **Snapshot availability broadcast:** `SnapshotAvailable(tabId: string, available: bool)` — lets `FileManagerSoftConfirmDialog` show the reassuring "snapshot will be saved" body text only when it's actually true. If unavailable, the dialog falls back to "cannot be undone" (still calm, just accurate).

5. **`AuthDialog` window-hint parameter:** The polkit agent should accept an optional `parentWindowId` (XDG Activation token or Wayland surface handle) so that SLM Files can request the dialog appear centered on the file manager window rather than screen-center. This is a quality-of-life improvement; the UX works without it but centering is noticeably better.

---

## 16. Open Questions

1. **Protected path definition:** Who maintains the list of "protected prefixes" (`/etc`, `/usr`, `/var`, `/root`, etc.)? Is this a hardcoded list in `slm-fsd`, or configurable per-deployment? The visual design assumes a fixed-for-now list. If it's dynamic, `FileManagerSidebarOps.js` must refresh badges on list change.

2. **Read-only mount option:** The "Mount Read-Only" banner button (Section 4.2) requires the backend to support mounting a protected volume in read-only mode. Is `slm-fsd` implementing this? If not, `mountReadOnlyAvailable` stays `false` and the button never appears.

3. **Tab ID strategy:** The UX spec uses a `tabId` string to scope elevation state per-tab. The current `FileManagerTabBar.qml` uses only an `index` integer. What is the canonical tab identity in the new architecture — a stable UUID assigned at tab creation, or the index?

4. **Snapshot technology:** Section 7 and Section 9.4 mention automatic snapshots for reassurance copy. Which snapshotting mechanism does `slm-fsd` use (Btrfs snapshots, a custom overlay, nothing)? If no snapshot capability exists yet, the soft-confirm dialog should use the "cannot be undone" variant unconditionally.

5. **Multi-window elevation scope:** If the user has two SLM Files windows open and elevates tab 2 in window A, does that token apply in window B? The spec assumes per-window-per-tab isolation. If the backend shares tokens across windows (per-user session), the UI indicators must reflect that — an "already elevated" state could skip the auth dialog if a sibling window already holds the token.

---

*End of specification. This document is a design artifact — it does not modify any source file. All QML snippets in Section 12 are spec-quality pseudocode; they are intended to be precise enough for an engineer to implement without design clarification, but are not yet compilable without the `AppDialog` base class resolution and `Theme.qml` token additions noted in Section 14.*
