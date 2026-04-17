#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Slm::PackagePolicy {

struct PackageTransaction {
    QStringList install;
    QStringList remove;
    QStringList upgrade;
    QStringList replace;
    QStringList downgrade;
    QString rawOutput;

    QJsonObject toJson() const;
};

} // namespace Slm::PackagePolicy
