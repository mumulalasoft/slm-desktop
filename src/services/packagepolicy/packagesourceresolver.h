#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace Slm::PackagePolicy {

struct PackageSourceInfo {
    QString sourceId;      // official | vendor | community | local
    QString sourceKind;    // from policy kind
    QString sourceRisk;    // from policy risk
    QString sourceDetail;  // URL/domain or local descriptor
};

class PackageSourceResolver
{
public:
    static QHash<QString, PackageSourceInfo> detectForPackages(const QStringList &packages,
                                                               const QString &tool);

private:
    static QString normalizePackageName(const QString &name);
    static PackageSourceInfo detectAptSource(const QString &packageName);
    static PackageSourceInfo classifyByRepositoryHint(const QString &hint);
};

} // namespace Slm::PackagePolicy
