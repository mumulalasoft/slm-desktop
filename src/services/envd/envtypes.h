#pragma once

#include <QHash>
#include <QString>

// Priority layers — lower value = lower priority.
// MergeResolver processes them in ascending order so higher layers win.
enum class EnvLayer : int {
    Default        = 0,  // /etc/environment, compiled-in defaults
    System         = 1,  // /etc/environment.d/*.conf
    UserPersistent = 2,  // ~/.config/slm/environment.d/user.json  (local store)
    Session        = 3,  // in-memory session overrides
    PerApp         = 4,  // ~/.config/slm/environment.d/apps/<appid>.json
    LaunchTime     = 5,  // supplied per-exec from launcher
};

// A resolved variable with its winning value and the layer that produced it.
struct ResolvedVar {
    QString value;
    EnvLayer sourceLayer = EnvLayer::Default;
};

// The final environment map produced by MergeResolver::resolve().
using ResolvedEnv = QHash<QString, ResolvedVar>;
