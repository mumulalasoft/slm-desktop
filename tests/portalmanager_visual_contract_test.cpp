#include <QtTest/QtTest>

#include "../portalmethodnames.h"
#include "../src/daemon/portald/portalmanager.h"

class PortalManagerVisualContractTest : public QObject
{
    Q_OBJECT

private slots:
    void pickColor_returnsNotImplementedContract()
    {
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("title"), QStringLiteral("Pick Accent Color")},
            {QStringLiteral("source"), QStringLiteral("topbar")},
        };

        const QVariantMap out = manager.PickColor(options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kPickColor));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("not-implemented"));
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
    }

    void wallpaper_returnsNotImplementedContract()
    {
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("mode"), QStringLiteral("set")},
            {QStringLiteral("uri"), QStringLiteral("file:///home/garis/Pictures/wallpaper.jpg")},
        };

        const QVariantMap out = manager.Wallpaper(options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kWallpaper));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("not-implemented"));
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
    }
};

QTEST_MAIN(PortalManagerVisualContractTest)
#include "portalmanager_visual_contract_test.moc"
