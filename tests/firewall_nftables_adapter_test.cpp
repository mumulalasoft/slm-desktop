#include <QtTest/QtTest>

#include "../src/services/firewall/nftablesadapter.h"

class FirewallNftablesAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void base_and_batch_contract()
    {
        Slm::Firewall::NftablesAdapter adapter;
        QString error;
        QVERIFY(adapter.ensureBaseRules(&error));
        QVERIFY(error.isEmpty());

        const QStringList batch{
            QStringLiteral("add table inet slm_firewall"),
            QStringLiteral("add chain inet slm_firewall input { type filter hook input priority 0; policy drop; }"),
        };
        QVERIFY(adapter.applyAtomicBatch(batch, &error));
        QCOMPARE(adapter.lastAppliedBatch(), batch);
        QVERIFY(adapter.reconcileState(batch, &error));
    }

    void base_policy_contract()
    {
        Slm::Firewall::NftablesAdapter adapter;
        QString error;
        const QVariantMap state{
            {QStringLiteral("enabled"), true},
            {QStringLiteral("mode"), QStringLiteral("home")},
            {QStringLiteral("defaultIncomingPolicy"), QStringLiteral("deny")},
            {QStringLiteral("defaultOutgoingPolicy"), QStringLiteral("allow")},
        };
        QVERIFY(adapter.applyBasePolicy(state, &error));
        QVERIFY(error.isEmpty());

        const QStringList rules = adapter.lastAppliedBatch();
        QVERIFY(rules.contains(QStringLiteral("add table inet slm_firewall")));
        QVERIFY(rules.contains(QStringLiteral("add chain inet slm_firewall input { type filter hook input priority 0; policy drop; }")));
        QVERIFY(rules.contains(QStringLiteral("add chain inet slm_firewall output { type filter hook output priority 0; policy accept; }")));
        QVERIFY(rules.contains(QStringLiteral("add rule inet slm_firewall input ip saddr 192.168.0.0/16 accept")));
    }
};

QTEST_GUILESS_MAIN(FirewallNftablesAdapterTest)
#include "firewall_nftables_adapter_test.moc"
