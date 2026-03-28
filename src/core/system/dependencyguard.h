#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>

namespace Slm::System {

struct ComponentRequirement {
    QString id;
    QString title;
    QString description;
    QString packageName;
    QStringList requiredExecutables;
    QStringList requiredPaths;
    bool autoInstallable = true;
    QString guidance;
    QMap<QString, QString> packageNamesByDistro;
};

QVariantMap checkComponent(const ComponentRequirement &req);
QVariantMap installComponentWithPolkit(const ComponentRequirement &req, int timeoutMs = 20 * 60 * 1000);

} // namespace Slm::System
