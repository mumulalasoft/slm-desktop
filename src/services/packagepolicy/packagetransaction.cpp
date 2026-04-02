#include "packagetransaction.h"

#include <QJsonArray>

namespace Slm::PackagePolicy {

QJsonObject PackageTransaction::toJson() const
{
    auto toArray = [](const QStringList &rows) {
        QJsonArray out;
        for (const QString &row : rows) {
            out.push_back(row);
        }
        return out;
    };

    QJsonObject obj;
    obj.insert(QStringLiteral("install"), toArray(install));
    obj.insert(QStringLiteral("remove"), toArray(remove));
    obj.insert(QStringLiteral("upgrade"), toArray(upgrade));
    obj.insert(QStringLiteral("replace"), toArray(replace));
    obj.insert(QStringLiteral("downgrade"), toArray(downgrade));
    return obj;
}

} // namespace Slm::PackagePolicy
