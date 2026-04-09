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
        QCOMPARE(result.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
        QCOMPARE(result.value(QStringLiteral("source")).toString(), QStringLiteral("default-incoming"));
        QVERIFY(result.value(QStringLiteral("identity")).toMap().contains(QStringLiteral("app_id")));
    }

    void evaluate_connection_prefers_matching_app_policy()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap addPolicy = engine.setAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("unknown")},
            {QStringLiteral("decision"), QStringLiteral("allow")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        QCOMPARE(addPolicy.value(QStringLiteral("ok")).toBool(), true);

        const QVariantMap result = engine.evaluateConnection(
            QVariantMap{{QStringLiteral("pid"), -1},
                        {QStringLiteral("direction"), QStringLiteral("incoming")}});
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(result.value(QStringLiteral("decision")).toString(), QStringLiteral("allow"));
        QCOMPARE(result.value(QStringLiteral("source")).toString(), QStringLiteral("app-policy"));
    }

    void evaluate_connection_outgoing_unknown_prompts()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap result = engine.evaluateConnection(
            QVariantMap{{QStringLiteral("pid"), -1},
                        {QStringLiteral("direction"), QStringLiteral("outgoing")}});
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(result.value(QStringLiteral("decision")).toString(), QStringLiteral("prompt"));
        QCOMPARE(result.value(QStringLiteral("source")).toString(), QStringLiteral("cli-default-prompt"));
        QCOMPARE(result.value(QStringLiteral("processKind")).toString(), QStringLiteral("unknown"));
        QCOMPARE(result.value(QStringLiteral("promptSuppressed")).toBool(), false);

        const QVariantMap second = engine.evaluateConnection(
            QVariantMap{{QStringLiteral("pid"), -1},
                        {QStringLiteral("direction"), QStringLiteral("outgoing")}});
        QCOMPARE(second.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(second.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
        QCOMPARE(second.value(QStringLiteral("source")).toString(), QStringLiteral("prompt-cooldown"));
        QCOMPARE(second.value(QStringLiteral("promptSuppressed")).toBool(), true);
    }

    void resolve_connection_decision_persists_when_remember_true()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap once = engine.resolveConnectionDecision(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        }, QStringLiteral("deny"), false);
        QCOMPARE(once.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(once.value(QStringLiteral("persisted")).toBool(), false);
        QCOMPARE(engine.listAppPolicies().size(), 0);

        const QVariantMap remember = engine.resolveConnectionDecision(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        }, QStringLiteral("allow"), true);
        QCOMPARE(remember.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(remember.value(QStringLiteral("persisted")).toBool(), true);
        QCOMPARE(engine.listAppPolicies().size(), 1);
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
        const QVariantMap normalizedPolicy = result.value(QStringLiteral("policy")).toMap();
        QCOMPARE(normalizedPolicy.value(QStringLiteral("scope")).toString(), QStringLiteral("incoming"));
        QVERIFY(!normalizedPolicy.value(QStringLiteral("policyId")).toString().isEmpty());
        QVERIFY(!normalizedPolicy.value(QStringLiteral("createdAt")).toString().isEmpty());

        const QStringList rules = nft.lastAppliedBatch();
        QVERIFY(rules.contains(QStringLiteral("add rule inet slm_firewall input ip saddr 203.0.113.10 drop")));
        QVERIFY(rules.contains(QStringLiteral("add rule inet slm_firewall input ip saddr 198.51.100.44 drop")));

        const QVariantList entries = store.value(QStringLiteral("firewall.rules.ipBlocks.entries")).toList();
        QCOMPARE(entries.size(), 1);
        const QVariantMap stored = entries.first().toMap();
        QCOMPARE(stored.value(QStringLiteral("type")).toString(), QStringLiteral("list"));
        QCOMPARE(stored.value(QStringLiteral("reason")).toString(), QStringLiteral("suspicious-ingress"));
        QCOMPARE(stored.value(QStringLiteral("duration")).toString(), QStringLiteral("24h"));
        QVERIFY(!stored.value(QStringLiteral("policyId")).toString().isEmpty());
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

    void remove_ip_policy_contract()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap first = engine.setIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("203.0.113.21")},
            {QStringLiteral("scope"), QStringLiteral("both")},
        });
        const QVariantMap second = engine.setIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("198.51.100.52")},
            {QStringLiteral("scope"), QStringLiteral("incoming")},
        });
        QCOMPARE(first.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(second.value(QStringLiteral("ok")).toBool(), true);

        const QString firstId = first.value(QStringLiteral("policy")).toMap().value(QStringLiteral("policyId")).toString();
        const QString secondId = second.value(QStringLiteral("policy")).toMap().value(QStringLiteral("policyId")).toString();
        QVERIFY(!firstId.isEmpty());
        QVERIFY(!secondId.isEmpty());
        QCOMPARE(engine.listIpPolicies().size(), 2);

        const QVariantMap removeResult = engine.removeIpPolicy(firstId);
        QCOMPARE(removeResult.value(QStringLiteral("ok")).toBool(), true);

        const QVariantList remaining = engine.listIpPolicies();
        QCOMPARE(remaining.size(), 1);
        QCOMPARE(remaining.first().toMap().value(QStringLiteral("policyId")).toString(), secondId);

        const QVariantMap missing = engine.removeIpPolicy(QStringLiteral("missing-policy-id"));
        QCOMPARE(missing.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(missing.value(QStringLiteral("error")).toString(), QStringLiteral("policy-id-not-found"));
    }

    void list_ip_policies_prunes_expired_temporary_entries()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap temporary = engine.setIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("203.0.113.42")},
            {QStringLiteral("scope"), QStringLiteral("both")},
            {QStringLiteral("temporary"), true},
            {QStringLiteral("duration"), QStringLiteral("1h")},
        });
        QCOMPARE(temporary.value(QStringLiteral("ok")).toBool(), true);

        QVariantList entries = store.value(QStringLiteral("firewall.rules.ipBlocks.entries")).toList();
        QCOMPARE(entries.size(), 1);

        QVariantMap expired = entries.first().toMap();
        expired.insert(QStringLiteral("createdAt"),
                       QDateTime::currentDateTimeUtc().addSecs(-7200).toString(Qt::ISODate));
        store.setValue(QStringLiteral("firewall.rules.ipBlocks.entries"), QVariantList{expired}, &error);

        const QVariantList listed = engine.listIpPolicies();
        QCOMPARE(listed.size(), 0);
        QCOMPARE(store.value(QStringLiteral("firewall.rules.ipBlocks.entries")).toList().size(), 0);
    }

    void app_policy_crud_contract()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap invalid = engine.setAppPolicy(QVariantMap{{QStringLiteral("decision"), QStringLiteral("allow")}});
        QCOMPARE(invalid.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(invalid.value(QStringLiteral("error")).toString(), QStringLiteral("app-policy-missing-app-id"));

        const QVariantMap first = engine.setAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.AppOne")},
            {QStringLiteral("appName"), QStringLiteral("App One")},
            {QStringLiteral("decision"), QStringLiteral("deny")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        const QVariantMap second = engine.setAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.AppTwo")},
            {QStringLiteral("decision"), QStringLiteral("allow")},
            {QStringLiteral("direction"), QStringLiteral("both")},
        });
        QCOMPARE(first.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(second.value(QStringLiteral("ok")).toBool(), true);

        const QVariantList listed = engine.listAppPolicies();
        QCOMPARE(listed.size(), 2);
        const QString firstId = first.value(QStringLiteral("policy")).toMap().value(QStringLiteral("policyId")).toString();
        QVERIFY(!firstId.isEmpty());

        const QVariantMap removeResult = engine.removeAppPolicy(firstId);
        QCOMPARE(removeResult.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(engine.listAppPolicies().size(), 1);

        const QVariantMap missing = engine.removeAppPolicy(QStringLiteral("missing-id"));
        QCOMPARE(missing.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(missing.value(QStringLiteral("error")).toString(), QStringLiteral("policy-id-not-found"));

        const QVariantMap clearResult = engine.clearAppPolicies();
        QCOMPARE(clearResult.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(engine.listAppPolicies().size(), 0);
    }

    void app_policy_set_deduplicates_by_app_and_direction()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantMap first = engine.setAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.Dedup")},
            {QStringLiteral("decision"), QStringLiteral("allow")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        QCOMPARE(first.value(QStringLiteral("ok")).toBool(), true);
        const QVariantMap firstPolicy = first.value(QStringLiteral("policy")).toMap();
        const QString policyId = firstPolicy.value(QStringLiteral("policyId")).toString();
        QVERIFY(!policyId.isEmpty());
        QCOMPARE(engine.listAppPolicies().size(), 1);

        const QVariantMap second = engine.setAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.Dedup")},
            {QStringLiteral("decision"), QStringLiteral("deny")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        QCOMPARE(second.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(engine.listAppPolicies().size(), 1);

        const QVariantMap stored = engine.listAppPolicies().first().toMap();
        QCOMPARE(stored.value(QStringLiteral("policyId")).toString(), policyId);
        QCOMPARE(stored.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
    }

    void list_connections_shape_contract()
    {
        Slm::Firewall::PolicyStore store;
        QString error;
        QVERIFY(store.start(&error));

        Slm::Firewall::NftablesAdapter nft;
        Slm::Firewall::AppIdentityClient identity;
        Slm::Firewall::PolicyEngine engine(&store, &nft, &identity);

        const QVariantList connections = engine.listConnections();
        for (const QVariant &item : connections) {
            const QVariantMap row = item.toMap();
            QVERIFY(row.contains(QStringLiteral("protocol")));
            QVERIFY(row.contains(QStringLiteral("state")));
            QVERIFY(row.contains(QStringLiteral("local")));
            QVERIFY(row.contains(QStringLiteral("remote")));
            QVERIFY(row.contains(QStringLiteral("pid")));
            QVERIFY(row.contains(QStringLiteral("identity")));
        }
    }
};

QTEST_GUILESS_MAIN(FirewallPolicyEngineContractTest)
#include "firewall_policyengine_contract_test.moc"
