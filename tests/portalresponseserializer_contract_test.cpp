#include <QtTest>

#include "../src/daemon/portald/portal-adapter/PortalResponseSerializer.h"

using namespace Slm::PortalAdapter;

class PortalResponseSerializerContractTest : public QObject
{
    Q_OBJECT

private slots:
    void success_includesCanonicalFields();
    void error_includesCanonicalFields();
};

void PortalResponseSerializerContractTest::success_includesCanonicalFields()
{
    const QVariantMap payload{
        {QStringLiteral("requestPath"), QStringLiteral("/org/desktop/portal/request/app/r1")},
        {QStringLiteral("pending"), true},
    };
    const QVariantMap out = PortalResponseSerializer::success(payload);
    QCOMPARE(out.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(out.value(QStringLiteral("error")).toString(), QString());
    QCOMPARE(out.value(QStringLiteral("response")).toUInt(), 0u);
    const QVariantMap results = out.value(QStringLiteral("results")).toMap();
    QCOMPARE(results.value(QStringLiteral("requestPath")).toString(),
             QStringLiteral("/org/desktop/portal/request/app/r1"));
    QCOMPARE(results.value(QStringLiteral("pending")).toBool(), true);
}

void PortalResponseSerializerContractTest::error_includesCanonicalFields()
{
    const QVariantMap denied = PortalResponseSerializer::error(
        PortalError::PermissionDenied, QStringLiteral("denied-for-test"));
    QCOMPARE(denied.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(denied.value(QStringLiteral("error")).toString(), QStringLiteral("PermissionDenied"));
    QCOMPARE(denied.value(QStringLiteral("response")).toUInt(), 1u);
    const QVariantMap deniedResults = denied.value(QStringLiteral("results")).toMap();
    QCOMPARE(deniedResults.value(QStringLiteral("error")).toString(),
             QStringLiteral("PermissionDenied"));

    const QVariantMap cancelled = PortalResponseSerializer::cancelled(QStringLiteral("cancelled-for-test"));
    QCOMPARE(cancelled.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(cancelled.value(QStringLiteral("error")).toString(), QStringLiteral("CancelledByUser"));
    QCOMPARE(cancelled.value(QStringLiteral("response")).toUInt(), 2u);
}

QTEST_MAIN(PortalResponseSerializerContractTest)
#include "portalresponseserializer_contract_test.moc"
