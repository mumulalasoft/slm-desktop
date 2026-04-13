#pragma once

#include "appentry.h"

namespace Slm::Appd {

// UIExposurePolicy — computes UIExposure bitmask from AppCategory.
//
// Policy matrix (from design spec):
//  shell               → none
//  gui-app             → dock, switcher, systemMonitor
//  gtk                 → dock, switcher, systemMonitor
//  kde                 → dock, switcher, systemMonitor
//  qt-generic          → dock, switcher, systemMonitor
//  qt-desktop-fallback → dock, switcher, systemMonitor
//  cli-app             → systemMonitor only
//  background-agent    → tray, systemMonitor
//  system-ignore       → none
//  unknown             → systemMonitor only (never dock/switcher by default)
class UIExposurePolicy
{
public:
    static UIExposure compute(AppCategory category);
};

} // namespace Slm::Appd
