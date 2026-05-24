# SLM AppDeck — Unified Spatial Operating Surface Guidelines

## MASTER DESIGN & IMPLEMENTATION GUIDE FOR AGENTS

---

# 1. CORE PHILOSOPHY

AppDeck bukan:

* dock biasa
* launcher biasa
* search popup
* overview clone
* launchpad clone
* spotlight clone

AppDeck adalah:

```text id="zy7m5w"
Unified Spatial Operating Surface
```

Tujuan:
menjadikan AppDeck sebagai pusat interaksi utama SLM Desktop.

AppDeck harus terasa:

* hidup
* spatial
* premium
* motion-driven
* contextual
* intelligent
* continuous

Target experience:

* bukan meniru macOS
* bukan GNOME Overview
* bukan Windows Start Menu

Tetapi:

```text id="c5hwdz"
desktop interaction layer generasi baru
```

---

# 2. APPDECK MENTAL MODEL

User tidak boleh merasa:

```text id="yljnh0"
“launcher dibuka”
```

User harus merasa:

```text id="4o0mfx"
“workspace berubah mode interaksi”
```

Artinya:

* Dock
* Grid
* Pulse
* Context

adalah satu sistem spatial yang sama.

---

# 3. ARCHITECTURE HIERARCHY (WAJIB)

JANGAN campur semua menjadi “state”.

Gunakan hierarchy resmi:

```text id="sx9r3f"
SpatialState
 ├── dock
 └── grid

ModeState
 ├── apps
 ├── pulse
 └── context

ContextDomain
 ├── workspace
 ├── activity
 ├── system
 └── semantic
```

---

# 4. OFFICIAL APPDECK STATES

## VALID COMBINATIONS

```text id="w0y4ud"
AppDeck[state=dock, mode=apps]

AppDeck[state=grid, mode=apps]
AppDeck[state=grid, mode=pulse]
AppDeck[state=grid, mode=context]
```

INVALID:

```text id="f5q3tg"
AppDeck[state=dock, mode=pulse]
AppDeck[state=dock, mode=context]
```

---

# 5. APPDECK IS ONE SURFACE

WAJIB:

* satu AppDeck surface
* satu render tree
* satu MotionController
* satu geometry coordinator
* satu animation clock

DILARANG:

* Pulse window
* Grid window
* Dock window
* separate Wayland surface
* fake overlay replacement

PulseLayer HARUS tetap berada dalam:

```text id="c4n0fr"
same AppDeck render tree
```

---

# 6. SPATIAL STATES

## dock

Compact persistent anchor.

## grid

Expanded spatial browsing state.

Grid adalah:

```text id="0kc1gb"
evolution of dock
```

Bukan surface baru.

---

# 7. MODE STATES

## apps

Browsing mode.

## pulse

Intent/search/action mode.

## context

Contextual interaction mode.

---

# 8. PULSE PHILOSOPHY

Pulse bukan search popup.

Pulse adalah:

```text id="1w2mjv"
intent layer emerging from AppDeck
```

User harus merasa:

```text id="xt10gg"
Pulse emerges from Grid
```

Bukan:

```text id="phc7eg"
another search window opened
```

---

# 9. SEARCH FIELD AS SPATIAL ANCHOR

SearchField adalah:

```text id="n7qj9v"
spatial origin point
```

WAJIB:

* Pulse muncul dari SearchField
* transisi berpusat pada geometry SearchField
* result layer expand dari search region

DILARANG:

* Pulse muncul dari center screen random
* hard popup animation

---

# 10. SEARCH TRANSITION MODEL

## GRID BROWSING

Saat AppDeck dibuka:

* app grid visible
* categories visible
* recent apps visible
* search idle

Tujuan:

```text id="v7ov2w"
visual exploration
```

---

## SEARCH FOCUS

Saat field focus:

* grid sedikit redup
* grid sedikit mengecil
* ambience meningkat
* search elevation naik

TETAPI:

* belum masuk Pulse
* belum rebuild layout
* belum hide grid

---

## PULSE ACTIVE

Pulse aktif jika:

```text id="6j1b8h"
query.length > 0
OR command intent detected
OR semantic action detected
```

Saat aktif:

* Grid tetap ada di belakang
* Pulse emerge progressive
* SearchField tetap visible
* continuity wajib terjaga

---

# 11. EMPTY QUERY POLICY

Jika query kosong:

```text id="xwqey9"
Pulse → Grid
```

WAJIB:

* reverse morph
* restore grid prominence
* preserve continuity

DILARANG:

* abrupt reset
* popup close feeling

---

# 12. GRID BEHAVIOR DURING PULSE

Grid TIDAK BOLEH hilang.

WAJIB:

* opacity turun ringan
* blur subtle
* scale sedikit turun
* tetap spatially visible

Rekomendasi:

```text id="sop36o"
opacity: 0.88–0.92
scale: 0.98–0.985
blur: minimal
```

---

# 13. BLUR POLICY

DILARANG:

* blur radius animasi besar
* heavy realtime blur
* layered blur stacking

WAJIB:

* blur stabil
* opacity/material lebih dominan
* gunakan elevation/depth

Karena:

```text id="e9t4kk"
KWin + QtQuick blur animation mahal
```

---

# 14. MOTION PRINCIPLES

ABSOLUTELY AVOID:

* abrupt page switch
* hard replace
* opacity-only transition
* popup feeling
* modal dialog behavior

WAJIB:

* morph animation
* shared element transition
* layered motion
* spatial continuity
* anchor-based expansion

---

# 15. MORPH PRINCIPLE

Dock → Grid harus terasa:

```text id="sj94xk"
surface evolution
```

Bukan:

```text id="g7q4mw"
surface replacement
```

Grid → Pulse harus terasa:

```text id="5z8mnu"
intent emergence
```

Bukan:

```text id="3g3h4j"
new overlay
```

---

# 16. HOVER CONTINUITY

Hover HARUS tetap hidup saat:

* Grid → Pulse
* Pulse → Grid
* query update
* result refresh
* morph transition

Pointer diam tetap wajib:

```text id="u9v0du"
recompute hover target
```

---

# 17. RESULT MODEL POLICY

DILARANG:

* rebuild seluruh model tiap keystroke
* destroy/recreate sections
* reset scene graph

WAJIB:

* incremental update
* diff update
* async search pipeline
* progressive reveal

---

# 18. PULSE RESULT STRUCTURE

Gunakan grouped sections:

```text id="jlwmwq"
PulseResults
 ├── Apps
 ├── Files
 ├── Actions
 ├── Settings
 ├── Workflows
 └── Suggestions
```

---

# 19. RESULT ANIMATION POLICY

WAJIB:

* subtle stagger
* progressive reveal
* spring easing
* layered emergence

DILARANG:

* waterfall animation besar
* motion chaos
* flashy transitions

---

# 20. RESULT UX

Hover:

```text id="mjlwm9"
scale: 1.0 → 1.04
soft lift
subtle bloom
```

Selection:

```text id="qmx2hm"
magnetic motion
active glow
context preview optional
```

Durasi:

```text id="h7e8uv"
120–160ms
```

---

# 21. AUTO COLLAPSE POLICY

AppDeck collapse ke dock saat:

* app activation
* outside click
* ESC
* workspace interaction

---

# 22. OUTSIDE CLICK BEHAVIOR

Saat Pulse aktif:

FIRST outside click:

```text id="otkwlc"
Pulse → Grid
```

SECOND outside click:

```text id="d4jv8j"
Grid → Dock
```

Tujuan:
lebih premium dan spatial.

---

# 23. AUTO HIDE POLICY

Appearance modes:

```text id="8p1hsv"
always_show
auto_hide
```

Dock tidak boleh hilang total.

Gunakan:

```text id="yl0sz9"
low presence state
```

Tetap sisakan:

* reveal zone
* subtle edge glow
* spatial anchor memory

---

# 24. INPUT REGION POLICY

Dock:

```text id="t3euj7"
dockRect + revealZone
```

Grid:

```text id="s0iv04"
gridRect
```

Opacity 0 layer:

```text id="fy7j3s"
MUST NOT capture input
```

---

# 25. PERFORMANCE POLICY

WAJIB:

* persistent scene graph
* shared render lifecycle
* lightweight animations
* stable geometry caching

DILARANG:

* recreate heavy views
* aggressive Loader switching
* blur-heavy animation
* multi-surface composition

---

# 26. RENDERING OWNERSHIP

All layers MUST share:

```text id="a3zk3z"
one render root
one motion clock
one geometry coordinator
one lifecycle
```

Layers:

```text id="8ob0wl"
DockLayer
GridLayer
PulseLayer
ContextLayer
```

Bukan renderer terpisah.

---

# 27. MOTIONCONTROLLER RESPONSIBILITY

MotionController bertanggung jawab untuk:

* morph transitions
* hover physics
* spring configs
* shared transitions
* depth motion
* interruptible animation
* animation continuity

---

# 28. SEARCHCONTROLLER RESPONSIBILITY

SearchController:

* query handling
* semantic intent detection
* result grouping
* async search pipeline
* Pulse activation

---

# 29. CONTEXTCONTROLLER RESPONSIBILITY

ContextController:

* mode switching
* contextual actions
* workspace context
* activity context
* semantic context

---

# 30. SLM DESIGN LANGUAGE (IMPORTANT)

SLM harus punya identitas sendiri.

JANGAN:

* copy Apple blur
* copy GNOME Overview
* copy Windows Start
* copy visionOS glassmorphism berlebihan

SLM harus:

```text id="9s40f8"
clean
spatial
alive
minimal
motion-driven
focused
```

---

# 31. BEST RECOMMENDATION FOR SLM

## A. PRIORITASKAN CONTINUITY

Continuity lebih penting daripada fitur banyak.

User lebih merasakan:

* continuity
* responsiveness
* motion consistency

daripada:

* 100 fitur tambahan

---

## B. APPDECK HARUS JADI IDENTITAS SLM

Jangan jadikan AppDeck:

```text id="1b8jpv"
dock + launcher
```

Tetapi:

```text id="s4n7u4"
operating surface
```

Ini pembeda terbesar SLM.

---

## C. FOKUS KE SPATIAL UX

SLM punya peluang besar membuat:

```text id="aefhkm"
Linux desktop pertama dengan spatial continuity yang benar
```

Banyak desktop Linux gagal karena:

* terlalu modular
* terlalu banyak popup
* terlalu banyak surface berbeda

SLM harus:

```text id="smv2it"
one spatial environment
```

---

## D. MOTION ADALAH FITUR UTAMA

Animasi bukan kosmetik.

Motion adalah:

* orientasi user
* continuity
* focus guidance
* interaction language

---

## E. JANGAN TERLALU CEPAT MENAMBAH FITUR

Saat ini fokus:

* AppDeck
* Crown
* Workspace continuity
* MotionController
* Input consistency

Jangan dulu:

* widget chaos
* AI overlay berlebihan
* floating utilities random

---

# 32. TARGET EXPERIENCE

Saat user memakai SLM:

mereka harus merasa:

```text id="d6i9x9"
desktop terasa hidup
```

Bukan:

```text id="8svw53"
kumpulan window dan popup
```

AppDeck harus menjadi:

```text id="w6u6sn"
the central living interaction surface of SLM Desktop
```

END GUIDELINE
