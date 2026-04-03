#pragma once

#include <QJsonObject>
#include <QString>

namespace Slm::PackagePolicy {

class PackagePolicyLogger
{
public:
    static QString logPath();
    static void writeDecision(const QString &tool,
                              const QStringList &args,
                              const QJsonObject &decision,
                              const QJsonObject &transaction);
};

} // namespace Slm::PackagePolicy
