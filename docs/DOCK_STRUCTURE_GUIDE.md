# APPDECK STRUCTURE GUIDE (CURRENT)

## Keputusan final

- Hanya ada **1 surface aplikasi**: `Qml/components/overlay/AppDeckWindow.qml`.
- `AppDeckWindow` adalah layer-shell surface yang memiliki state:
  - `collapsed` (quick access)
  - `expanded` (AppHub mode)
  - `context` (Pulse result mode)
  - `hidden` (opsional)
- Tidak ada window launcher terpisah untuk AppHub/Pulse.
- Tidak ada host ganda AppDeck.

## Stacking target

```text
AppDeckWindow (TOP / layer-shell)
Application windows
Shell ApplicationWindow scene
```

## Arsitektur komponen

- AppDeck root:
  - `Qml/components/overlay/AppDeckWindow.qml`
- Sub-view:
  - `Qml/components/appdeck/AppDeckCollapsedView.qml`
  - `Qml/components/appdeck/AppDeckExpandedView.qml`
  - `Qml/components/appdeck/AppDeckContextView.qml`
- Renderer collapsed:
  - `Qml/components/appdeck/AppDeck.qml`
- Input/state interaction:
  - `Qml/AppDeckController.qml`

## Rules wajib

- Jangan membuat ulang:
  - `DockWindow`
  - `LaunchpadWindow`
  - `SearchPopup`
  - `AppHubWindow`
  - `PulseWindow`
- Semua trigger launcher harus menuju API mode di `AppDeckWindow`:
  - `enterCollapsedMode()`
  - `enterExpandedMode()`
  - `enterContextMode()`
  - `enterHiddenMode()`
- `Pulse` hanya logic provider; hasil dirender di `AppDeckContextView`.

## Checklist validasi

- [ ] `Main.qml` hanya memuat `AppDeckWindow` untuk surface aplikasi.
- [ ] Tidak ada loader aktif untuk `AppHubWindow`/`PulseWindow`/`PulseDismissWindow`.
- [ ] `AppDeckWindow` memuat collapsed + expanded + context views.
- [ ] Tidak ada referensi `AppDeckSystem`.
- [ ] `qmllint` untuk file AppDeck utama lulus.

## Prinsip pegangan

> SLM hanya punya satu surface aplikasi: AppDeckWindow.
> AppHub dan Pulse adalah mode/view di dalam surface yang sama.
