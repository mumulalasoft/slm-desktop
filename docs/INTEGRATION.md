Bangun integrasi desktop yang menyatukan Shell, Topbar, Dock, Workspace Manager, Launchpad, File Manager, Notification System, dan Settings sebagai satu sistem desktop terpadu. Jangan implementasikan komponen-komponen ini sebagai modul UI yang berdiri sendiri. Semua komponen harus berbagi state global, kontrak interaksi yang sama, model focus yang sama, model layer yang sama, dan sumber data yang sama.

Tujuan utama implementasi ini adalah menciptakan desktop yang terasa sebagai satu sistem utuh, bukan kumpulan fitur terpisah. Shell harus menjadi orkestrator global. Topbar, Dock, Launchpad, Workspace surfaces, Notification surfaces, popup sistem, Settings surfaces, Search surfaces, dan transient surfaces harus diperlakukan sebagai shell-owned surfaces atau surface yang terintegrasi langsung dengan policy Shell.

Aturan arsitektur utama:

Shell harus menjadi pemilik tunggal untuk:
- layer/z-order policy
- focus arbitration
- workspace context
- transient surface policy
- global drag-and-drop session
- activation routing
- launchpad visibility
- notification routing
- settings propagation
- session state
- theme and motion propagation

Topbar harus menjadi shell-owned surface, bukan panel biasa. Topbar bertanggung jawab untuk:
- status area
- jam
- jaringan
- baterai
- audio
- workspace indicator
- entry point popup sistem
- quick settings
- notification entry
- popup yang ter-anchor ke applet pemicu

Semua popup Topbar harus dikelola oleh Shell. Popup Topbar tidak boleh ditangani lokal tanpa koordinasi ke Shell. Popup Topbar harus selalu berada di atas app windows dan mengikuti focus/dismiss policy global.

Dock harus menjadi permukaan navigasi aplikasi global dan tidak boleh memiliki model aplikasi sendiri. Dock wajib membaca pinned apps, running apps, active app, badges, progress, recent files per app, dan quick actions dari service global yang sama. Dock harus selalu sinkron dengan Workspace, Launchpad, File Manager, Notification, dan Settings. Dock harus menjadi layer paling atas untuk permukaan desktop utama.

Workspace Manager tidak boleh hanya menjadi animasi perpindahan. Workspace Manager harus memiliki model nyata untuk:
- workspace list
- active workspace
- window membership per workspace
- active window per workspace
- active app per workspace
- focus restore saat pindah workspace
- move window across workspaces
- overview state

Launchpad harus menjadi mode milik Shell, bukan aplikasi terpisah. Launchpad wajib:
- membaca daftar aplikasi dari app registry global
- membaca hasil search dari search service global
- membaca status running apps dari source global yang sama dengan Dock
- muncul di atas aplikasi
- berada di bawah Dock
- tidak boleh memiliki model aplikasi sendiri
- tidak boleh membentuk dunia UI kedua yang terpisah dari desktop utama

File Manager harus menjadi pusat objek desktop, bukan sekadar browser folder. File Manager wajib:
- membaca volumes dari storage service global
- membaca recent files dan recent folders dari service global
- menggunakan open-with dari app registry/action service global
- menggunakan file operation service global
- mengirim progress operasi file ke notification/progress system global
- memakai contextual actions dari action service global
- sinkron dengan Search, Dock, Workspace context, Notification, dan Settings

Notification System harus menjadi bagian terintegrasi dari Shell policy, bukan overlay liar. Notification System wajib:
- menerima event notifikasi dari service global
- menampilkan bubble notifikasi pada layer yang benar
- mendukung stacked notifications
- mendukung dismiss manual dan auto-dismiss policy
- mendukung action buttons
- mendukung progress notification untuk file operations atau task tertentu
- sinkron dengan Notification Center bila ada
- tidak boleh memblokir mouse event di area kosong di luar bubble
- surface bubble notifikasi harus hanya seluas bubble aktual
- area transparan atau kosong di sekitar bubble tidak boleh ikut menangkap pointer event
- setiap bubble harus memiliki input region yang pas dengan geometri bubble
- beberapa bubble yang ditumpuk harus tetap menjaga pointer hit area akurat per bubble
- bubble notifikasi tidak boleh membuat area layar yang kosong menjadi tidak bisa diklik

Settings harus menjadi bagian dari integrasi desktop, bukan halaman terpisah yang punya dunia state sendiri. Settings wajib:
- membaca dan menulis ke settings service global
- menjadi sumber konfigurasi tunggal untuk theme, accent, icon theme, motion policy, reduce motion, dock behavior, notification behavior, workspace behavior, launchpad behavior, file manager behavior, dan shell policy lain
- mempropagasikan perubahan ke Shell, Topbar, Dock, Launchpad, File Manager, dan Notification secara konsisten
- tidak boleh membuat konfigurasi visual atau perilaku lokal yang tidak tunduk pada settings service global

Service minimum yang wajib ada:
- desktop-shelld
- desktop-appd
- desktop-workspaced
- desktop-storaged
- desktop-fileopsd
- desktop-actiond
- desktop-searchd
- desktop-notifyd
- desktop-settingsd

Tanggung jawab service:
- desktop-shelld: shell orchestration, layer policy, focus policy, transient surface policy, launchpad visibility, activation routing, drag session broker, notification routing
- desktop-appd: app registry, pinned apps, running apps, app-window mapping, icon resolution, app metadata, quick actions
- desktop-workspaced: workspace list, active workspace, workspace-window mapping, focus restore, overview model
- desktop-storaged: mounted volumes, removable devices, bookmarks, recent folders, trash metadata
- desktop-fileopsd: copy, move, delete, restore, rename, conflict handling, progress, undo hooks
- desktop-actiond: open-with, share, send-to, contextual actions
- desktop-searchd: app search, file search, action search, settings search, ranking broker
- desktop-notifyd: notifications, badges, urgent state, progress surfacing, notification queue, bubble lifecycle
- desktop-settingsd: theme, icon theme, accent, motion policy, reduce motion, dock behavior, workspace behavior, launchpad behavior, file manager behavior, notification behavior

State global minimal yang wajib punya owner tunggal:
- activeWorkspaceId
- workspaceIds
- workspaceWindowMap
- activeWindowId
- activeAppId
- runningApps
- pinnedApps
- appWindowMap
- launchpadVisible
- searchVisible
- searchQuery
- dockVisibilityMode
- dockHoveredItem
- dockExpandedItem
- dragSession
- mountedVolumes
- recentFiles
- recentFolders
- selectedObjects
- fileOperationQueue
- notificationQueue
- notificationCenterState
- bubbleNotificationState
- themeMode
- accentColor
- iconTheme
- reduceMotion
- sessionRestoreState

Tidak boleh ada duplicate state untuk konsep yang sama. Dock, Launchpad, Workspace UI, File Manager, Notification UI, dan Settings UI tidak boleh menyimpan versi state mereka sendiri untuk pinned apps, running apps, active app, recent files, mounted volumes, notification queue, app list, atau theme state.

Aturan layer dan surface yang wajib:

Definisikan surface roles secara eksplisit untuk:
- wallpaper/background surface
- app windows
- shell base surfaces
- topbar surface
- workspace overview surfaces
- launchpad surface
- dock surface
- notification bubble surfaces
- notification center surface
- settings surfaces
- shell popup surfaces
- shell modal/transient surfaces

Urutan layer wajib:
1. wallpaper/background
2. app windows
3. shell base surfaces
4. topbar anchored surfaces
5. workspace overview / shell overview surfaces bila dipakai
6. launchpad surface
7. notification center / settings auxiliary shell surfaces bila relevan sesuai policy
8. dock surface
9. shell popup/transient/system modal surfaces sesuai policy yang lebih tinggi bila diperlukan

Aturan wajib yang tidak boleh dilanggar:
- Launchpad harus selalu berada di atas aplikasi
- Dock surface harus selalu berada di atas Launchpad
- Dock harus menjadi layer paling atas untuk permukaan desktop utama
- Topbar harus tetap menjadi shell-owned anchored surface yang stabil
- Bubble notifikasi harus tampil pada layer yang benar tanpa membuat area kosong ikut menangkap mouse event
- Surface bubble notifikasi harus hanya seluas bubble aktual
- Input region bubble notifikasi harus mengikuti bentuk/geometri bubble
- Area di luar bubble tidak boleh intercept pointer event
- Popup Topbar dan popup Shell harus mengikuti policy Shell, bukan sekadar z-value lokal
- Urutan visual tidak boleh bergantung pada window creation order
- Jangan gunakan z-index QML untuk mengatur antar-surface Wayland
- Jangan gunakan WindowStaysOnTopHint sebagai solusi utama
- Jangan gunakan raise() loop sebagai solusi layering

Aturan focus wajib:
- hanya ada satu active app global
- hanya ada satu active window global
- hanya ada satu active workspace global
- klik item Dock harus deterministik
- buka file dari File Manager harus route melalui Shell activation policy
- close Launchpad harus restore focus ke target sebelumnya
- switch workspace harus restore focus secara deterministik
- interaksi dengan bubble notifikasi tidak boleh merusak focus policy global
- bubble notifikasi yang tidak aktif tidak boleh membuat area layar kosong menjadi non-interaktif

Aturan klik Dock wajib:
- jika app belum running, launch
- jika app running dan punya window di workspace aktif, focus ke last active window
- jika app running hanya di workspace lain, switch atau reveal berdasarkan policy global
- jika app punya banyak window, cycle atau reveal sesuai policy global

Aturan notifikasi wajib:
- bubble notifikasi hanya menerima pointer event pada area bubble yang terlihat
- background transparan di sekitar bubble harus click-through
- stacked notifications harus punya hit area terpisah dan akurat
- action button pada bubble harus dapat diklik normal
- dismiss gesture atau close button hanya berlaku pada bubble terkait
- bubble yang hilang harus melepas input region dengan benar
- notification center dan bubble toast harus berbagi source state yang sama
- progress notifications harus sinkron dengan file operation service
- perubahan setting notification harus dipropagasikan ke perilaku bubble dan notification center

Aturan drag-and-drop wajib global:
- Shell harus menjadi broker drag session global
- drag file dari File Manager ke app di Dock harus didukung
- drag file ke shortcut/folder target di Dock harus didukung bila fitur itu ada
- drag item ke Trash harus didukung
- drag window ke workspace lain harus didukung bila overview/workspace UI mendukung
- feedback hover/drop harus konsisten di semua target

Payload drag minimum harus memuat:
- source component
- object type
- mime/capabilities
- allowed operations
- preferred action
- target hints

Recent files harus global dan harus bisa dipakai oleh:
- Dock quick actions
- Launchpad recommendations
- Search
- File Manager
- future session recovery

Open With harus memakai app registry yang sama dengan Dock dan Launchpad. Tidak boleh ada perbedaan app name, icon, category, capability, atau default handler resolution.

Search harus satu backend global untuk:
- app
- file
- action
- setting

Semua komponen wajib tunduk pada desktop-settingsd untuk:
- theme mode
- accent
- icon theme
- animation policy
- reduce motion
- dock behavior
- workspace behavior
- launchpad behavior
- file manager behavior
- notification behavior
- topbar behavior

Dilarang:
- Dock punya app list sendiri
- Launchpad punya app list sendiri
- File Manager scan asosiasi app sendiri di luar app registry global
- Workspace hanya menjadi animasi tanpa model
- popup Topbar dikelola lokal tanpa Shell
- recent files hidup hanya di File Manager
- volumes hanya diketahui File Manager
- Notification UI punya queue sendiri yang tidak sinkron dengan desktop-notifyd
- Settings UI punya state konfigurasi sendiri yang tidak sinkron dengan desktop-settingsd
- search Shell dan search Launchpad memakai backend berbeda
- icon resolution berbeda antara Dock dan Launchpad
- layering diselesaikan dengan hack z-order lokal
- bubble notification menggunakan surface besar transparan yang menangkap mouse event di area kosong

Urutan implementasi wajib:
1. definisikan owner state dan service contracts
2. implementasikan desktop-shelld sebagai orkestrator global
3. implementasikan app registry tunggal
4. implementasikan workspace model tunggal
5. implementasikan notification model tunggal
6. implementasikan settings model tunggal
7. hubungkan Dock ke app registry dan workspace state
8. hubungkan Launchpad ke app registry dan search service
9. hubungkan File Manager ke storage, fileops, action, dan recent services
10. implementasikan Notification bubble surfaces dengan input region akurat
11. implementasikan layer/surface policy yang benar
12. implementasikan focus routing dan activation routing
13. implementasikan drag-and-drop global
14. implementasikan notifikasi, badge, dan progress surfacing
15. terakhir baru UI polish dan animasi

Skenario yang wajib lolos:
- saat app terbuka dan Launchpad dibuka, Launchpad harus berada di atas aplikasi
- saat Launchpad terbuka, Dock harus tetap berada di atas Launchpad
- Dock tidak boleh pernah tampil di belakang Launchpad
- Dock tidak boleh pernah tampil di belakang Shell base surface
- popup Topbar harus selalu tampil pada layer yang benar
- bubble notifikasi hanya menangkap mouse event pada area bubble
- area transparan di sekitar bubble notifikasi harus click-through
- stacked bubble notifikasi tetap akurat hit area-nya
- klik area kosong di belakang bubble notifikasi harus tetap sampai ke surface di bawahnya
- klik app di Dock harus menghasilkan focus/launch yang deterministik
- buka file dari File Manager harus resolve app dan workspace secara konsisten
- drag file dari File Manager ke Dock app harus berhasil
- progress file operation harus muncul di File Manager dan Shell notification/progress surface
- pindah workspace harus menjaga sinkronisasi Dock, active app, dan active window
- Search harus konsisten dengan app metadata dan file metadata yang dipakai komponen lain
- perubahan theme, motion, dan notification behavior harus sinkron ke semua komponen

Kriteria selesai:
- tidak ada duplicate state untuk app, workspace, recent files, volume, focus, app list, notification queue, dan settings state
- Shell benar-benar menjadi orkestrator layer, focus, activation, popup, notification surface, dan transient surfaces
- Launchpad selalu di atas aplikasi
- Dock surface selalu di atas Launchpad
- Dock menjadi layer paling atas untuk permukaan desktop utama
- bubble notifikasi tidak memblokir mouse event di area kosong
- surface bubble notifikasi hanya seluas bubble aktual
- Topbar terintegrasi sebagai shell-owned anchored surface
- File Manager tidak memiliki dunia data sendiri
- Notification System tidak memiliki dunia state sendiri di luar desktop-notifyd
- Settings tidak memiliki dunia state sendiri di luar desktop-settingsd
- semua komponen memakai service global yang sama
- semua skenario uji lolos

Hasil yang harus dikirim:
- diagram komponen dan service
- daftar owner state
- kontrak IPC/event antar service
- kontrak layer/surface
- kontrak focus
- kontrak workspace
- kontrak drag-and-drop
- kontrak notification bubble input region
- kontrak recent files/open-with/actions
- kontrak settings propagation
- daftar anti-pattern yang dihindari
- daftar skenario uji yang telah dipenuhi
