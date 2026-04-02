#include <QtTest>

#include "src/services/secret/secretservice.h"

using Slm::Secret::SecretService;

class SecretServiceContractTest : public QObject
{
    Q_OBJECT

private slots:
    void storeGetDescribeDelete_roundtrip();
    void appIsolation_blocksCrossAccess();
};

void SecretServiceContractTest::storeGetDescribeDelete_roundtrip()
{
    SecretService service;
    const QString appId = QStringLiteral("org.example.app");
    const QVariantMap options{
        {QStringLiteral("namespace"), QStringLiteral("tokens")},
        {QStringLiteral("key"), QStringLiteral("api-token")},
        {QStringLiteral("label"), QStringLiteral("Primary Token")},
        {QStringLiteral("sensitivity"), QStringLiteral("high")},
    };
    const QByteArray secret("super-secret-token");

    const QVariantMap store = service.StoreSecret(appId, options, secret);
    QCOMPARE(store.value(QStringLiteral("ok")).toBool(), true);

    const QVariantMap describe = service.DescribeSecret(appId, options);
    QCOMPARE(describe.value(QStringLiteral("ok")).toBool(), true);
    const QVariantMap metadata = describe.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("ownerAppId")).toString(), appId);
    QCOMPARE(metadata.value(QStringLiteral("namespace")).toString(), QStringLiteral("tokens"));
    QCOMPARE(metadata.value(QStringLiteral("key")).toString(), QStringLiteral("api-token"));
    QCOMPARE(metadata.value(QStringLiteral("sensitivity")).toString(), QStringLiteral("high"));

    const QVariantMap get = service.GetSecret(appId, options);
    QCOMPARE(get.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(get.value(QStringLiteral("secret")).toByteArray(), secret);

    const QVariantMap del = service.DeleteSecret(appId, options);
    QCOMPARE(del.value(QStringLiteral("ok")).toBool(), true);

    const QVariantMap getAfterDelete = service.GetSecret(appId, options);
    QCOMPARE(getAfterDelete.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(getAfterDelete.value(QStringLiteral("error")).toString(), QStringLiteral("not-found"));
}

void SecretServiceContractTest::appIsolation_blocksCrossAccess()
{
    SecretService service;
    const QVariantMap options{
        {QStringLiteral("namespace"), QStringLiteral("cloud")},
        {QStringLiteral("key"), QStringLiteral("refresh-token")},
    };
    const QByteArray secret("cloud-token");

    const QVariantMap store = service.StoreSecret(QStringLiteral("org.example.owner"), options, secret);
    QCOMPARE(store.value(QStringLiteral("ok")).toBool(), true);

    const QVariantMap getFromOther = service.GetSecret(QStringLiteral("org.example.other"), options);
    QCOMPARE(getFromOther.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(getFromOther.value(QStringLiteral("error")).toString(), QStringLiteral("not-found"));
}

QTEST_MAIN(SecretServiceContractTest)
#include "secretservice_contract_test.moc"

