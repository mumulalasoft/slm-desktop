# SLM Animation Framework (Qt/QML + Wayland)

## 1. Ringkasan
Framework motion SLM berfokus pada tiga mode utama: spring, physics, dan gesture-driven. Engine inti ada di C++ untuk stabilitas frame/update, lalu diekspos ke QML lewat controller deklaratif agar shell tetap cepat diiterasi.

## 2. Arsitektur Komponen

```text
Input System
  -> Gesture Recognizer
     -> AnimationScheduler (frame loop / dt)
        -> AnimationEngine (spring/physics/time model)
           -> MotionValue / AnimatedProperty
              -> Scene Transform Layer (QML bindings)
                 -> Compositor / Renderer (KWin/Wayland)
```

- `Input System`: pointer/touch/wheel/shortcut.
- `Gesture Recognizer`: ekstrak delta, velocity, direction, cancel/end.
- `AnimationScheduler`: update per frame (vsync-aware interval, dt clamp).
- `AnimationEngine`: evaluasi channel motion, interrupt/retarget.
- `MotionValue`: nilai animasi observable untuk binding QML.
- `Scene Transform Layer`: terapkan translate/scale/opacity/progress.
- `Compositor`: komposisi final, blur, shadow, damage scheduling.

## 3. Modul C++ yang sudah dibuat
- `src/core/motion/slmanimationscheduler.*`
- `src/core/motion/slmanimationengine.*`
- `src/core/motion/slmmotionvalue.*`
- `src/core/motion/slmspringsolver.*`
- `src/core/motion/slmphysicsintegrator.*`
- `src/core/motion/slmgesturebinding.*`
- `src/core/motion/slmmotionpresetlibrary.*`
- `src/core/motion/slmmotioncontroller.*`
- `src/core/motion/slmmotiontypes.*`

## 4. Data Flow per Frame
1. Scheduler hit frame tick dan hitung `dt` (detik).
2. Engine update semua channel aktif (spring/physics).
3. Engine emit nilai terbaru `channelUpdated`.
4. Controller update `MotionValue`.
5. QML binding merender transform di frame berikutnya.

## 5. API C++ Inti
- `AnimationEngine::startSpringFromCurrent(channel, current, target, velocity, preset)`
- `AnimationEngine::retarget(channel, target)`
- `AnimationEngine::adoptVelocity(channel, velocity)`
- `AnimationEngine::cancelAndSettle(channel, target)`
- `AnimationScheduler::start()/stop()`
- `GestureBinding::begin/updateByDistance/end(...)`

## 6. API QML (via context `MotionController`)
Contoh konseptual:

```qml
Item {
    property real progress: MotionController.value

    function onSwipeUpdate(dx, width) {
        MotionController.gestureUpdate(dx, width)
    }

    function onSwipeEnd(vx) {
        const decision = MotionController.gestureEnd(vx, 0.5, 0.5, 800)
        if (decision === 1) MotionController.startFromCurrent(1.0)
        else if (decision === 2) MotionController.startFromCurrent(0.0)
        else MotionController.startFromCurrent(progress >= 0.5 ? 1.0 : 0.0)
    }
}
```

## 7. Solver Konseptual
- Spring: `F = -k(x-target) - c*v`, integrasi semi-implicit Euler.
- Physics inertia: velocity decay eksponensial berbasis friction.
- Stop condition:
  - spring: `|x-target| < epsilon` dan `|v|` kecil
  - physics: `|v| < stopVelocity`

## 8. Gesture-driven Transitions
- Progress real-time: `progress = distance / range`.
- Saat release: settle berdasarkan threshold + velocity.
- Settling menggunakan spring agar continuity velocity terjaga.

## 9. Interruptibility Strategy
- Tidak reset ke state awal.
- Retarget di posisi + velocity saat ini.
- API yang disediakan:
  - `startFromCurrent`
  - `retarget`
  - `adoptVelocity`
  - `cancelAndSettle`

## 10. Motion Presets
Preset awal:
- `snappy`
- `smooth`
- `gentle`
- `responsive`
- `heavy`
- `elastic`

Preset disediakan di `MotionPresetLibrary`, dipakai lintas shell agar konsisten.

## 11. Mapping Use Case
- Workspace swipe: gesture progress + spring settle.
- Notification slide: spring ringan + exit cepat.
- Window snap settle: spring retargetable.
- Panel reveal/hide: gesture + opacity/position binding.
- Launcher/tothespot reveal: scale + opacity + translate via channel.
- Overview: multi-channel transition terkoordinasi.

## 12. Debugging & Testing
Sudah ada test dasar:
- `tests/slmmotioncore_test.cpp`
  - spring convergence
  - gesture settle decision

Rencana lanjut:
- frame-time inspector
- active-channel inspector
- slow-motion mode
- dropped-frame counter

## 13. Tahapan Implementasi
1. Foundation module (selesai).
2. Integrasi launchpad/workspace memakai `MotionController`.
3. Tambah debug overlay dan metric collector.
4. Reduced-motion policy + preset tuning.
5. Integrasi tingkat compositor untuk animasi window-management.

## 14. Risiko & Kompromi
- QML-only animation lebih cepat di-build tapi lemah untuk interruptibility kompleks.
- Engine C++ menambah kompleksitas awal, namun stabil untuk shell-scale motion.
- Integrasi compositor memberi latency lebih baik untuk window transitions, tapi coupling lebih tinggi ke backend.
