#include <QtTest/QtTest>

#include "../src/services/firewall/appidentityclient.h"
#include "../src/services/firewall/nftablesadapter.h"
#include "../src/services/firewall/policyengine.h"
#include "../src/services/firewall/policystore.h"

class FirewallPolicyEngineContractTest : public QObject
{
    Q_OBJECT

private slots:
    void evaluate_connection_returns_decision()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap result = engine.evaluateConnection(
            QVariantMap{{QStringLiteral("pid"), 1234},
                        {QStringLiteral("direction"), QStringLiteral("incoming")}});
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(result.value(QStringLiteral("decision")).toString(), QStringLiteral("prompt"));
        QVERIFY(result.value(QStringLiteral("identity")).toMap().contains(QStringLiteral("app_id")));
    }

    void apply_base_policy_pushes_batch_to_nft()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap state{
            {QStringLiteral("enabled"), true},
            {QStringLiteral("mode"), QStringLiteral("public")},
            {QStringLiteral("defaultIncomingPolicy"), QStringLiteral("deny")},
            {QStringLiteral("defaultOutgoingPolicy"), QStringLiteral("deny")},
        };
        const QVariantMap result = engine.applyBasePolicy(state);
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);

        const QStringList rules = nft.lastAppliedBatch();
        QVERIFY(rules.contains(QStringLiteral("add chain inet slm_firewall input { type filter hook input priority 0; policy drop; }")));
        QVERIFY(rules.contains(QStringLiteral("add chain inet slm_firewall output { type filter hook output priority 0; policy drop; }")));
        QVERIFY(rules.contains(QStringLiteral("add rule inet slm_firewall input meta pkttype broadcast drop")));
    }
};

QTEST_GUILESS_MAIN(FirewallPolicyEngineContractTest)
#include "firewall_policyengine_contract_test.moc"
