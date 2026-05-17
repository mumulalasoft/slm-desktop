# AppDeck Unified Spatial Surface System

> **STATUS IMPLEMENTASI (2026-05-17)**
> - ✅ §2 `transitioning` internal state + Timer lock
> - ✅ §4 `intent` property (browse / search / command / contextual)
> - ✅ §11 `appearanceMode` (always_show / auto_hide)
> - ✅ §13–15 Auto hide: `autoHidePresence`, idle timer, edge-reveal, spatial motion (no teleport)
> - ✅ §22 Spatial Memory properties (spatialLastGridPage, spatialLastFocusedIconId, etc.)
> - ✅ §23 Resting Animation: breathing luminance on dockBackground
> - ✅ §25 Motion Tokens: morphDuration, modeDuration, collapseDuration, autoHide*, resting*
> - ✅ §27 `morphProgress`, `dockVisible` API surface properties
> - ✅ §28 Content Reveal Timeline: staggered opacity via `contentRevealOpacity` (dock→grid 0.40–0.70 reveal, grid→dock 0.70–1.00 fade)
> - ✅ §30 Validation: outside click collapse, app activation collapse, ESC collapse, single surface
> - ⏳ §19 Icon continuity (dockAnchor/gridAnchor) — requires per-icon positional tracking
> - ⏳ §20 Focus Gravity System — morph origin from clicked icon center

## 🎯 TUJUAN

Bangun AppDeck sebagai:

```plaintext id="tb4a56"
Spatial Interaction Surface
```

Bukan sekadar:

* dock
* launchpad
* search popup
* app launcher

AppDeck harus terasa:

* hidup
* spatial
* konsisten
* premium
* responsif
* predictable

Dengan:

* satu surface utama
* sistem state + mode konsisten
* morph animation kontinu
* motion architecture unified
* continuity visual & interaction

---

# 1. SINGLE SURFACE RULE

Hanya ada satu surface utama:

```plaintext id="4ft2xk"
AppDeck
```

DILARANG membuat:

* AppHub
* Launchpad
* Pulse window
* Grid window
* Dock window

Semua adalah representasi state/mode dari AppDeck.

---

# 2. STATE SYSTEM

## Public State

```plaintext id="m9qj4n"
dock
grid
```

## Internal Runtime State

```plaintext id="b2gjv0"
transitioning
```

Runtime flow:

```plaintext id="7v8a8g"
dock → transitioning → grid
grid → transitioning → dock
```

### RULE

* `transitioning` hanya internal runtime state
* tidak boleh dipakai sebagai terminology UI publik
* digunakan untuk:

  * animation locking
  * gesture interruption
  * transition safety
  * race-condition prevention

---

# 3. MODE SYSTEM

```plaintext id="t99c6w"
apps
pulse
context
```

---

# 4. INTENT SYSTEM

Tambahkan interaction intent layer:

```qml id="fq6mq5"
property string intent
```

Valid intent:

```plaintext id="bmn8k9"
browse
search
command
contextual
```

Mapping:

| Mode    | Intent           |
| ------- | ---------------- |
| apps    | browse           |
| pulse   | search / command |
| context | contextual       |

---

# 5. VALID COMBINATIONS

```plaintext id="3j3s5o"
AppDeck[state=dock, mode=apps]

AppDeck[state=grid, mode=apps]
AppDeck[state=grid, mode=pulse]
AppDeck[state=grid, mode=context]
```

Forbidden:

```plaintext id="2q1fl7"
AppDeck[state=dock, mode=pulse]
AppDeck[state=dock, mode=context]
```

---

# 6. OFFICIAL ALIAS

```plaintext id="b4f0ow"
dock    = AppDeck[state=dock, mode=apps]
grid    = AppDeck[state=grid, mode=apps]
pulse   = AppDeck[state=grid, mode=pulse]
context = AppDeck[state=grid, mode=context]
```

---

# 7. TRANSITION GRAPH

```plaintext id="kp8mp5"
dock ↔ grid

grid ↔ pulse
grid ↔ context

pulse ↔ context
```

---

# 8. AUTO COLLAPSE POLICY

AppDeck WAJIB collapse kembali ke:

```plaintext id="6xq1ct"
AppDeck[state=dock, mode=apps]
```

Pada:

* app activation
* app launching
* outside click
* ESC pressed
* workspace interaction
* focus loss

---

# 9. APP ACTIVATION FLOW

Saat app launch:

```plaintext id="c1d3cl"
1. activation accepted
2. launch animation start
3. AppDeck collapse start
4. focus transfer
5. app active
```

DILARANG:

* menunggu app load selesai
* freeze AppDeck
* blocking transition

---

# 10. OUTSIDE CLICK POLICY

Click pada:

* wallpaper
* workspace
* desktop
* app window
* shell surface lain

WAJIB:

```plaintext id="mjmnl4"
collapse → dock
```

---

# 11. APPEARANCE POLICY

Tambahkan appearance mode:

```plaintext id="tdb4bq"
always_show
auto_hide
```

---

# 12. ALWAYS SHOW

```plaintext id="7s44o3"
appearance = always_show
```

Dock:

* selalu visible
* persistent anchor
* tetap morphable ke grid

---

# 13. AUTO HIDE

```plaintext id="t0v7i2"
appearance = auto_hide
```

Dock:

* hide otomatis saat idle
* reveal saat interaction

Dock TIDAK BOLEH:

```plaintext id="1d0p0l"
visible = false
destroy()
opacity = 0 permanent
```

Sebaliknya:

```plaintext id="2zx3v5"
low-presence state
```

Dock tetap meninggalkan:

* subtle edge glow
* reveal hint
* luminance edge
* tiny presence

Tujuan:

```plaintext id="icg30h"
workspace still has anchor memory
```

---

# 14. AUTO HIDE TRIGGER

Reveal saat:

* edge hover
* gesture
* keyboard shortcut
* focus request
* workspace interaction

Hide saat:

* idle timeout
* fullscreen focus
* inactivity

---

# 15. AUTO HIDE MOTION

DILARANG:

* teleport
* instant disappear
* fade-only

WAJIB:

* spatial offset
* opacity transition
* continuity movement

Dock harus terasa:

```plaintext id="0y3mbj"
alive and attached to workspace
```

---

# 16. MORPH TRANSITION PRINCIPLE

Dock ↔ Grid WAJIB:

```plaintext id="mv5o1n"
continuous spatial morph
```

Bukan:

```plaintext id="6k87rf"
surface replacement
fade swap
fullscreen popup
```

---

# 17. MORPH ELEMENTS

WAJIB dimorph:

* container size
* radius
* blur/material
* icon position
* icon scale
* spacing
* shadow
* elevation
* density
* clip shape
* padding

---

# 18. SPATIAL CONTINUITY RULE

Dock TIDAK BOLEH:

```plaintext id="9j4v0z"
hilang → grid muncul
```

WAJIB:

```plaintext id="ny7g5s"
dock evolves into grid
```

---

# 19. ICON CONTINUITY RULE

Setiap icon WAJIB punya:

```plaintext id="zk2qpk"
dockAnchor
gridAnchor
```

Animasi:

```plaintext id="cgys5r"
icon.position = interpolate(dockAnchor, gridAnchor, morphProgress)
icon.scale    = interpolate(dockScale, gridScale, morphProgress)
```

DILARANG:

* teleport
* destroy/recreate
* layout jump

---

# 20. FOCUS GRAVITY SYSTEM

Setiap transisi WAJIB memiliki:

```plaintext id="sbyf86"
gravity center
```

Contoh:

* clicked icon
* pointer location
* focused app
* search invocation point

Morph harus terasa berasal dari titik tersebut.

---

# 21. PULSE DESIGN RULE

Pulse BUKAN:

* popup search
* window terpisah
* launcher lain

Pulse adalah:

```plaintext id="8vuy09"
attention state of AppDeck
```

Yang berubah:

* focus
* density
* blur
* content
* interaction intent

Spatial continuity harus tetap terjaga.

---

# 22. SPATIAL MEMORY SYSTEM

AppDeck WAJIB mempertahankan:

* last scroll position
* last focused icon
* last pulse cursor
* last grid position
* interaction history ringan

Tujuan:

```plaintext id="om17t0"
persistent workspace feeling
```

---

# 23. RESTING ANIMATION RULE

AppDeck boleh memiliki:

* breathing luminance
* micro parallax
* ambient hover field

Tetapi:

* sangat subtle
* low frequency
* tidak distracting

---

# 24. MOTIONCONTROLLER CONTRACT

Semua animasi WAJIB lewat MotionController.

Pisahkan:

* state transition
* mode transition
* appearance transition
* micro interaction

---

# 25. MOTION TOKEN

WAJIB ada:

```plaintext id="d4h44i"
appdeck.morph.duration
appdeck.morph.spring
appdeck.morph.easing

appdeck.mode.duration
appdeck.mode.easing

appdeck.collapse.duration
appdeck.collapse.easing

appdeck.appearance.hideDelay
appdeck.appearance.showDuration
appdeck.appearance.hideDuration

appdeck.resting.amplitude
appdeck.resting.frequency
```

---

# 26. PERFORMANCE RULE

DILARANG:

* blur radius animasi besar
* multi heavy blur surface
* recreate layer
* expensive shader stacking

WAJIB:

* stable blur
* lightweight opacity
* shadow/elevation based depth
* GPU safe animation

---

# 27. IMPLEMENTASI QML

```qml id="e75a6n"
property string deckState: "dock"
property string deckMode: "apps"
property string intent: "browse"

property string appearanceMode: "always_show"

property bool transitioning: false

property real morphProgress: 0.0
property bool dockVisible: true
```

---

# 28. CONTENT REVEAL TIMELINE

## dock → grid

```plaintext id="4djlwm"
0.00–0.40 spatial morph
0.40–0.70 content reveal
0.70–1.00 settle
```

## grid → dock

```plaintext id="ic9mv2"
0.00–0.30 content fade out
0.30–1.00 collapse morph
```

---

# 29. LAYERING RULE

AppDeck tetap SATU surface.

Boleh:

* ghost layer
* morph proxy
* clip helper
* shadow helper

Tidak boleh:

* separate dock window
* separate grid window
* fake overlay replacement

---

# 30. VALIDATION CHECKLIST

Agent WAJIB memastikan:

* hanya satu AppDeck
* terminology konsisten
* state/mode valid
* dock ↔ grid memakai morph
* outside click collapse
* app activation collapse
* ESC collapse
* auto hide smooth
* no teleport
* no recreate icon
* no fade-only transition
* no surface replacement
* MotionController menjadi pusat semua animasi

---

# 31. DESIGN PHILOSOPHY

AppDeck harus terasa sebagai:

```plaintext id="t84v4w"
Living Spatial Surface
```

Dock adalah:

```plaintext id="g2ypfe"
living anchor
```

Grid adalah:

```plaintext id="pfrv25"
expanded evolution of dock
```

Pulse adalah:

```plaintext id="s5skwd"
focused attention state
```

Bukan sekadar launcher atau popup.

END PROMPT
