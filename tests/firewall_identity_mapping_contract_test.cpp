#include <QtTest/QtTest>

#include "../src/services/firewall/appidentityclient.h"

#include <QCoreApplication>

class FirewallIdentityMappingContractTest : public QObject
{
    Q_OBJECT

private slots:
    void resolve_by_pid_shape_contract()
    {
        Slm::Firewall::AppIdentityClient client;
        const QVariantMap identity = client.resolveByPid(4242);

        QVERIFY(identity.contains(QStringLiteral("app_name")));
        QVERIFY(identity.contains(QStringLiteral("app_id")));
        QVERIFY(identity.contains(QStringLiteral("pid")));
        QVERIFY(identity.contains(QStringLiteral("executable")));
        QVERIFY(identity.contains(QStringLiteral("source")));
        QVERIFY(identity.contains(QStringLiteral("trust_level")));
        QVERIFY(identity.contains(QStringLiteral("context")));
        QCOMPARE(identity.value(QStringLiteral("pid")).toLongLong(), 4242);
    }

    void resolve_current_pid_has_executable()
    {
        Slm::Firewall::AppIdentityClient client;
        const qint64 pid = QCoreApplication::applicationPid();
        const QVariantMap identity = client.resolveByPid(pid);
        QCOMPARE(identity.value(QStringLiteral("pid")).toLongLong(), pid);
        QVERIFY(!identity.value(QStringLiteral("executable")).toString().trimmed().isEmpty());
        QVERIFY(identity.value(QStringLiteral("context")).toString() == QStringLiteral("gui")
                || identity.value(QStringLiteral("context")).toString() == QStringLiteral("cli")
                || identity.value(QStringLiteral("context")).toString() == QStringLiteral("interpreter"));
    }
};

QTEST_GUILESS_MAIN(FirewallIdentityMappingContractTest)
#include "firewall_identity_mapping_contract_test.moc"
