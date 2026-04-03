# Notification UI Mock (Phase 1/2)

## Banner (Top-right, transient)

```text
┌──────────────────────────────────────────────┐
│ [icon]  App Name                    12:41    │
│         Title                                 │
│         Message preview line (max 2-3 lines) │
│         [Open] [Dismiss]                     │
└──────────────────────────────────────────────┘
```

Rules:
- Max visible banners: 3.
- Newest appears at the top.
- Hover pauses dismiss timer.
- Swipe/drag horizontal dismisses card.

## Notification Center (Persistent panel)

```text
┌────────────────────────── Notification Center ┐
│                                    [Clear All]│
│                                                │
│ App Group: File Manager                        │
│  ┌──────────────────────────────────────────┐  │
│  │ Title                                    │  │
│  │ Body preview                             │  │
│  └──────────────────────────────────────────┘  │
│                                                │
│ App Group: Browser                             │
│  ┌──────────────────────────────────────────┐  │
│  │ Download Complete                        │  │
│  │ file.zip                                 │  │
│  └──────────────────────────────────────────┘  │
└────────────────────────────────────────────────┘
```

Rules:
- Opens as right slide-in panel.
- Uses virtualized `ListView`.
- Group header by app name in phase 1 (thread grouping later).
