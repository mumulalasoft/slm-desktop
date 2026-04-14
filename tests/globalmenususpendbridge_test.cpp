#include <QtTest/QtTest>

#include "../src/services/globalmenu/globalmenususpendbridge.h"

class GlobalMenuSuspendBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void resumeSetsPendingThenSleepClearsPending()
    {
        GlobalMenuSuspendBridge bridge;
        QVERIFY(!bridge.resumePending());

        QVERIFY(QMetaObject::invokeMethod(&bridge,
                                          "onPrepareForSleep",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, false)));
        QVERIFY(bridge.resumePending());

        QVERIFY(QMetaObject::invokeMethod(&bridge,
                                          "onPrepareForSleep",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, true)));
        QVERIFY(!bridge.resumePending());
    }
};

QTEST_MAIN(GlobalMenuSuspendBridgeTest)
#include "globalmenususpendbridge_test.moc"

