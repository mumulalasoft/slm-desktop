# Notification Animation Spec (Phase 1)

## Banner
- Entry: implicit list insertion + opacity/position easing.
- Exit: `opacity 1 -> 0`, `scale 1 -> 0.96`, `x 0 -> width`.
- Duration:
  - fade/scale: `170ms`
  - horizontal exit: `180ms`
- Dismiss finalize delay: `190ms` (allows exit animation to complete).

## Notification Center
- Panel slide:
  - `x: width -> width - panelWidth`
  - duration: `300ms`
  - easing: `OutCubic`
- Dim overlay:
  - `opacity: 0 -> 1`
  - duration: `180ms`
  - easing: `OutCubic`

## Interaction Animation
- Card drag uses direct pointer movement (`DragHandler` target `x`).
- On drag release:
  - if displacement > 32% width -> dismiss animation.
  - else card snaps back to `x=0` with easing.

## Next polish targets
- Switch center slide to spring curve.
- Add subtle overshoot for banner entry.
- Add blur/elevation transitions optimized for GPU path.
