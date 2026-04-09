#include <QtTest/QtTest>

#include "../src/services/firewall/appidentityclient.h"

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
};

QTEST_GUILESS_MAIN(FirewallIdentityMappingContractTest)
#include "firewall_identity_mapping_contract_test.moc"
