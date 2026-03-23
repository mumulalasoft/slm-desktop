#pragma once

#include <QStringList>

struct AppStartupArgs {
    bool startWindowed = false;
    int windowWidth = 0;
    int windowHeight = 0;
};

AppStartupArgs parseAppStartupArgs(const QStringList &arguments);

