#include <QtTest/QtTest>

#define private public
#include "../src/core/workspace/kwinwaylandstatemodel.h"
#undef private

class KWinWaylandStateModelTest : public QObject {
    Q_OBJECT

private slots:
    void parseGeometryList_handlesCommonFormats()
    {
        KWinWaylandStateModel model;

        const QVariantList a = model.parseGeometryList(QStringLiteral("10,20 1280x720"));
        QCOMPARE(a.size(), 4);
        QCOMPARE(a.at(0).toInt(), 10);
        QCOMPARE(a.at(1).toInt(), 20);
        QCOMPARE(a.at(2).toInt(), 1280);
        QCOMPARE(a.at(3).toInt(), 720);

        const QVariantList b = model.parseGeometryList(QStringLiteral("x=3 y=4 w=800 h=600"));
        QCOMPARE(b.size(), 4);
        QCOMPARE(b.at(0).toInt(), 3);
        QCOMPARE(b.at(1).toInt(), 4);
        QCOMPARE(b.at(2).toInt(), 800);
        QCOMPARE(b.at(3).toInt(), 600);
    }

    void parseSupportWindowBlock_canonicalizesAppId()
    {
        KWinWaylandStateModel model;
        model.m_activeSpace = 2;

        const QStringList block{
            QStringLiteral("caption: Terminal"),
            QStringLiteral("desktopFileName: org.kde.konsole.desktop"),
            QStringLiteral("geometry: 40,50 900x600"),
            QStringLiteral("desktop: 3"),
            QStringLiteral("active: true"),
            QStringLiteral("minimized: false"),
        };

        const QVariantMap parsed = model.parseSupportWindowBlock(block);
        QVERIFY(!parsed.isEmpty());
        QCOMPARE(parsed.value(QStringLiteral("title")).toString(), QStringLiteral("Terminal"));
        QCOMPARE(parsed.value(QStringLiteral("appId")).toString(), QStringLiteral("org.kde.konsole"));
        QCOMPARE(parsed.value(QStringLiteral("space")).toInt(), 3);
        QCOMPARE(parsed.value(QStringLiteral("focused")).toBool(), true);
        QCOMPARE(parsed.value(QStringLiteral("mapped")).toBool(), true);
    }

    void scheduleDebounce_coalescesBursts()
    {
        KWinWaylandStateModel model;
        model.m_refreshWindowsCount = 0;
        model.m_refreshSpacesCount = 0;

        for (int i = 0; i < 10; ++i) {
            model.scheduleWindowRefresh(35);
            model.scheduleSpaceRefresh(25);
        }

        QTest::qWait(120);
        QCOMPARE(model.m_refreshWindowsCount, quint64(1));
        QCOMPARE(model.m_refreshSpacesCount, quint64(1));
    }

    void adaptiveFallbackInterval_movesWithinBounds()
    {
        KWinWaylandStateModel model;
        model.m_supportFallbackBaseIntervalMs = 4500;
        model.m_supportFallbackMinIntervalMs = 4500;
        model.m_supportFallbackMaxIntervalMs = 18000;
        model.m_objectModelMissStreak = 10;

        model.refreshWindows();
        QVERIFY(model.m_supportFallbackMinIntervalMs >= 4500);
        QVERIFY(model.m_supportFallbackMinIntervalMs <= 18000);
    }
};

QTEST_MAIN(KWinWaylandStateModelTest)
#include "kwinwaylandstatemodel_test.moc"

