#include "uiexposurepolicy.h"

namespace Slm::Appd {

UIExposure UIExposurePolicy::compute(AppCategory category)
{
    UIExposure e;
    switch (category) {
    case AppCategory::Shell:
    case AppCategory::SystemIgnore:
        // No UI exposure.
        break;

    case AppCategory::GuiApp:
    case AppCategory::Gtk:
    case AppCategory::Kde:
    case AppCategory::QtGeneric:
    case AppCategory::QtDesktopFallback:
        e.dock          = true;
        e.appSwitcher   = true;
        e.systemMonitor = true;
        break;

    case AppCategory::CliApp:
        e.systemMonitor = true;
        break;

    case AppCategory::BackgroundAgent:
        e.tray          = true;
        e.systemMonitor = true;
        break;

    case AppCategory::Unknown:
        e.systemMonitor = true;
        // Dock and switcher intentionally off for unknown category.
        break;
    }
    return e;
}

} // namespace Slm::Appd
