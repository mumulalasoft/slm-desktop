#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Slm::System {

struct ComponentRequirement {
    QString id;
    QString title;
    QString description;
    QString packageName;
    QStringList requiredExecutables;
    bool autoInstallable = true;
    QString guidance;
};

QVariantMap checkComponent(const ComponentRequirement &req);
QVariantMap installComponentWithPolkit(const ComponentRequirement &req, int timeoutMs = 20 * 60 * 1000);

} // namespace Slm::System
