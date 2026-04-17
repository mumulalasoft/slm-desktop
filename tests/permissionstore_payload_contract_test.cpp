#include <QtTest>

#include "../src/core/permissions/PermissionStore.h"

#include <QTemporaryDir>

using namespace Slm::Permissions;

class PermissionStorePayloadContractTest : public QObject
{
    Q_OBJECT

private slots:
    void listPermissions_contract();
    void listAuditLogs_contract();
};

void PermissionStorePayloadContractTest::listPermissions_contract()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("permissions.db"));

    PermissionStore store;
    QVERIFY(store.open(dbPath));
    QVERIFY(store.savePermission(QStringLiteral("org.example.App"),
                                 Capability::SearchQueryFiles,
                                 DecisionType::AllowAlways,
                                 QStringLiteral("persistent"),
                                 QStringLiteral("search-query"),
                                 QStringLiteral("all")));

    const QVariantList rows = store.listPermissions();
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();

    const QSet<QString> expectedKeys{
        QStringLiteral("appId"),
        QStringLiteral("capability"),
        QStringLiteral("decision"),
        QStringLiteral("scope"),
        QStringLiteral("resourceType"),
        QStringLiteral("resourceId"),
        QStringLiteral("updatedAt"),
    };
    QCOMPARE(QSet<QString>(row.keyBegin(), row.keyEnd()), expectedKeys);
    QCOMPARE(row.value(QStringLiteral("appId")).toString(), QStringLiteral("org.example.App"));
    QCOMPARE(row.value(QStringLiteral("capability")).toString(),
             QStringLiteral("Search.QueryFiles"));
    QCOMPARE(row.value(QStringLiteral("decision")).toString(),
             QStringLiteral("allow_always"));
    QCOMPARE(row.value(QStringLiteral("scope")).toString(),
             QStringLiteral("persistent"));
    QCOMPARE(row.value(QStringLiteral("resourceType")).toString(),
             QStringLiteral("search-query"));
    QCOMPARE(row.value(QStringLiteral("resourceId")).toString(),
             QStringLiteral("all"));
    QVERIFY(row.value(QStringLiteral("updatedAt")).canConvert<qint64>());
}

void PermissionStorePayloadContractTest::listAuditLogs_contract()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("permissions.db"));

    PermissionStore store;
    QVERIFY(store.open(dbPath));
    QVERIFY(store.appendAudit(QStringLiteral("org.example.App"),
                              Capability::SearchQueryFiles,
                              DecisionType::Allow,
                              QStringLiteral("search-query"),
                              QStringLiteral("third-party-low-risk")));

    const QVariantList rows = store.listAuditLogs(10);
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();

    const QSet<QString> expectedKeys{
        QStringLiteral("appId"),
        QStringLiteral("capability"),
        QStringLiteral("decision"),
        QStringLiteral("resourceType"),
        QStringLiteral("timestamp"),
        QStringLiteral("reason"),
    };
    QCOMPARE(QSet<QString>(row.keyBegin(), row.keyEnd()), expectedKeys);
    QCOMPARE(row.value(QStringLiteral("appId")).toString(), QStringLiteral("org.example.App"));
    QCOMPARE(row.value(QStringLiteral("capability")).toString(),
             QStringLiteral("Search.QueryFiles"));
    QCOMPARE(row.value(QStringLiteral("decision")).toString(), QStringLiteral("allow"));
    QCOMPARE(row.value(QStringLiteral("resourceType")).toString(), QStringLiteral("search-query"));
    QCOMPARE(row.value(QStringLiteral("reason")).toString(), QStringLiteral("third-party-low-risk"));
    QVERIFY(row.value(QStringLiteral("timestamp")).canConvert<qint64>());
}

QTEST_MAIN(PermissionStorePayloadContractTest)
#include "permissionstore_payload_contract_test.moc"

