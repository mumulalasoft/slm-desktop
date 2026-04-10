#include <QtTest/QtTest>

#include "../src/apps/settings/modules/network/firewallserviceclient.h"
#include "../src/daemon/firewalld/firewallservice.h"
#include "../src/services/settingsd/settingsservice.h"

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QSignalSpy>

class FirewallServiceClientDbusSignalFlowTest : public QObject
{
    Q_OBJECT

private:
    static QVariantMap makeEvaluateRequest(const QString &ip)
    {
        return QVariantMap{
            {QStringLiteral("direction"), QStringLiteral("outgoing")},
            {QStringLiteral("destinationIp"), ip},
            {QStringLiteral("destinationPort"), 443},
            {QStringLiteral("pid"), -1},
        };
    }

private slots:
    void init()
    {
        const QString testName = QString::fromLatin1(QTest::currentTestFunction());
        m_storePath = QDir::tempPath()
                + QStringLiteral("/slm-firewall-client-flow-test-")
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

    void dbusSignalFlow_contract()
    {
        SettingsService settings;
        QString settingsError;
        const bool settingsAvailable = settings.start(&settingsError);
        if (!settingsAvailable) {
            QVERIFY2(settingsError.isEmpty(), qPrintable(settingsError));
        }

        FirewallService service;
        QString serviceError;
        if (!service.start(&serviceError)) {
            QSKIP(qPrintable(QStringLiteral("cannot start firewall service for signal flow test: %1")
                                 .arg(serviceError)));
        }
        QSignalSpy promptSpy(&service, &FirewallService::ConnectionPromptRequested);

        FirewallServiceClient client;
        QTRY_VERIFY(client.available());
        QTRY_COMPARE(client.pendingPrompts().size(), 0);

        QDBusInterface firewallIface(QStringLiteral("org.slm.Desktop.Firewall"),
                                     QStringLiteral("/org/slm/Desktop/Firewall"),
                                     QStringLiteral("org.slm.Desktop.Firewall"),
                                     QDBusConnection::sessionBus());
        QVERIFY2(firewallIface.isValid(), "DBus firewall interface unavailable");

        const QVariantMap request = makeEvaluateRequest(QStringLiteral("203.0.113.10"));
        QDBusReply<QVariantMap> evaluateReply = firewallIface.call(QStringLiteral("EvaluateConnection"), request);
        QVERIFY2(evaluateReply.isValid(), "EvaluateConnection DBus call failed");
        const QVariantMap evaluateOut = evaluateReply.value();
        QCOMPARE(evaluateOut.value(QStringLiteral("decision")).toString(), QStringLiteral("prompt"));
        QTRY_COMPARE(promptSpy.count(), 1);

        QTRY_COMPARE(client.pendingPrompts().size(), 1);
        const QVariantMap row = client.pendingPrompts().first().toMap();
        QCOMPARE(row.value(QStringLiteral("duplicateCount")).toInt(), 0);
        const QVariantMap rowRequest = row.value(QStringLiteral("request")).toMap();
        QCOMPARE(rowRequest.value(QStringLiteral("destinationIp")).toString(), QStringLiteral("203.0.113.10"));
    }

private:
    QString m_storePath;
};

QTEST_GUILESS_MAIN(FirewallServiceClientDbusSignalFlowTest)
#include "firewallserviceclient_dbus_signal_flow_test.moc"
