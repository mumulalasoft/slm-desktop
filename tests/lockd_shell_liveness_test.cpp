#include <QtTest/QtTest>
#include <QSignalSpy>

#include "../src/daemon/lockd/shellliveness.h"

// Exercises the slm-lockd ShellLiveness routing without requiring a real
// session bus. The DBus owner-changed slot is invoked directly via
// QMetaObject so we can verify shellLost / shellReturned routing.

class LockdShellLivenessTest : public QObject
{
    Q_OBJECT

private:
    static void invokeOwnerChanged(QObject *target,
                                   const QString &name,
                                   const QString &oldOwner,
                                   const QString &newOwner)
    {
        QVERIFY(QMetaObject::invokeMethod(target,
                                          "onServiceOwnerChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, name),
                                          Q_ARG(QString, oldOwner),
                                          Q_ARG(QString, newOwner)));
    }

private slots:
    void shellLost_emittedOnNameOwnerVanish()
    {
        Slm::Lockd::ShellLiveness liveness(QStringLiteral("org.slm.Desktop"));
        QSignalSpy lostSpy(&liveness, &Slm::Lockd::ShellLiveness::shellLost);
        QSignalSpy returnedSpy(&liveness, &Slm::Lockd::ShellLiveness::shellReturned);

        // Prime: shell goes alive (no real bus → start as not alive).
        invokeOwnerChanged(&liveness, QStringLiteral("org.slm.Desktop"),
                           QString(), QStringLiteral(":1.42"));
        QCOMPARE(returnedSpy.count(), 1);
        QVERIFY(liveness.shellAlive());

        // Then disappears.
        invokeOwnerChanged(&liveness, QStringLiteral("org.slm.Desktop"),
                           QStringLiteral(":1.42"), QString());
        QCOMPARE(lostSpy.count(), 1);
        QCOMPARE(lostSpy.first().first().toString(),
                 QStringLiteral("shell-process-died"));
        QVERIFY(!liveness.shellAlive());
    }

    void shellReturned_emittedOnReclaim()
    {
        Slm::Lockd::ShellLiveness liveness(QStringLiteral("org.slm.Desktop"));
        QSignalSpy returnedSpy(&liveness, &Slm::Lockd::ShellLiveness::shellReturned);

        invokeOwnerChanged(&liveness, QStringLiteral("org.slm.Desktop"),
                           QString(), QStringLiteral(":1.99"));
        QCOMPARE(returnedSpy.count(), 1);
        QVERIFY(liveness.shellAlive());
    }

    void differentBusName_isIgnored()
    {
        Slm::Lockd::ShellLiveness liveness(QStringLiteral("org.slm.Desktop"));
        QSignalSpy lostSpy(&liveness, &Slm::Lockd::ShellLiveness::shellLost);

        invokeOwnerChanged(&liveness, QStringLiteral("org.slm.SomethingElse"),
                           QStringLiteral(":1.5"), QString());
        QCOMPARE(lostSpy.count(), 0);
    }

    void duplicateState_doesNotEmitTwice()
    {
        Slm::Lockd::ShellLiveness liveness(QStringLiteral("org.slm.Desktop"));
        QSignalSpy lostSpy(&liveness, &Slm::Lockd::ShellLiveness::shellLost);

        // Prime alive.
        invokeOwnerChanged(&liveness, QStringLiteral("org.slm.Desktop"),
                           QString(), QStringLiteral(":1.7"));

        // Two consecutive disappear events for the same name.
        invokeOwnerChanged(&liveness, QStringLiteral("org.slm.Desktop"),
                           QStringLiteral(":1.7"), QString());
        invokeOwnerChanged(&liveness, QStringLiteral("org.slm.Desktop"),
                           QString(), QString());

        QCOMPARE(lostSpy.count(), 1);
    }
};

QTEST_MAIN(LockdShellLivenessTest)
#include "lockd_shell_liveness_test.moc"
