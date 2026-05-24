#pragma once

#include "appentry.h"

namespace Slm::Appd {

// UIExposurePolicy — computes UIExposure bitmask from AppCategory.
//
// Policy matrix (from design spec):
//  shell               → none
//  gui-app             → appdeck, switcher, systemMonitor
//  gtk                 → appdeck, switcher, systemMonitor
//  kde                 → appdeck, switcher, systemMonitor
//  qt-generic          → appdeck, switcher, systemMonitor
//  qt-desktop-fallback → appdeck, switcher, systemMonitor
//  cli-app             → systemMonitor only
//  background-agent    → tray, systemMonitor
//  system-ignore       → none
//  unknown             → systemMonitor only (never appdeck/switcher by default)
class UIExposurePolicy
{
public:
    static UIExposure compute(AppCategory category);
};

} // namespace Slm::Appd
