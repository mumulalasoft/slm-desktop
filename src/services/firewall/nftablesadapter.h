#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Slm::Firewall {

class NftablesAdapter
{
public:
    bool ensureBaseRules(QString *error = nullptr);
    bool applyBasePolicy(const QVariantMap &state, QString *error = nullptr);
    bool applyAtomicBatch(const QStringList &rules, QString *error = nullptr);
    bool reconcileState(QString *error = nullptr);

    QStringList lastAppliedBatch() const;

private:
    static QString normalizePolicy(const QString &value, const QString &fallback);
    static QString normalizeMode(const QString &value);
    QStringList buildBasePolicyRules(const QVariantMap &state) const;

    QStringList m_lastBatch;
};

} // namespace Slm::Firewall
