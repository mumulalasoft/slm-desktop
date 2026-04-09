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

        const QVariantMap settingsPayloadAfter = settings.GetSettings();
        const QVariantMap settingsAfterRoot = settingsPayloadAfter.value(QStringLiteral("settings")).toMap();
        const QVariantMap firewallAfter = settingsAfterRoot.value(QStringLiteral("firewall")).toMap();
        QCOMPARE(firewallAfter.value(QStringLiteral("mode")).toString(), QStringLiteral("custom"));
        QCOMPARE(firewallAfter.value(QStringLiteral("enabled")).toBool(), false);
    }

private:
    QString m_storePath;
};

QTEST_GUILESS_MAIN(FirewallServiceDbusContractTest)
#include "firewallservice_dbus_contract_test.moc"
