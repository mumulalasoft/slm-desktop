#include <QtTest/QtTest>

#include "../src/services/globalmenu/globalmenuadaptivecontroller.h"

class GlobalMenuAdaptiveControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToFullMode()
    {
        GlobalMenuAdaptiveController controller;
        QCOMPARE(controller.mode(), QStringLiteral("full"));
        QVERIFY(controller.isFullMode());
        QVERIFY(!controller.isCompactMode());
        QVERIFY(!controller.isFocusMode());
    }

    void autoSwitchesToCompactOnSmallWidth()
    {
        GlobalMenuAdaptiveController controller;
        controller.setAvailableWidth(800);
        QCOMPARE(controller.mode(), QStringLiteral("compact"));
        QVERIFY(controller.isCompactMode());
    }

    void fullscreenForcesFocusMode()
    {
        GlobalMenuAdaptiveController controller;
        controller.setAvailableWidth(1400);
        controller.setWindowFullscreen(true);
        QCOMPARE(controller.mode(), QStringLiteral("focus"));
        QVERIFY(controller.isFocusMode());
    }

    void userOverrideWinsOverAuto()
    {
        GlobalMenuAdaptiveController controller;
        controller.setAvailableWidth(700);
        QCOMPARE(controller.mode(), QStringLiteral("compact"));

        controller.setUserOverride(QStringLiteral("full"));
        QCOMPARE(controller.mode(), QStringLiteral("full"));
        QVERIFY(controller.isFullMode());

        controller.setWindowFullscreen(true);
        QCOMPARE(controller.mode(), QStringLiteral("full"));

        controller.setUserOverride(QStringLiteral("auto"));
        QCOMPARE(controller.mode(), QStringLiteral("focus"));
        QVERIFY(controller.isFocusMode());
    }
};

QTEST_MAIN(GlobalMenuAdaptiveControllerTest)
#include "globalmenuadaptivecontroller_test.moc"
