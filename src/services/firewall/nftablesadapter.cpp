#include "nftablesadapter.h"

#include <QByteArray>
#include <QDebug>
#include <QProcessEnvironment>
#include <QProcess>

namespace Slm::Firewall {

namespace {
// Run nft with rules piped via stdin (-f -).
// Each rule must be a complete nftables statement on its own line.
// Returns true on exit code 0; populates error with stderr otherwise.
bool runNft(const QStringList &rules, QString *error)
{
    if (rules.isEmpty()) {
        return true;
    }

    const QByteArray script = (rules.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();

    QProcess proc;
    proc.setProgram(QStringLiteral("nft"));
    proc.setArguments({QStringLiteral("-f"), QStringLiteral("-")});
    proc.start();

    if (!proc.waitForStarted(3000)) {
        const QString msg = QStringLiteral("nft-start-failed: %1").arg(proc.errorString());
        qWarning() << "[firewall/nft]" << msg;
        if (error) {
            *error = msg;
        }
        return false;
    }

    proc.write(script);
    proc.closeWriteChannel();

    if (!proc.waitForFinished(10000)) {
        proc.kill();
        const QString msg = QStringLiteral("nft-timeout");
        qWarning() << "[firewall/nft]" << msg;
        if (error) {
            *error = msg;
        }
        return false;
    }

    if (proc.exitCode() != 0) {
        const QString stderr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const QString msg = stderr.isEmpty()
                ? QStringLiteral("nft-exit=%1").arg(proc.exitCode())
                : stderr;
        qWarning() << "[firewall/nft] error:" << msg;
        if (error) {
            *error = msg;
        }
        return false;
    }

    return true;
}
} // namespace

bool NftablesAdapter::ensureBaseRules(QString *error)
{
    // Ensure the slm_firewall table exists.  A no-op if already present.
    const QStringList probe{
        QStringLiteral("add table inet slm_firewall"),
    };
    return runNft(probe, error);
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
    if (!runNft(rules, error)) {
        const bool allowDryRun = QProcessEnvironment::systemEnvironment().value(
                                     QStringLiteral("SLM_FIREWALL_ALLOW_DRYRUN"))
                                         == QStringLiteral("1");
        if (allowDryRun) {
            qWarning() << "[firewall/nft] apply failed; dry-run mode active, keeping in-memory batch only";
            m_lastBatch = rules;
            if (error) {
                error->clear();
            }
            return true;
        }
        return false;
    }
    m_lastBatch = rules;
    return true;
}

bool NftablesAdapter::reconcileState(const QStringList &fullRuleSet, QString *error)
{
    // Full reconciliation: the caller provides the complete desired rule set
    // (base flush + base rules + all active IP block rules). We re-apply it
    // atomically. This is the correct way to remove rules from nftables without
    // per-rule handle tracking — flush the table and rebuild from scratch.
    return applyAtomicBatch(fullRuleSet, error);
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
