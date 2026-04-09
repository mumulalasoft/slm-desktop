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

    void set_ip_policy_persists_and_applies_rules()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap request{
            {QStringLiteral("type"), QStringLiteral("list")},
            {QStringLiteral("addresses"), QVariantList{
                 QStringLiteral("203.0.113.10"),
                 QStringLiteral("198.51.100.44"),
             }},
            {QStringLiteral("scope"), QStringLiteral("incoming")},
            {QStringLiteral("reason"), QStringLiteral("suspicious-ingress")},
            {QStringLiteral("temporary"), true},
            {QStringLiteral("duration"), QStringLiteral("24h")},
        };

        const QVariantMap result = engine.setIpPolicy(request);
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(result.value(QStringLiteral("appliedRules")).toInt(), 2);
        QCOMPARE(result.value(QStringLiteral("policy")).toMap().value(QStringLiteral("scope")).toString(),
                 QStringLiteral("incoming"));

        const QStringList rules = nft.lastAppliedBatch();
        QVERIFY(rules.contains(QStringLiteral("add rule inet slm_firewall input ip saddr 203.0.113.10 drop")));
        QVERIFY(rules.contains(QStringLiteral("add rule inet slm_firewall input ip saddr 198.51.100.44 drop")));

        const QVariantList entries = store.value(QStringLiteral("firewall.rules.ipBlocks.entries")).toList();
        QCOMPARE(entries.size(), 1);
        const QVariantMap stored = entries.first().toMap();
        QCOMPARE(stored.value(QStringLiteral("type")).toString(), QStringLiteral("list"));
        QCOMPARE(stored.value(QStringLiteral("reason")).toString(), QStringLiteral("suspicious-ingress"));
        QCOMPARE(stored.value(QStringLiteral("duration")).toString(), QStringLiteral("24h"));
    }

    void list_and_clear_ip_policies_contract()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        QCOMPARE(engine.listIpPolicies().size(), 0);

        const QVariantMap addResult = engine.setIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("203.0.113.7")},
            {QStringLiteral("scope"), QStringLiteral("both")},
        });
        QCOMPARE(addResult.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(engine.listIpPolicies().size(), 1);

        const QVariantMap clearResult = engine.clearIpPolicies();
        QCOMPARE(clearResult.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(engine.listIpPolicies().size(), 0);
    }
};

QTEST_GUILESS_MAIN(FirewallPolicyEngineContractTest)
#include "firewall_policyengine_contract_test.moc"
