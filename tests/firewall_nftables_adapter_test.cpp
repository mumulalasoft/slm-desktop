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
        QVERIFY(adapter.reconcileState(&error));
    }
};

QTEST_GUILESS_MAIN(FirewallNftablesAdapterTest)
#include "firewall_nftables_adapter_test.moc"
