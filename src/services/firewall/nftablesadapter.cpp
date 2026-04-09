#include "nftablesadapter.h"

namespace Slm::Firewall {

bool NftablesAdapter::ensureBaseRules(QString *error)
{
    if (error) {
        error->clear();
    }
    return true;
}

bool NftablesAdapter::applyBasePolicy(const QVariantMap &state, QString *error)
{
    const QStringList rules = buildBasePolicyRules(state);
    return applyAtomicBatch(rules, error);
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

QString NftablesAdapter::normalizePolicy(const QString &value, const QString &fallback)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("allow")
            || normalized == QLatin1String("deny")
            || normalized == QLatin1String("prompt")) {
        return normalized;
    }
    return fallback;
}

QString NftablesAdapter::normalizeMode(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == QLatin1String("public") || mode == QLatin1String("custom")) {
        return mode;
    }
    return QStringLiteral("home");
}

QStringList NftablesAdapter::buildBasePolicyRules(const QVariantMap &state) const
{
    const bool enabled = state.value(QStringLiteral("enabled"), true).toBool();
    const QString mode = normalizeMode(state.value(QStringLiteral("mode"), QStringLiteral("home")).toString());
    const QString incoming = normalizePolicy(
        state.value(QStringLiteral("defaultIncomingPolicy"), QStringLiteral("deny")).toString(),
        QStringLiteral("deny"));
    const QString outgoing = normalizePolicy(
        state.value(QStringLiteral("defaultOutgoingPolicy"), QStringLiteral("allow")).toString(),
        QStringLiteral("allow"));

    const QString inputPolicy = enabled
            ? (incoming == QLatin1String("allow") ? QStringLiteral("accept") : QStringLiteral("drop"))
            : QStringLiteral("accept");
    const QString outputPolicy = enabled
            ? (outgoing == QLatin1String("deny") ? QStringLiteral("drop") : QStringLiteral("accept"))
            : QStringLiteral("accept");

    QStringList rules{
        QStringLiteral("flush table inet slm_firewall"),
        QStringLiteral("add table inet slm_firewall"),
        QStringLiteral("add chain inet slm_firewall input { type filter hook input priority 0; policy %1; }")
            .arg(inputPolicy),
        QStringLiteral("add chain inet slm_firewall output { type filter hook output priority 0; policy %1; }")
            .arg(outputPolicy),
        QStringLiteral("add chain inet slm_firewall forward { type filter hook forward priority 0; policy drop; }"),
        QStringLiteral("add rule inet slm_firewall input iif lo accept"),
        QStringLiteral("add rule inet slm_firewall input ct state established,related accept"),
    };

    if (enabled && mode == QLatin1String("home")) {
        rules << QStringLiteral("add rule inet slm_firewall input ip saddr 10.0.0.0/8 accept")
              << QStringLiteral("add rule inet slm_firewall input ip saddr 172.16.0.0/12 accept")
              << QStringLiteral("add rule inet slm_firewall input ip saddr 192.168.0.0/16 accept");
    }

    if (enabled && mode == QLatin1String("public")) {
        rules << QStringLiteral("add rule inet slm_firewall input meta pkttype broadcast drop");
    }

    return rules;
}

} // namespace Slm::Firewall
