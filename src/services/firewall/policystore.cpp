#include "policystore.h"

namespace Slm::Firewall {

bool PolicyStore::start(QString *error)
{
    if (error) {
        error->clear();
    }
    m_root = QVariantMap{
        {QStringLiteral("firewall"), QVariantMap{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("mode"), QStringLiteral("home")},
             {QStringLiteral("defaultIncomingPolicy"), QStringLiteral("deny")},
             {QStringLiteral("defaultOutgoingPolicy"), QStringLiteral("allow")},
         }},
    };
    return true;
}

QVariantMap PolicyStore::snapshot() const
{
    return m_root;
}

bool PolicyStore::setValue(const QString &path, const QVariant &value, QString *error)
{
    if (error) {
        error->clear();
    }
    const QString key = path.trimmed();
    if (key.isEmpty()) {
        if (error) {
            *error = QStringLiteral("invalid-path");
        }
        return false;
    }
    m_root.insert(key, value);
    return true;
}

QVariant PolicyStore::value(const QString &path, const QVariant &fallback) const
{
    const QString key = path.trimmed();
    if (key.isEmpty()) {
        return fallback;
    }
    return m_root.value(key, fallback);
}

} // namespace Slm::Firewall
