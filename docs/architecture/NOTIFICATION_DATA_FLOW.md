# Notification Data Flow (End-to-End)

## Flow
1. Source app sends notification:
   - `org.freedesktop.Notifications.Notify` or
   - `org.example.Desktop.Notification.Notify`.
2. `NotificationManager` normalizes payload:
   - app identity
   - title/body/icon
   - actions
   - priority/group/read/banner flags.
3. Manager updates models:
   - `notifications` (history/persistent)
   - `bannerNotifications` (transient stack).
4. Manager emits events:
   - Qt model change signals for QML.
   - D-Bus `NotificationAdded/NotificationRemoved`.
5. QML UI renders:
   - `BannerContainer` for transient cards.
   - `NotificationCenter` for persistent panel.
6. User action callback:
   - click or dismiss in QML
   - invokes manager `dismiss/clearAll/toggleCenter`.
7. Manager mutates state and publishes updated signals.

## Text Diagram

```text
App/Portal -> D-Bus Interface -> NotificationManager -> Models
                                                 |-> bannerNotifications -> BannerContainer
                                                 |-> notifications      -> NotificationCenter

QML Actions (dismiss/open/clear/toggle) -> NotificationManager -> D-Bus Signals
```

## Recovery notes
- Runtime state is in-memory in phase 1.
- Persistence/replay policy planned for phase 3 hardening.
