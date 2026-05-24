#pragma once

#include <QLoggingCategory>

// Centralised logging categories for shell runtime subsystems.
// Definitions live next to the primary owning translation unit:
//   slmKwin       → src/core/workspace/kwinwaylandipcclient.cpp
//   slmLayershell → src/core/wayland/layershell/wlrlayershell.cpp
//
// Toggle at runtime with QT_LOGGING_RULES, e.g.:
//   QT_LOGGING_RULES="slm.kwin=true;slm.layershell.debug=true"

Q_DECLARE_LOGGING_CATEGORY(slmKwin)
Q_DECLARE_LOGGING_CATEGORY(slmLayershell)
