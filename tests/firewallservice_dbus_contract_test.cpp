#include <QtTest/QtTest>

#include "../src/daemon/firewalld/firewallservice.h"
#include "../src/services/settingsd/settingsservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

class FirewallServiceDbusContractTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("SLM_FIREWALL_ALLOW_DRYRUN", QByteArrayLiteral("1"));
    }

    void init()
    {
        const QString testName = QString::fromLatin1(QTest::currentTestFunction());
        m_storePath = QDir::tempPath()
                + QStringLiteral("/slm-firewalld-ssot-test-")
                + QString::number(QCoreApplication::applicationPid())
                + QStringLiteral("-")
                + testName
                + QStringLiteral(".json");
        QFile::remove(m_storePath);
        QString lastGood = m_storePath;
        lastGood.chop(5);
        lastGood += QStringLiteral(".last-good.json");
        QFile::remove(lastGood);
        qputenv("SLM_SETTINGSD_STORE_PATH", m_storePath.toUtf8());
    }

    void cleanup()
    {
        QFile::remove(m_storePath);
        QString lastGood = m_storePath;
        if (lastGood.endsWith(QStringLiteral(".json"))) {
            lastGood.chop(5);
            lastGood += QStringLiteral(".last-good.json");
            QFile::remove(lastGood);
        }
    }

    void ping_shape_contract()
    {
        FirewallService service;
        const QVariantMap ping = service.Ping();
        QCOMPARE(ping.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(ping.value(QStringLiteral("service")).toString(),
                 QStringLiteral("org.slm.Desktop.Firewall"));
        QCOMPARE(ping.value(QStringLiteral("apiVersion")).toString(), QStringLiteral("1.0"));
    }

    void status_mode_roundtrip_contract()
    {
        SettingsService settings;
        QString settingsError;
        const bool settingsAvailable = settings.start(&settingsError);
        if (!settingsAvailable) {
            QVERIFY2(settingsError.isEmpty(), qPrintable(settingsError));
            QVERIFY(settings.Ping().value(QStringLiteral("ok")).toBool());
        }

        FirewallService service;
        service.SetMode(QStringLiteral("public"));
        QVariantMap status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("mode")).toString(), QStringLiteral("public"));

        if (settingsAvailable) {
            const QVariantMap settingsPayload = settings.GetSettings();
            const QVariantMap settingsRoot = settingsPayload.value(QStringLiteral("settings")).toMap();
            const QVariantMap firewall = settingsRoot.value(QStringLiteral("firewall")).toMap();
            QCOMPARE(firewall.value(QStringLiteral("mode")).toString(), QStringLiteral("public"));
        }

        service.SetMode(QStringLiteral("custom"));
        status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("mode")).toString(), QStringLiteral("custom"));

        const QVariantMap setEnabledResult = service.SetEnabled(false);
        QCOMPARE(setEnabledResult.value(QStringLiteral("enabled")).toBool(), false);
        QCOMPARE(setEnabledResult.value(QStringLiteral("packetApplied")).toBool(), true);

        if (settingsAvailable) {
            const QVariantMap settingsPayloadAfter = settings.GetSettings();
            const QVariantMap settingsAfterRoot = settingsPayloadAfter.value(QStringLiteral("settings")).toMap();
            const QVariantMap firewallAfter = settingsAfterRoot.value(QStringLiteral("firewall")).toMap();
            QCOMPARE(firewallAfter.value(QStringLiteral("mode")).toString(), QStringLiteral("custom"));
            QCOMPARE(firewallAfter.value(QStringLiteral("enabled")).toBool(), false);
        }
    }

    void default_policy_roundtrip_contract()
    {
        SettingsService settings;
        QString settingsError;
        const bool settingsAvailable = settings.start(&settingsError);
        if (!settingsAvailable) {
            QVERIFY2(settingsError.isEmpty(), qPrintable(settingsError));
            QVERIFY(settings.Ping().value(QStringLiteral("ok")).toBool());
        }

        FirewallService service;
        QVariantMap status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("deny"));
        QCOMPARE(status.value(QStringLiteral("defaultOutgoingPolicy")).toString(), QStringLiteral("allow"));
        QCOMPARE(status.value(QStringLiteral("promptCooldownSeconds")).toInt(), 20);

        service.SetDefaultIncomingPolicy(QStringLiteral("prompt"));
        const QVariantMap outgoingResult = service.SetDefaultOutgoingPolicy(QStringLiteral("deny"));
        QCOMPARE(outgoingResult.value(QStringLiteral("packetApplied")).toBool(), true);
        const QVariantMap cooldownResult = service.SetPromptCooldownSeconds(42);
        QCOMPARE(cooldownResult.value(QStringLiteral("ok")).toBool(), settingsAvailable);

        status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("prompt"));
        QCOMPARE(status.value(QStringLiteral("defaultOutgoingPolicy")).toString(), QStringLiteral("deny"));
        QCOMPARE(status.value(QStringLiteral("promptCooldownSeconds")).toInt(), 42);

        // Invalid value should be normalized to previous valid value.
        service.SetDefaultIncomingPolicy(QStringLiteral("invalid-policy"));
        status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("prompt"));

        if (settingsAvailable) {
            const QVariantMap settingsPayload = settings.GetSettings();
            const QVariantMap settingsRoot = settingsPayload.value(QStringLiteral("settings")).toMap();
            const QVariantMap firewall = settingsRoot.value(QStringLiteral("firewall")).toMap();
            QCOMPARE(firewall.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("prompt"));
            QCOMPARE(firewall.value(QStringLiteral("defaultOutgoingPolicy")).toString(), QStringLiteral("deny"));
            QCOMPARE(firewall.value(QStringLiteral("promptCooldownSeconds")).toInt(), 42);
        }
    }

    void evaluate_connection_policy_contract()
    {
        FirewallService service;
        QSignalSpy promptSpy(&service, &FirewallService::ConnectionPromptRequested);
        const QVariantMap incomingDefault = service.EvaluateConnection(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        QCOMPARE(incomingDefault.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(incomingDefault.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
        QCOMPARE(incomingDefault.value(QStringLiteral("source")).toString(), QStringLiteral("default-incoming"));

        const QVariantMap addPolicy = service.SetAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("unknown")},
            {QStringLiteral("decision"), QStringLiteral("allow")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        QCOMPARE(addPolicy.value(QStringLiteral("ok")).toBool(), true);

        const QVariantMap incomingApp = service.EvaluateConnection(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        QCOMPARE(incomingApp.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(incomingApp.value(QStringLiteral("decision")).toString(), QStringLiteral("allow"));
        QCOMPARE(incomingApp.value(QStringLiteral("source")).toString(), QStringLiteral("app-policy"));

        const QVariantMap outgoingUnknown = service.EvaluateConnection(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("outgoing")},
        });
        QCOMPARE(outgoingUnknown.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(outgoingUnknown.value(QStringLiteral("decision")).toString(), QStringLiteral("prompt"));
        QCOMPARE(outgoingUnknown.value(QStringLiteral("source")).toString(), QStringLiteral("cli-default-prompt"));
        QCOMPARE(outgoingUnknown.value(QStringLiteral("promptSuppressed")).toBool(), false);
        QCOMPARE(promptSpy.count(), 1);
        const QVariantMap promptPayload = promptSpy.takeFirst().at(0).toMap();
        QVERIFY(!promptPayload.value(QStringLiteral("request")).toMap().isEmpty());
        QVERIFY(!promptPayload.value(QStringLiteral("evaluation")).toMap().isEmpty());

        const QVariantMap outgoingSuppressed = service.EvaluateConnection(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("outgoing")},
        });
        QCOMPARE(outgoingSuppressed.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(outgoingSuppressed.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
        QCOMPARE(outgoingSuppressed.value(QStringLiteral("source")).toString(), QStringLiteral("prompt-cooldown"));
        QCOMPARE(outgoingSuppressed.value(QStringLiteral("promptSuppressed")).toBool(), true);
        QCOMPARE(promptSpy.count(), 0);

        const QVariantMap outgoingLocal = service.EvaluateConnection(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("outgoing")},
            {QStringLiteral("destinationIp"), QStringLiteral("10.10.0.2")},
            {QStringLiteral("destinationPort"), 8080},
        });
        QCOMPARE(outgoingLocal.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(outgoingLocal.value(QStringLiteral("decision")).toString(), QStringLiteral("allow"));
        QCOMPARE(outgoingLocal.value(QStringLiteral("source")).toString(), QStringLiteral("cli-local-target-allow"));
        const QVariantMap localTarget = outgoingLocal.value(QStringLiteral("target")).toMap();
        QCOMPARE(localTarget.value(QStringLiteral("ip")).toString(), QStringLiteral("10.10.0.2"));
        QCOMPARE(localTarget.value(QStringLiteral("port")).toInt(), 8080);
        QVERIFY(outgoingLocal.value(QStringLiteral("actor")).toMap().contains(QStringLiteral("appName")));

        const QVariantMap remember = service.ResolveConnectionDecision(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        }, QStringLiteral("allow"), true);
        QCOMPARE(remember.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(remember.value(QStringLiteral("persisted")).toBool(), true);
        QCOMPARE(service.ListAppPolicies().size(), 1);

        const QVariantMap rememberLocalOnly = service.ResolveConnectionDecision(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("outgoing")},
            {QStringLiteral("destinationIp"), QStringLiteral("203.0.113.34")},
            {QStringLiteral("localOnly"), true},
        }, QStringLiteral("allow"), true);
        QCOMPARE(rememberLocalOnly.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(rememberLocalOnly.value(QStringLiteral("persisted")).toBool(), true);
        QCOMPARE(rememberLocalOnly.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));

        const QVariantList localOnlyPolicies = service.ListAppPolicies();
        QCOMPARE(localOnlyPolicies.size(), 2);
        const QVariantMap latestLocalOnly = localOnlyPolicies.last().toMap();
        QCOMPARE(latestLocalOnly.value(QStringLiteral("direction")).toString(), QStringLiteral("outgoing"));
        QCOMPARE(latestLocalOnly.value(QStringLiteral("decision")).toString(), QStringLiteral("allow"));
        QCOMPARE(latestLocalOnly.value(QStringLiteral("targetScope")).toString(), QStringLiteral("local"));

        const QVariantMap ipPolicy = service.SetIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("198.51.100.9")},
            {QStringLiteral("scope"), QStringLiteral("incoming")},
        });
        QCOMPARE(ipPolicy.value(QStringLiteral("ok")).toBool(), true);
        const QString policyId = ipPolicy.value(QStringLiteral("policy")).toMap()
                                     .value(QStringLiteral("policyId")).toString();
        QVERIFY(!policyId.isEmpty());

        const QVariantMap blocked = service.EvaluateConnection(QVariantMap{
            {QStringLiteral("pid"), -1},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
            {QStringLiteral("sourceIp"), QStringLiteral("198.51.100.9")},
        });
        QCOMPARE(blocked.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(blocked.value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
        QCOMPARE(blocked.value(QStringLiteral("source")).toString(), QStringLiteral("ip-policy"));
        QCOMPARE(blocked.value(QStringLiteral("policyId")).toString(), policyId);

        const QVariantList ipPolicies = service.ListIpPolicies();
        QCOMPARE(ipPolicies.size(), 1);
        QCOMPARE(ipPolicies.first().toMap().value(QStringLiteral("hitCount")).toInt(), 1);
    }

    void ip_policy_contract()
    {
        FirewallService service;
        const QVariantMap request{
            {QStringLiteral("type"), QStringLiteral("subnet")},
            {QStringLiteral("cidr"), QStringLiteral("203.0.113.0/24")},
            {QStringLiteral("scope"), QStringLiteral("both")},
            {QStringLiteral("reason"), QStringLiteral("manual-block")},
            {QStringLiteral("note"), QStringLiteral("soc-ticket-7")},
        };

        const QVariantMap result = service.SetIpPolicy(request);
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);
        QVERIFY(result.value(QStringLiteral("policy")).toMap().contains(QStringLiteral("targets")));
        QVERIFY(result.value(QStringLiteral("policy")).toMap().contains(QStringLiteral("policyId")));

        const QVariantList listed = service.ListIpPolicies();
        QCOMPARE(listed.size(), 1);
        QVERIFY(listed.first().toMap().value(QStringLiteral("targets")).toStringList().contains(
                    QStringLiteral("203.0.113.0/24")));
        QCOMPARE(listed.first().toMap().value(QStringLiteral("note")).toString(),
                 QStringLiteral("soc-ticket-7"));

        const QVariantMap cleared = service.ClearIpPolicies();
        QCOMPARE(cleared.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(service.ListIpPolicies().size(), 0);
    }

    void remove_ip_policy_contract()
    {
        FirewallService service;

        const QVariantMap first = service.SetIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("203.0.113.81")},
            {QStringLiteral("scope"), QStringLiteral("both")},
        });
        const QVariantMap second = service.SetIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("198.51.100.91")},
            {QStringLiteral("scope"), QStringLiteral("incoming")},
        });
        QCOMPARE(first.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(second.value(QStringLiteral("ok")).toBool(), true);

        const QString firstId = first.value(QStringLiteral("policy")).toMap().value(QStringLiteral("policyId")).toString();
        const QString secondId = second.value(QStringLiteral("policy")).toMap().value(QStringLiteral("policyId")).toString();
        QVERIFY(!firstId.isEmpty());
        QVERIFY(!secondId.isEmpty());
        QCOMPARE(service.ListIpPolicies().size(), 2);

        const QVariantMap removeResult = service.RemoveIpPolicy(firstId);
        QCOMPARE(removeResult.value(QStringLiteral("ok")).toBool(), true);
        const QVariantList listed = service.ListIpPolicies();
        QCOMPARE(listed.size(), 1);
        QCOMPARE(listed.first().toMap().value(QStringLiteral("policyId")).toString(), secondId);

        const QVariantMap missing = service.RemoveIpPolicy(QStringLiteral("policy-does-not-exist"));
        QCOMPARE(missing.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(missing.value(QStringLiteral("error")).toString(), QStringLiteral("policy-id-not-found"));

        const QVariantMap cleared = service.ClearIpPolicies();
        QCOMPARE(cleared.value(QStringLiteral("ok")).toBool(), true);
    }

    void list_ip_policies_prunes_expired_temporary_entries()
    {
        FirewallService service;

        const QVariantMap temporary = service.SetIpPolicy(QVariantMap{
            {QStringLiteral("ip"), QStringLiteral("203.0.113.98")},
            {QStringLiteral("scope"), QStringLiteral("incoming")},
            {QStringLiteral("temporary"), true},
            {QStringLiteral("duration"), QStringLiteral("1s")},
        });
        QCOMPARE(temporary.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(service.ListIpPolicies().size(), 1);

        QTest::qWait(2100);
        QCOMPARE(service.ListIpPolicies().size(), 0);
    }

    void app_policy_contract()
    {
        FirewallService service;
        QCOMPARE(service.ListAppPolicies().size(), 0);

        const QVariantMap first = service.SetAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.AppOne")},
            {QStringLiteral("decision"), QStringLiteral("deny")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        const QVariantMap second = service.SetAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.AppTwo")},
            {QStringLiteral("decision"), QStringLiteral("allow")},
            {QStringLiteral("direction"), QStringLiteral("both")},
        });
        QCOMPARE(first.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(second.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(service.ListAppPolicies().size(), 2);

        const QString firstId = first.value(QStringLiteral("policy")).toMap().value(QStringLiteral("policyId")).toString();
        QVERIFY(!firstId.isEmpty());

        const QVariantMap removeResult = service.RemoveAppPolicy(firstId);
        QCOMPARE(removeResult.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(service.ListAppPolicies().size(), 1);

        const QVariantMap missing = service.RemoveAppPolicy(QStringLiteral("missing-policy-id"));
        QCOMPARE(missing.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(missing.value(QStringLiteral("error")).toString(), QStringLiteral("policy-id-not-found"));

        const QVariantMap clearResult = service.ClearAppPolicies();
        QCOMPARE(clearResult.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(service.ListAppPolicies().size(), 0);

        const QVariantMap dedup1 = service.SetAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.Dedup")},
            {QStringLiteral("decision"), QStringLiteral("allow")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        const QVariantMap dedup2 = service.SetAppPolicy(QVariantMap{
            {QStringLiteral("appId"), QStringLiteral("org.example.Dedup")},
            {QStringLiteral("decision"), QStringLiteral("deny")},
            {QStringLiteral("direction"), QStringLiteral("incoming")},
        });
        QCOMPARE(dedup1.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(dedup2.value(QStringLiteral("ok")).toBool(), true);
        const QVariantList dedupList = service.ListAppPolicies();
        QCOMPARE(dedupList.size(), 1);
        QCOMPARE(dedupList.first().toMap().value(QStringLiteral("decision")).toString(), QStringLiteral("deny"));
    }

    void list_connections_shape_contract()
    {
        FirewallService service;
        const QVariantList connections = service.ListConnections();
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

private:
    QString m_storePath;
};

QTEST_GUILESS_MAIN(FirewallServiceDbusContractTest)
#include "firewallservice_dbus_contract_test.moc"
