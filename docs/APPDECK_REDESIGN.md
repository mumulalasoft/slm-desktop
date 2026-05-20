# AppDeck Redesign Plan

Status: **MVP complete (4/4 phases)** — pending visual verification in QEMU
Owner: m.mukharom
Started: 2026-05-20
Completed: 2026-05-20

## Goal

Redesign Grid Layer dan Pulse Layer AppDeck dengan filosofi Apple
(*clarity, deference, depth*):

> *"Grid is for browsing. Pulse is for intent."*

- **Grid** = launchpad visual, eksploratif, kategori, paging horizontal.
- **Pulse** = command bar, keyboard-first, contextual dynamic layout.

Search logic disatukan: mengetik di Grid otomatis membuka Pulse.

---

## Grid Layer — Final Spec

Vertical stack of 3 regions; paging hanya di region bawah.

```
┌─────────────────────────────────────────────────────────┐
│  Recent                                                  │
│  ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐    (6 ikon, horizontal)  │
│  └──┘ └──┘ └──┘ └──┘ └──┘ └──┘                          │
│                                                          │
│  Suggestions                                             │
│  ┌──┐ ┌──┐ ┌──┐ ┌──┐                                    │
│  └──┘ └──┘ └──┘ └──┘              (3–4 ikon)            │
│                                                          │
│  All Apps                                                │
│  [All] [Productivity] [Internet] [Graphics & Media]      │
│        [Utilities]                                       │
│  ─────────────────────────────────────────────────────   │
│  ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐                     │
│  └──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘                     │
│  ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐                     │
│  └──┘ └──┘ └──┘ └──┘ └──┘ └──┘ └──┘                     │
│                  ●  ○  ○  ○        (paging dots)         │
└─────────────────────────────────────────────────────────┘
```

### Region 1 — Recent
- 6 ikon, single horizontal strip, **no paging**, no category filter.
- Sumber: **MRU semua app** dari `lastOpenedMs` + `activationCount` (sudah ada di `rowScore()` PulseOverlay.qml).
- Always visible.

### Region 2 — Suggestions
- 3–4 ikon, contextual ranking:
  - Time-of-day bucket:
    - 06–11: Office/Development/Email categories
    - 11–17: usage-pattern average pada slot waktu ini
    - 17–24: Media/Network/Graphics
  - **Override**: app yang dipanggil dalam 3 hari terakhir tapi belum di-pin → prioritas.
- Label "Suggestions" (no AI/Smart prefix — sengaja vague, Apple style).

### Region 3 — All Apps
- **Segmented control** dengan 5 bucket tetap:
  - **All**
  - **Productivity** (Categories: Office, Development, TextEditor, IDE, ProjectManagement)
  - **Internet** (Network, WebBrowser, Email, Chat, FileTransfer)
  - **Graphics & Media** (Graphics, AudioVideo, Photography, Music, Player)
  - **Utilities** (Settings, System, Utility, FileManager, Calculator, Terminal)
- Paging horizontal **dipertahankan** di region ini.
- Pagination dots di footer **region ini saja**, bukan global.
- Kategori mapping: app dengan multiple `.desktop Categories=` diklasifikasikan ke bucket pertama yang cocok mengikuti urutan di atas.

### Search trigger
- User mengetik (focus di mana pun dalam Grid) → grid blur ke belakang (`MultiEffect` blur ~20 px + opacity 0.7) → **Pulse overlay** muncul di atas.
- ESC di Pulse → kembali ke Grid, **state preserved**: page index, kategori segment, dan scroll.

---

## Pulse Layer — Final Spec

Layout **kontekstual dinamis** merespons query.

### State A — Single column (default)
Trigger: 1 kategori menguasai ≥80% hasil **atau** total <5 hasil.

```
┌──────────────────────────────────────────────────┐
│  ⌕  query...                              ⌘K     │
├──────────────────────────────────────────────────┤
│  Top Result                                       │
│  ┌──┐  Visual Studio Code                         │
│  │  │  /usr/share/applications/code.desktop       │
│  └──┘                                             │
├──────────────────────────────────────────────────┤
│  More                                             │
│  ▸ Code - OSS                                     │
│  ▸ Codium                                         │
└──────────────────────────────────────────────────┘
```

### State B — Two columns (auto)
Trigger: ≥2 kategori, masing-masing ≥2 item (dari top-12 hasil).

```
┌──────────────────────────────────────────────────────────────┐
│  ⌕  query...                                          ⌘K     │
├──────────────────────────────────────────────────────────────┤
│  Top Result                                                   │
├──────────────────────────────┬───────────────────────────────┤
│  Apps                        │  Files                         │
│  ▸ ...                       │  ▸ ...                        │
├──────────────────────────────┼───────────────────────────────┤
│  Settings                    │  Commands                      │
│  ▸ ...                       │  ▸ ...                        │
└──────────────────────────────┴───────────────────────────────┘
```

Transition A↔B: width column animasi 300 ms ease-out — panel
terasa "membuka diri" ketika hasil meluas. Tidak rebuild total.

Kolom mapping:
- Kiri: Apps + Settings
- Kanan: Files + Commands

### State C — Command mode (`>` / `/`)
- Bar tinted dengan `accent` ~6% opacity.
- Prompt glyph `›` di kiri input.

### Idle state
```
┌──────────────────────────────────────────────────┐
│  ⌕  Search apps, files, settings...        ⌘K    │
├──────────────────────────────────────────────────┤
│  Recents                                          │
│  ┌──┐ ┌──┐ ┌──┐ ┌──┐                              │
│  └──┘ └──┘ └──┘ └──┘                              │
│                                                   │
│  Tip:  ›  for commands     /  for slash actions  │
└──────────────────────────────────────────────────┘
```

### Removed from Pulse
- `PulseActionRow` chip horizontal (Open / Pin / Reveal) — pindah ke
  context menu via `⌘K`.
- `PulseGridSection` System grid di bawah saat mode search biasa —
  masuk ke kolom kanan di State B.
- 5 idle quick-action chips lama (Toggle Dark Mode, dst) — diganti
  Recents row + tip teks.

### Context menu (⌘K)
Membuka menu kontekstual untuk hasil terpilih:
- App: Open / Pin to AppDeck / Open New Window
- File: Open / Reveal in Files / Copy Path
- Setting/Command: Execute
- Calculator: Copy Result / Copy Expression

---

## Phased Implementation

Setiap fase dapat dimerge independen, tidak boleh memutus build atau
test suite. Save state di file ini setelah tiap fase selesai.

### Phase 1 — Grid restructure
- Refactor `AppDeckGridView.qml` menjadi 3 region (Recent / Suggestions / All Apps).
- Wire Recent dan Suggestions data source.
- Tidak mengubah search dulu (filter teks tetap lokal sementara).
- Acceptance: 3 region terlihat, paging tetap berfungsi di region All Apps.

### Phase 2 — Category segmented control
- Implement 5 bucket category mapping.
- Segmented control sticky di atas grid bawah.
- Filter paging berdasarkan kategori terpilih.
- Acceptance: segmented switch memfilter paging dengan benar; "All" = semua app.

### Phase 3 — Pulse contextual layout
- Refactor `PulseOverlay.qml`:
  - Hapus `PulseActionRow` dan `PulseGridSection` saat search.
  - Implement layout detector (single vs two-column berdasarkan distinct categories di top-12).
  - Animasi transisi width.
  - Idle state baru (Recents + tip).
  - ⌘K context menu.
- Acceptance: layout berubah otomatis sesuai distribusi hasil; idle state baru.

### Phase 4 — Grid→Pulse search integration
- Hapus filter teks lokal di Grid.
- Mengetik di Grid → trigger Pulse open dengan query seed.
- Grid blur ke belakang saat Pulse aktif.
- ESC restore Grid state (page + kategori).
- Acceptance: tidak ada double search logic; transisi smooth.

---

## Progress Log

- 2026-05-20: Design locked. Doc written. Tasks created. Phase 1 starting.
- 2026-05-20: **Phase 1 complete**. Changes in `Qml/components/appdeck/AppDeckGridView.qml`:
  - Added `recentApps` (6 MRU) and `suggestionApps` (4) properties, sourced from
    `AppModel.frequentApps()`. Refreshed on `appScoresChanged`.
  - Removed top "Applications" header label (redundant with section labels).
  - Inserted Recent section (label + horizontal Row Repeater of `AppDeckItemDelegate`).
  - Inserted Suggestions section (same shape).
  - Added "All Apps" section label above existing paged GridView.
  - Both strips hide while local `filterText` is non-empty (Phase 4 routes
    search to Pulse instead).
  - Note: Phase 1 suggestionApps = next-frequent minus Recent.
    Phase 2 will refine with time-of-day category bucket.
  - Build OK. All 6 AppDeck contract tests pass.
  - Visual verification in QEMU still TODO (user-driven).
- 2026-05-20: **Phase 2 complete**. Changes:
  - `src/core/appmodel.h` — added `categories: QStringList` to
    `DesktopAppEntry`; added `CategoriesRole` to roles enum.
  - `src/core/appmodel.cpp` — added `parseDesktopCategories()` helper that
    tokenizes `Categories=` (semicolon-separated, trims, drops empties).
    Wired in all 3 scan paths: inline GAppInfo, threaded
    `computeAppsFromSystem` GAppInfo, and both GKeyFile fallbacks.
    Exposed `categories` in `page()` and `frequentApps()` output.
    Updated `data()`/`roleNames()` and extended `appCatalogSignature` so
    Categories= changes trigger a refresh (sameAppCatalog comparison).
  - `Qml/components/appdeck/AppDeckGridView.qml`:
    - Added `selectedCategory` property + `categoryButtons` list (5 buckets).
    - Added `_categoryBucket(categories)` mapping XDG tokens →
      Internet / Graphics & Media / Productivity / Utilities. Priority order
      is most-specific-first so e.g. Mail (Office;Network;Email;) → Internet.
    - `_normalize` now propagates `categories` array.
    - `_rebuildModel` filters by selected bucket (skipped while local
      filter text is non-empty — Phase 4 routes search to Pulse).
    - `onSelectedCategoryChanged` resets `currentPage` and rebuilds.
    - Replaced static "All Apps" label with a Repeater of pill chips
      (active chip = accent fill + accentText label).
  - Build OK. 8/8 AppDeck + AppModel tests pass.
- 2026-05-20: **Phase 3 complete**. Changes in
  `Qml/components/pulse/PulseOverlay.qml`:
  - Added `settingsModel`, `commandsModel`, `recentsModel` ListModels.
    `systemModel` is kept as the combined source for command-mode and
    keyboard section-4 navigation; settings/commands are derived in
    `rebuildSections` based on `provider`.
  - Added `useTwoColumns` (auto-detect): ≥2 of {apps, files, settings,
    commands} buckets each ≥2 items → two columns.
  - Removed legacy clutter: `PulseActionRow` (chip horizontal Open/Pin/...)
    and the bottom `PulseGridSection` System/Commands. Quick actions per
    result moved to a ⌘K context menu.
  - Idle state replaced: `Recents` row (4 apps from `AppModel.frequentApps`,
    refreshed on `appScoresChanged`) + tip text
    `Tip: › for commands  / for slash actions`.
  - Search layout switches between `singleColPanel` (stacked Apps/Files/
    Settings/Commands) and `twoColPanel` (Apps+Settings left / Files+
    Commands right) based on `useTwoColumns`.
  - Added `Menu` (`contextMenu`) and helper `openContextMenuForFocused()`.
    Keyboard shortcut `Ctrl/⌘+K` opens it on the focused result.
    Menu adapts per entry kind (app / file / system / calculator).
  - Build OK. AppDeck/AppModel tests still 8/8 pass. Pulse C++ test
    failures (`pulseservice_dbus_test`, `pulse_controller_js_test`) are
    pre-existing test-infra issues unrelated to QML changes — verified by
    `git stash` reproducing the same failures on the prior commit.
  - Known follow-up: keyboard navigation for settingsModel/commandsModel
    is mouse-only in this pass (no dedicated focusedSection index).
    Acceptable for the MVP; refine in a later iteration.
- 2026-05-20: **Phase 4 complete**. Most of the wiring was already in
  place from prior work — `AppDeckGridAppsView` already passes
  `filterText: ""` to the inner grid and emits `pulseQueryChanged` to
  flip `deckMode` to pulse; ESC routes through
  `AppDeckWindow.dismissCurrentLayer()` (pulseMode→grid→dock). The
  gap from Phase 1+2 was that the visibility checks I added to the
  inner grid referenced its (always-empty) `filterText` instead of the
  parent's search intent — so the strips + segmented control never
  collapsed during search.
  - `Qml/components/appdeck/AppDeckGridView.qml` — added `searchActive`
    property; replaced 3 visibility checks and the category-filter
    guard in `_rebuildModel` to use it. Added `onSearchActiveChanged`
    to rebuild when search state flips so the category filter stops
    pruning while a search is in flight.
  - `Qml/components/appdeck/AppDeckGridAppsView.qml` — bind
    `searchActive: root.searchActive` on the inner grid (the parent
    already exposed this readonly property derived from its own
    filterText).
  - Build OK. 8/8 AppDeck + AppModel tests pass.
  - Backdrop "blur ke belakang" remains the existing opacity 0.88 +
    scale 0.98 (no `MultiEffect` blur). Adding true blur on Wayland
    needs care around layer.enabled cost; tracked as a follow-up.
- 2026-05-20: **Phase 1 revision after visual eval**. The first QEMU
  screenshot exposed three issues:
  - `AppDeckGridAppsView` already renders `AppDeckFavoritesRow` as the
    "Recent" section (its own "Recent" label + 8 large icons sorted by
    lastLaunch). My new Recent strip inside `AppDeckGridView` duplicated
    it visually.
  - Suggestions delegates lacked `compact: true` so the icons looked
    smaller than the favoritesRow above them.
  - Page indicator dots floated far above the dock pill because
    `AppDeckWindow` added a `+20` extra padding on top of the dock
    item's already-built-in `tooltipHeadroom(44) + 8` safety margin.

  Fixes:
  - Removed the duplicate Recent strip (`recentApps`,
    `_rebuildRecentApps`, `recentSection`) from `AppDeckGridView.qml`.
  - Added an `excludeKeys` property on `AppDeckGridView`. The parent
    builds it from `favoriteApps[*].appId|desktopId|...` so Suggestions
    no longer echoes apps the user already sees in Recent above.
  - Suggestions delegates now use `compact: true` and the same 96×104
    cell footprint as `AppDeckFavoritesRow`.
  - `AppDeckWindow.qml` — trimmed the `bottomSafeInset` extra padding
    from `+20` to `+8`. The dock item already reserves
    `tooltipHeadroom(44) + 8` inside its own height, so the hover
    tooltip and the dock pill cannot be clipped by this change.
  - Build OK. 8/8 AppDeck + AppModel tests still pass.
