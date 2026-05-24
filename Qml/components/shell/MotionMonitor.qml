import QtQuick
import Slm_Desktop

// MotionMonitor — real-time FPS sampler for the motion quality guard.
//
// Mount exactly once in the shell root (Main.qml). It is invisible and
// zero-cost to render. FrameAnimation fires on every scene-graph paint,
// so currentFps reflects the actual rendered frame rate — not a timer estimate.
//
// Every sampleInterval ms it calls MotionController.reportFps(fps), which
// updates qualityTier and fires the lowFps signal when FPS < 50.
//
// Placement in Main.qml:
//   ShellComp.MotionMonitor { }

Item {
    id: monitor
    width: 0; height: 0
    visible: false

    // How often (milliseconds) to compute and push the FPS sample.
    property int sampleInterval: 2000

    property int   _frameCount:    0
    property real  _lastSampleMs:  0.0

    FrameAnimation {
        id: fa
        running: MotionController.animationsEnabled

        onTriggered: {
            monitor._frameCount++

            var now = Date.now()
            if (monitor._lastSampleMs === 0.0) {
                monitor._lastSampleMs = now
                return
            }

            var elapsed = now - monitor._lastSampleMs
            if (elapsed >= monitor.sampleInterval) {
                var fps = (monitor._frameCount / elapsed) * 1000.0
                MotionController.reportFps(fps)
                monitor._frameCount   = 0
                monitor._lastSampleMs = now
            }
        }
    }
}
