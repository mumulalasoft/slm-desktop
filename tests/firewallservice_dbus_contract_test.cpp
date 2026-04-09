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
        QVERIFY2(settings.start(&settingsError), qPrintable(settingsError));

        FirewallService service;
        service.SetMode(QStringLiteral("public"));
        QVariantMap status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("mode")).toString(), QStringLiteral("public"));

        const QVariantMap settingsPayload = settings.GetSettings();
        const QVariantMap settingsRoot = settingsPayload.value(QStringLiteral("settings")).toMap();
        const QVariantMap firewall = settingsRoot.value(QStringLiteral("firewall")).toMap();
        QCOMPARE(firewall.value(QStringLiteral("mode")).toString(), QStringLiteral("public"));

        service.SetMode(QStringLiteral("custom"));
        status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("mode")).toString(), QStringLiteral("custom"));

        const QVariantMap setEnabledResult = service.SetEnabled(false);
        QCOMPARE(setEnabledResult.value(QStringLiteral("enabled")).toBool(), false);
        QCOMPARE(setEnabledResult.value(QStringLiteral("packetApplied")).toBool(), true);

        const QVariantMap settingsPayloadAfter = settings.GetSettings();
        const QVariantMap settingsAfterRoot = settingsPayloadAfter.value(QStringLiteral("settings")).toMap();
        const QVariantMap firewallAfter = settingsAfterRoot.value(QStringLiteral("firewall")).toMap();
        QCOMPARE(firewallAfter.value(QStringLiteral("mode")).toString(), QStringLiteral("custom"));
        QCOMPARE(firewallAfter.value(QStringLiteral("enabled")).toBool(), false);
    }

    void default_policy_roundtrip_contract()
    {
        SettingsService settings;
        QString settingsError;
        QVERIFY2(settings.start(&settingsError), qPrintable(settingsError));

        FirewallService service;
        QVariantMap status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("deny"));
        QCOMPARE(status.value(QStringLiteral("defaultOutgoingPolicy")).toString(), QStringLiteral("allow"));

        service.SetDefaultIncomingPolicy(QStringLiteral("prompt"));
        const QVariantMap outgoingResult = service.SetDefaultOutgoingPolicy(QStringLiteral("deny"));
        QCOMPARE(outgoingResult.value(QStringLiteral("packetApplied")).toBool(), true);

        status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("prompt"));
        QCOMPARE(status.value(QStringLiteral("defaultOutgoingPolicy")).toString(), QStringLiteral("deny"));

        // Invalid value should be normalized to previous valid value.
        service.SetDefaultIncomingPolicy(QStringLiteral("invalid-policy"));
        status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("prompt"));

        const QVariantMap settingsPayload = settings.GetSettings();
        const QVariantMap settingsRoot = settingsPayload.value(QStringLiteral("settings")).toMap();
        const QVariantMap firewall = settingsRoot.value(QStringLiteral("firewall")).toMap();
        QCOMPARE(firewall.value(QStringLiteral("defaultIncomingPolicy")).toString(), QStringLiteral("prompt"));
        QCOMPARE(firewall.value(QStringLiteral("defaultOutgoingPolicy")).toString(), QStringLiteral("deny"));
    }

    void ip_policy_contract()
    {
        FirewallService service;
        const QVariantMap request{
            {QStringLiteral("type"), QStringLiteral("subnet")},
            {QStringLiteral("cidr"), QStringLiteral("203.0.113.0/24")},
            {QStringLiteral("scope"), QStringLiteral("both")},
            {QStringLiteral("reason"), QStringLiteral("manual-block")},
        };

        const QVariantMap result = service.SetIpPolicy(request);
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);
        QVERIFY(result.value(QStringLiteral("policy")).toMap().contains(QStringLiteral("targets")));
        QVERIFY(result.value(QStringLiteral("policy")).toMap().contains(QStringLiteral("policyId")));

        const QVariantList listed = service.ListIpPolicies();
        QCOMPARE(listed.size(), 1);
        QVERIFY(listed.first().toMap().value(QStringLiteral("targets")).toStringList().contains(
                    QStringLiteral("203.0.113.0/24")));

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

private:
    QString m_storePath;
};

QTEST_GUILESS_MAIN(FirewallServiceDbusContractTest)
#include "firewallservice_dbus_contract_test.moc"
