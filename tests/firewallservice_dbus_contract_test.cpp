#include <QtTest/QtTest>

#include "../src/daemon/firewalld/firewallservice.h"

class FirewallServiceDbusContractTest : public QObject
{
    Q_OBJECT

private slots:
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
        FirewallService service;
        service.SetMode(QStringLiteral("public"));
        QVariantMap status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("mode")).toString(), QStringLiteral("public"));

        service.SetMode(QStringLiteral("custom"));
        status = service.GetStatus();
        QCOMPARE(status.value(QStringLiteral("mode")).toString(), QStringLiteral("custom"));
    }
};

QTEST_GUILESS_MAIN(FirewallServiceDbusContractTest)
#include "firewallservice_dbus_contract_test.moc"
