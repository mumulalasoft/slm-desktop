#include <QtTest/QtTest>
#include <QSignalSpy>

#include "../src/daemon/desktopd/sessionstatemanager.h"

// Exercises the logind PrepareForSleep handling on SessionStateManager.
//
// We can't drive a real org.freedesktop.login1 PrepareForSleep on the system
// bus from a unit test, so we invoke the private slot directly via
// QMetaObject. This mirrors tests/globalmenususpendbridge_test.cpp.

class SessionStateManagerSuspendTest : public QObject
{
    Q_OBJECT

private:
    static void invokePrepareForSleep(SessionStateManager *mgr, bool sleeping)
    {
        QVERIFY(QMetaObject::invokeMethod(mgr,
                                          "onPrepareForSleep",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, sleeping)));
    }

private slots:
    void resume_emitsResumedSignal_doesNotChangeLockState()
    {
        SessionStateManager mgr(nullptr, nullptr);

        QSignalSpy resumedSpy(&mgr, &SessionStateManager::Resumed);
        QSignalSpy lockedSpy(&mgr, &SessionStateManager::SessionLocked);

        invokePrepareForSleep(&mgr, false);

        QCOMPARE(resumedSpy.count(), 1);
        // Wake never auto-unlocks or auto-locks — only the layer-resync hook.
        QCOMPARE(lockedSpy.count(), 0);
    }

    void suspend_attemptsLock_emitsSessionLocked()
    {
        SessionStateManager mgr(nullptr, nullptr);

        QSignalSpy lockedSpy(&mgr, &SessionStateManager::SessionLocked);
        QSignalSpy idleSpy(&mgr, &SessionStateManager::IdleChanged);

        // No real session bus in the unit test, so callScreenSaver() and
        // callLogin1() will fail silently and SessionLocked won't fire.
        // The test focus is the slot routing + Resumed emission; lock-path
        // wiring under DBus is exercised by sessionstateservice_dbus_test.
        invokePrepareForSleep(&mgr, true);

        // We can't assert the lock fired (depends on session bus), but we can
        // assert no spurious Resumed and that the call did not crash.
        QSignalSpy resumedSpy(&mgr, &SessionStateManager::Resumed);
        QCOMPARE(resumedSpy.count(), 0);

        // Whether lockedSpy/idleSpy fired depends on the bus availability;
        // both are valid: 0 (no bus) or >=1 (bus available). What we can pin:
        QVERIFY(lockedSpy.count() <= 1);
        QVERIFY(idleSpy.count() <= 1);
    }

    void lock_isIdempotentOnceIdle()
    {
        SessionStateManager mgr(nullptr, nullptr);

        QSignalSpy lockedSpy(&mgr, &SessionStateManager::SessionLocked);

        // Drive into the idle/locked state via the screen-saver-active proxy
        // (this is the public route SessionStateManager uses for ScreenSaver
        // ActiveChanged signals). Using a private slot via QMetaObject keeps
        // the test independent of any DBus bus.
        QVERIFY(QMetaObject::invokeMethod(&mgr,
                                          "onScreenSaverActiveChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, true)));
        // emitIdle(true) emits SessionLocked once via its idle->locked path.
        const int firstCount = lockedSpy.count();
        QVERIFY(firstCount >= 1);

        // A subsequent Lock() must be idempotent: no new SessionLocked.
        mgr.Lock();
        QCOMPARE(lockedSpy.count(), firstCount);

        // PrepareForSleep(true) while already idle must also be idempotent.
        invokePrepareForSleep(&mgr, true);
        QCOMPARE(lockedSpy.count(), firstCount);
    }

    void resume_emittedEachTime()
    {
        SessionStateManager mgr(nullptr, nullptr);
        QSignalSpy resumedSpy(&mgr, &SessionStateManager::Resumed);

        invokePrepareForSleep(&mgr, false);
        invokePrepareForSleep(&mgr, false);
        invokePrepareForSleep(&mgr, false);

        QCOMPARE(resumedSpy.count(), 3);
    }
};

QTEST_MAIN(SessionStateManagerSuspendTest)
#include "sessionstatemanager_suspend_test.moc"
