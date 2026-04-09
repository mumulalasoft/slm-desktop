#include "nftablesadapter.h"

namespace Slm::Firewall {

bool NftablesAdapter::ensureBaseRules(QString *error)
{
    if (error) {
        error->clear();
    }
    return true;
}

bool NftablesAdapter::applyAtomicBatch(const QStringList &rules, QString *error)
{
    if (error) {
        error->clear();
    }
    m_lastBatch = rules;
    return true;
}

bool NftablesAdapter::reconcileState(QString *error)
{
    if (error) {
        error->clear();
    }
    return true;
}

QStringList NftablesAdapter::lastAppliedBatch() const
{
    return m_lastBatch;
}

} // namespace Slm::Firewall
