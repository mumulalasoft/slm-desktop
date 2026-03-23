#pragma once

#include <QString>

namespace Slm::Motion {

enum class MotionPreset {
    Snappy,
    Smooth,
    Gentle,
    Responsive,
    Launcher,
    Heavy,
    Elastic,
};

enum class MotionState {
    Hidden,
    Peeking,
    PartiallyVisible,
    FullyVisible,
    Dismissed,
    SnappedLeft,
    SnappedRight,
    OverviewOpen,
    WorkspaceTransitioning,
};

enum class SettleDecision {
    Stay,
    Forward,
    Backward,
};

struct SpringParams {
    double stiffness = 520.0;
    double damping = 46.0;
    double mass = 1.0;
    double settleEpsilon = 0.001;
};

struct TimeParams {
    double durationMs = 180.0;
    double delayMs = 0.0;
};

struct PhysicsParams {
    double friction = 10.5;
    double stopVelocity = 5.0;
    double maxVelocity = 6000.0;
};

struct GestureSettleConfig {
    double forwardThreshold = 0.5;
    double backwardThreshold = 0.5;
    double velocityThreshold = 800.0;
};

struct MotionPresetSpec {
    MotionPreset preset = MotionPreset::Smooth;
    SpringParams spring;
    TimeParams time;
    PhysicsParams physics;
};

QString toString(MotionPreset preset);
MotionPreset motionPresetFromString(const QString &value);

} // namespace Slm::Motion
