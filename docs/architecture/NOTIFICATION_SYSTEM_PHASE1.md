# Notification System Phase 1 (Skeleton)

## Scope
- Introduce desktop-native notification interface: `org.example.Desktop.Notification`.
- Keep compatibility with `org.freedesktop.Notifications`.
- Add model-driven QML structure for banner + center panel.
- Preserve existing runtime behavior while enabling incremental migration.

## Implemented Layers
1. Notification Service (backend):
   - `src/services/notifications/notificationmanager.h/.cpp`
   - `src/services/notifications/desktopnotificationadaptor.h/.cpp`
2. Notification UI (QML):
   - `Qml/components/notification/NotificationManager.qml`
   - `Qml/components/notification/BannerContainer.qml`
   - `Qml/components/notification/NotificationCard.qml`
   - `Qml/components/notification/NotificationCenter.qml`
   - `Qml/components/notification/NotificationGroup.qml`
3. Integration:
   - `Meta+N` toggles center (`Qml/DesktopScene.qml`)
   - `ShellSystemWindows` instantiates new notification manager component.

## D-Bus Contracts
- `org.freedesktop.Notifications` remains available on `/org/freedesktop/Notifications`.
- New interface:
  - service: `org.example.Desktop.Notification`
  - path: `/org/example/Desktop/Notification`
  - methods: `Notify`, `Dismiss`, `GetAll`, `ClearAll`, `ToggleCenter`
  - signals: `NotificationAdded`, `NotificationRemoved`
- Introspection spec:
  - `docs/contracts/org.example.Desktop.Notification.xml`

## Data Model (Phase 1)
- `id`
- `app_id`
- `app_name`
- `title` (mapped from summary)
- `body`
- `icon`
- `timestamp`
- `actions[]`
- `priority` (`low|normal|high`)
- `group_id`
- `read`

## Animation Baseline
- Center panel slide uses 300ms ease-out.
- Dim overlay fade uses 180ms ease-out.
- Banner stack supports:
  - max visible = 3
  - hover pause
  - swipe/drag dismiss
  - timed auto-dismiss for non-sticky priorities.

## Data Flow
1. App emits notification via freedesktop or desktop-native interface.
2. `NotificationManager` normalizes payload -> store model.
3. Model update emits:
   - `NotificationAdded`/`NotificationRemoved` (D-Bus)
   - Qt model change signals for QML views.
4. QML layer consumes model:
   - transient stack (`BannerContainer`)
   - persistent list (`NotificationCenter`)
5. User action (dismiss/clear/toggle) flows back to manager and D-Bus state.

## Known Gaps (Phase 2)
- Real blur/elevation polish for cards.
- Thread-level grouping (current grouping baseline is by app).
- Priority routing refinements (`high` sticky, fullscreen/gaming policies).
- Persistence retention strategy and recovery restore.
- Inline action rendering and callbacks (currently stored in model only).
