LANDASAN KONSEP APPDECK — SLM DESKTOP
Dokumen ini adalah referensi dasar arsitektur dan UX. Jangan menafsirkan AppDeck sebagai dock biasa, launchpad biasa, atau gabungan tempelan keduanya. AppDeck adalah sistem inti baru yang harus dipahami secara konseptual sebelum implementasi dimulai.

==================================================
1. DEFINISI INTI
==================================================

AppDeck adalah surface aplikasi utama pada desktop SLM.

AppDeck bukan:
- bukan clone Dock macOS
- bukan clone Launchpad macOS
- bukan taskbar Windows
- bukan launcher grid biasa
- bukan popup pencarian
- bukan panel berisi ikon statis

AppDeck adalah:
- single intelligent app surface
- adaptive application access layer
- pusat akses aplikasi, status aplikasi, discovery, dan context results
- satu sistem dengan banyak state, bukan banyak komponen terpisah

Konsep utama:
“AppDeck bukan tempat ikon aplikasi tinggal.
AppDeck adalah tempat sistem menyajikan apa yang paling relevan untuk pengguna.”

==================================================
2. POSISI APPDECK DALAM EKOSISTEM SLM
==================================================

SLM memiliki peran komponen berikut:

- Crown:
  layer status, konteks global, kontrol sistem, global menu, session indicators
- Pulse:
  engine intent, search, command, parsing input, pemicu perubahan isi AppDeck
- AppDeck:
  surface aplikasi utama yang selalu menjadi pusat interaksi aplikasi
- AppHub:
  bukan komponen terpisah, melainkan mode discovery/full view di dalam AppDeck

Hubungan antarkomponen:
- Crown memberi konteks global
- Pulse memberi niat / intent
- AppDeck memberi tindakan / presentation layer
- AppHub adalah state dari AppDeck, bukan launcher lain

Formula arsitektur:
Crown = konteks global
Pulse = otak / intent
AppDeck = surface aksi
AppHub = expanded discovery mode inside AppDeck

==================================================
3. MASALAH YANG HARUS DIHINDARI
==================================================

Jangan kembali ke pola lama yang terpecah:
- DockWindow terpisah
- LaunchpadWindow terpisah
- Search results popup terpisah
- surface saling tumpang tindih dan saling berebut layer
- banyak entry point untuk fungsi aplikasi

Masalah yang ingin diselesaikan AppDeck:
- redundansi antara dock dan launchpad
- konflik z-order / stacking
- UI launcher yang terpisah-pisah
- kebingungan user: buka aplikasi dari mana, cari dari mana, lihat running apps di mana
- agent cenderung kembali membuat dock klasik jika tidak diberi landasan konsep yang kuat

==================================================
4. PRINSIP ARSITEKTUR
==================================================

Prinsip 1:
Hanya ada satu surface aplikasi utama: AppDeck.

Prinsip 2:
Tidak boleh ada launcher aplikasi lain di luar AppDeck.

Prinsip 3:
Pulse tidak merender hasil dalam popup terpisah; Pulse hanya mengendalikan isi/state AppDeck.

Prinsip 4:
AppHub bukan window atau komponen terpisah; AppHub hanyalah salah satu mode AppDeck.

Prinsip 5:
AppDeck harus bisa tampil dalam beberapa state tanpa memecah diri menjadi banyak window launcher.

Prinsip 6:
AppDeck adalah evolusi konsep dock + launchpad + context surface, tetapi hasil akhirnya harus tampil sebagai sistem baru yang original.

==================================================
5. IDENTITAS UX APPDECK
==================================================

Karakter AppDeck:
- futuristik
- hidup
- adaptif
- presisi
- tidak berat
- tidak ramai
- tidak generik
- tidak terasa sebagai “launcher grid biasa”

AppDeck harus terasa seperti:
- living surface
- workspace entry layer
- sistem yang bereaksi terhadap konteks
- UI yang berubah bentuk sesuai kebutuhan, bukan layout statis permanen

Perbedaan dengan Dock macOS:
- Dock macOS bersifat linear, statis, shortcut-oriented
- AppDeck bersifat adaptive, multi-state, context-driven

Perbedaan dengan Launchpad:
- Launchpad adalah katalog aplikasi
- AppDeck adalah surface kerja dan discovery yang aktif

==================================================
6. TUJUAN UX APPDECK
==================================================

AppDeck harus mampu menangani kebutuhan berikut dalam satu sistem:
- quick access ke aplikasi yang sering dipakai
- indikator aplikasi yang sedang berjalan
- discovery semua aplikasi
- context-aware suggestions
- hasil pencarian / command / recent / file results dari Pulse
- transisi halus antar mode tanpa kesan pindah ke sistem lain

Artinya:
AppDeck harus terasa sebagai satu permukaan yang berubah fungsi secara elegan.

==================================================
7. STATE DASAR APPDECK
==================================================

Minimal AppDeck memiliki state konseptual berikut:

1. collapsed
- mode ringkas
- berfungsi seperti quick access surface
- menampilkan pinned apps / running apps / essential shortcuts
- ini bukan dock klasik, tetapi state AppDeck yang paling kecil

2. expanded
- mode terbuka
- menampilkan struktur aplikasi lebih lengkap
- dipakai untuk discovery / browsing apps
- ini adalah basis AppHub mode

3. context
- mode responsif terhadap Pulse atau konteks sistem
- isi bisa berupa apps, files, commands, actions, recent items, suggestions
- bukan grid tetap
- layout bisa menyesuaikan konteks

4. hidden atau immersive (opsional)
- untuk fullscreen immersion / kondisi tertentu
- hanya jika diperlukan oleh desain shell

Catatan:
State ini harus dipahami sebagai satu komponen yang berubah bentuk, bukan banyak komponen berbeda.

==================================================
8. APPHUB DALAM KONSEP BARU
==================================================

AppHub tidak dihapus, tetapi diredefinisi.

AppHub yang benar:
- AppHub = discovery mode dari AppDeck
- AppHub = expanded_full state
- AppHub = tampilan lengkap saat user ingin melihat semua aplikasi atau kategori aplikasi

AppHub yang salah:
- window launcher terpisah
- sistem kedua selain AppDeck
- komponen yang punya life cycle sendiri di luar AppDeck

Kesimpulan:
Pertahankan nama AppHub sebagai mode/experience, bukan sebagai entitas UI terpisah.

==================================================
9. HUBUNGAN DENGAN PULSE
==================================================

Pulse adalah engine intent dan search, bukan surface hasil.

Saat user memicu Pulse:
- Pulse menerima input
- Pulse memproses intent / query / keyword / command
- AppDeck masuk ke state context
- hasil ditampilkan di AppDeck

Jangan membuat:
- popup hasil search
- list hasil terpisah
- overlay baru yang berdiri sendiri di luar AppDeck

Prinsip:
Pulse berpikir, AppDeck menampilkan.

==================================================
10. HUBUNGAN DENGAN CROWN
==================================================

Crown tetap terpisah dari AppDeck secara fungsi.

Crown menangani:
- system indicators
- global context
- global menu
- session controls
- entry ringan ke Pulse atau control center

Crown tidak boleh berubah menjadi launcher aplikasi.

Crown boleh memicu:
- Pulse
- AppDeck expanded mode
- notification center
- calendar
- control center

Tetapi hasil atau surface aplikasi tetap berada di AppDeck.

==================================================
11. LANDASAN VISUAL DAN MOTION
==================================================

Arah visual:
- clean
- premium
- modern
- tidak terlalu ornamental
- tidak terlalu datar
- tidak terlalu macOS
- tidak terlalu Windows
- original Linux desktop identity

Arah motion:
- halus
- ringan
- transisi punya bobot
- bukan sekadar fade biasa
- tidak agresif
- tidak ramai
- terasa “alive” namun tetap elegan

Karakter motion AppDeck:
- muncul dan berubah bentuk dengan lembut
- transisi antar state harus terasa seperti surface yang sama
- jangan terasa seperti mengganti halaman / membuka aplikasi baru / berpindah window launcher

==================================================
12. HAL YANG WAJIB DIJAGA AGAR TIDAK MENJADI CLONE
==================================================

Hindari:
- dock garis lurus yang jelas meniru macOS
- magnification effect ala dock macOS
- launchpad grid penuh layar yang terlalu mirip Apple
- folder bubble ala launchpad
- search popup ala Spotlight
- taskbar ala Windows
- panel app list ala Linux klasik

Kejar pembeda:
- satu surface, banyak state
- adaptive layout
- konteks mempengaruhi isi
- pengalaman terasa hidup
- struktur dan transisi menunjukkan ini adalah sistem baru

==================================================
13. OUTPUT YANG DIHARAPKAN DARI AGENT
==================================================

Sebelum coding, agent harus memahami bahwa:
- AppDeck adalah core application surface
- AppHub adalah mode di dalam AppDeck
- Pulse tidak punya UI hasil sendiri
- Crown tidak mengambil fungsi launcher
- semua aplikasi, discovery, running state, dan context results harus mengalir ke dalam sistem AppDeck

Agent tidak boleh langsung mulai membuat:
- dock klasik
- launchpad window
- popup search
- taskbar baru
- app grid statis
tanpa memetakan semuanya ke konsep AppDeck terlebih dahulu

==================================================
14. KALIMAT KUNCI UNTUK MENJAGA ARAH IMPLEMENTASI
==================================================

Pegang kalimat ini selama implementasi:

“AppDeck adalah satu surface aplikasi utama yang berubah state sesuai kebutuhan pengguna.”

“Pulse adalah otak, bukan hasil UI.”

“AppHub adalah mode AppDeck, bukan launcher terpisah.”

“Crown memberi konteks global, bukan akses aplikasi utama.”

“SLM tidak memiliki banyak launcher; SLM hanya memiliki AppDeck.”

==================================================
15. TUGAS AGENT SETELAH MEMAHAMI DOKUMEN INI
==================================================

Setelah memahami landasan ini, baru lanjut ke tahap berikut:
- menyusun arsitektur komponen
- menyusun state machine AppDeck
- memetakan peran Pulse, Crown, AppHub
- menentukan layering rules
- menyusun interaction flow
- menentukan visual hierarchy
- lalu baru implementasi QML/Wayland surface

Jangan melompat langsung ke rendering UI tanpa menyelaraskan arsitektur berdasarkan landasan ini.

SLM DESKTOP — APPDECK ARCHITECTURE SPEC (ANTI-FAIL)

Tujuan:
Membangun sistem desktop berbasis AppDeck sebagai single application surface dengan arsitektur konsisten, tanpa multi-launcher, tanpa konflik layer, dan tanpa UI redundan.

Agent WAJIB mengikuti seluruh aturan di bawah ini. Pelanggaran terhadap rules dianggap kegagalan implementasi.

==================================================
0. SCOPE IMPLEMENTASI
==================================================

Implement komponen berikut:

- Crown (Top Layer)
- AppDeck (Core Surface)
- Workspace (App Windows Layer)
- Pulse (Logic Engine, non-UI)
- AppHub (Mode di dalam AppDeck)

JANGAN membuat:
- DockWindow
- LaunchpadWindow
- SearchPopupWindow
- Taskbar
- Panel launcher lain

==================================================
1. GLOBAL LAYERING ARCHITECTURE
==================================================

Struktur layer WAJIB seperti ini:

Layer Z-order (top → bottom):

1. CrownLayer
2. AppDeckLayer
3. WorkspaceLayer (aplikasi)
4. DesktopLayer (wallpaper)

Constraint:
- CrownLayer ALWAYS ON TOP
- AppDeckLayer ALWAYS ABOVE WorkspaceLayer
- WorkspaceLayer TIDAK BOLEH overlap AppDeck
- Tidak ada surface lain di atas AppDeck selain Crown

Implementasi (QML conceptual):

RootWindow
 ├── CrownLayer (z: 300)
 ├── AppDeckLayer (z: 200)
 ├── WorkspaceLayer (z: 100)
 └── DesktopLayer (z: 0)

DILARANG:
- membuat AppDeck sebagai window terpisah tanpa kontrol z global
- menggunakan Qt.WindowStaysOnTopHint sebagai solusi utama
- membuat multiple top-level window untuk launcher

==================================================
2. APPDECK CORE ARCHITECTURE
==================================================

AppDeck adalah SATU komponen QML utama.

File:
AppDeck.qml

Struktur internal:

AppDeck
 ├── StateMachine
 ├── LayoutEngine
 ├── ZoneManager
 ├── AnimationController
 └── ContentRenderer

==================================================
3. APPDECK STATE MACHINE (WAJIB)
==================================================

State minimum:

- collapsed
- expanded
- context
- hidden (optional)

Definisi:

collapsed:
- compact mode
- tampil seperti quick access
- berisi pinned + running apps

expanded:
- discovery mode (AppHub)
- tampil semua aplikasi

context:
- hasil Pulse / context system
- isi dinamis (apps, files, actions)

hidden:
- optional untuk immersive mode

QML pseudo:

states: [
  State { name: "collapsed" },
  State { name: "expanded" },
  State { name: "context" },
  State { name: "hidden" }
]

Transitions HARUS smooth (NumberAnimation / Behavior)

==================================================
4. ZONE SYSTEM (APPDECK INTERNAL)
==================================================

AppDeck tidak boleh flat grid saja.

Minimal zones:

- FavoritesZone
- RunningZone
- SuggestedZone
- ContextZone

Rules:
- zone bisa muncul/hilang
- ukuran zone adaptif
- posisi tidak harus fixed

ZoneManager bertanggung jawab:
- layout distribusi
- visibilitas zone
- priority berdasarkan context

==================================================
5. PULSE INTEGRATION (STRICT RULE)
==================================================

Pulse TIDAK BOLEH punya UI hasil sendiri.

Flow:

User input → PulseEngine → emit result → AppDeck.updateContext()

DILARANG:
- membuat popup hasil search
- membuat overlay list hasil
- membuat window search

AppDeck WAJIB handle:
- render hasil
- highlight app
- tampilkan file/action

==================================================
6. APPHUB IMPLEMENTATION
==================================================

AppHub BUKAN komponen.

Implementasi:

IF AppDeck.state == "expanded"
→ itu adalah AppHub mode

DILARANG:
- membuat AppHubWindow
- membuat route UI terpisah

==================================================
7. CROWN INTEGRATION
==================================================

CrownLayer adalah independent UI.

Tugas:
- system indicators
- global menu
- trigger Pulse
- trigger AppDeck state

Crown TIDAK BOLEH:
- menampilkan app launcher
- menampilkan search result
- menggantikan fungsi AppDeck

==================================================
8. WORKSPACE INTEGRATION
==================================================

WorkspaceLayer:
- berisi semua window aplikasi
- tidak aware AppDeck secara visual

AppDeck:
- overlay di atas Workspace
- tidak mengubah window aplikasi secara langsung

==================================================
9. ANIMATION SYSTEM (WAJIB ADA)
==================================================

Gunakan:
- Behavior on x/y/scale/opacity
- NumberAnimation
- Easing (InOutCubic / OutQuad)

Prinsip:
- tidak ada hard jump
- semua transisi halus
- terasa satu surface berubah bentuk

DILARANG:
- instant switch tanpa animasi
- fade-only tanpa transform

==================================================
10. LAYOUT ENGINE (ANTI STATIC GRID)
==================================================

JANGAN buat layout seperti:
- grid tetap full screen
- dock linear statis

WAJIB:
- adaptive layout
- responsive terhadap jumlah item
- bisa berubah dari compact → grid → cluster

==================================================
11. EVENT FLOW (WAJIB JELAS)
==================================================

Trigger utama:

1. User klik icon AppDeck
   → state = expanded

2. User input (Pulse)
   → state = context

3. User keluar
   → state = collapsed

4. Fullscreen app
   → state = hidden (optional)

Gunakan signal/slot atau QML Connections.

==================================================
12. ANTI-FAIL RULES (KRITIS)
==================================================

Jika agent melakukan salah satu ini → FAIL:

- membuat DockWindow
- membuat LaunchpadWindow
- membuat SearchPopup
- membuat AppHub sebagai window
- membuat multiple launcher UI
- tidak menggunakan state machine
- AppDeck bukan single component
- Pulse punya UI sendiri
- layout statis tanpa adaptasi
- z-index konflik

==================================================
13. DELIVERABLE YANG WAJIB
==================================================

Agent harus menghasilkan:

1. Struktur file:
   - AppDeck.qml
   - Crown.qml
   - PulseEngine (C++/logic)
   - Zone components

2. State machine implementasi

3. Layout adaptive system

4. Integration flow:
   Pulse → AppDeck

5. Layering system (z-order fix)

==================================================
14. VALIDATION CHECKLIST
==================================================

Checklist sebelum selesai:

[ ] Tidak ada DockWindow
[ ] Tidak ada LaunchpadWindow
[ ] Tidak ada SearchPopup
[ ] AppDeck satu komponen utama
[ ] Pulse tidak punya UI
[ ] AppHub hanya state
[ ] Layering sesuai spec
[ ] Transisi smooth
[ ] Layout adaptif
[ ] Tidak terasa seperti macOS clone

==================================================
15. FINAL PRINCIPLE
==================================================

Agent harus selalu kembali ke prinsip ini:

“SLM hanya memiliki satu surface aplikasi: AppDeck.
Semua interaksi aplikasi harus terjadi di dalamnya.”

END OF ARCHITECTURE SPEC
