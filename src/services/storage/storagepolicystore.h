#pragma once

#include <QString>
#include <QVariantMap>

namespace Slm::Storage {

class StoragePolicyStore
{
public:
    static QString policyFilePath();
    static QVariantMap loadPolicyMap();
    static bool savePolicyMap(const QVariantMap &policyMap, QString *errorOut = nullptr);
};

} // namespace Slm::Storage

