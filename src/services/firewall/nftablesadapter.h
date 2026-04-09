#pragma once

#include <QString>
#include <QStringList>

namespace Slm::Firewall {

class NftablesAdapter
{
public:
    bool ensureBaseRules(QString *error = nullptr);
    bool applyAtomicBatch(const QStringList &rules, QString *error = nullptr);
    bool reconcileState(QString *error = nullptr);

    QStringList lastAppliedBatch() const;

private:
    QStringList m_lastBatch;
};

} // namespace Slm::Firewall
