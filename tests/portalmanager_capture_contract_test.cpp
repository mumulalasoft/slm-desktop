#include <QtTest/QtTest>

#include "../portalmethodnames.h"
#include "../src/daemon/portald/portalmanager.h"

class PortalManagerCaptureContractTest : public QObject
{
    Q_OBJECT

private slots:
    void screenshot_returnsNotImplementedContract()
    {
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("screen")},
            {QStringLiteral("interactive"), true},
        };

        const QVariantMap out = manager.Screenshot(options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kScreenshot));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("not-implemented"));
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
    }

    void screencast_returnsNotImplementedContract()
    {
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("fps"), 60},
            {QStringLiteral("withAudio"), true},
        };

        const QVariantMap out = manager.ScreenCast(options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kScreenCast));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("not-implemented"));
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
    }
};

QTEST_MAIN(PortalManagerCaptureContractTest)
#include "portalmanager_capture_contract_test.moc"
