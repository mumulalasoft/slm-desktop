#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include "../src/daemon/desktopd/sessionstatemanager.h"
#include "../src/daemon/desktopd/sessionstateservice.h"
#include "../src/daemon/desktopd/sessionunlockthrottle.h"

namespace {
constexpr const char kService[] = "org.slm.SessionState";
constexpr const char kPath[] = "/org/slm/SessionState";
constexpr const char kIface[] = "org.slm.SessionState1";

QString busSkipReasonIfAny()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return QStringLiteral("session bus is not available in this test environment");
    }
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return QStringLiteral("session bus interface is not available");
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        return QStringLiteral("org.slm.SessionState is already owned by another process");
    }
    return QString();
}

QString throttleStatePathForTest()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.trimmed().isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/slm");
    }
    return QDir(base).filePath(QStringLiteral("session_unlock_throttle.json"));
}
}

class SessionStateServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void cleanup()
    {
        QFile::remove(throttleStatePathForTest());
    }

    void ping_returnsOkMap()
    {
        const QString skipReason = busSkipReasonIfAny();
        if (!skipReason.isEmpty()) QSKIP(qPrintable(skipReason));
        QDBusConnection bus = QDBusConnection::sessionBus();

        SessionStateManager manager(nullptr, nullptr);
        SessionStateService service(&manager);
        if (!service.serviceRegistered()) {
            QSKIP("unable to register org.slm.SessionState on session bus in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Ping"));
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QVERIFY(out.value(QStringLiteral("ok")).toBool());
        QCOMPARE(out.value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));
        QCOMPARE(out.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));

        QDBusReply<QVariantMap> capsReply = iface.call(QStringLiteral("GetCapabilities"));
        QVERIFY(capsReply.isValid());
        const QVariantMap caps = capsReply.value();
        QVERIFY(caps.value(QStringLiteral("ok")).toBool());
        QCOMPARE(caps.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));
        const QStringList list = caps.value(QStringLiteral("capabilities")).toStringList();
        QVERIFY(list.contains(QStringLiteral("inhibit")));
        QVERIFY(list.contains(QStringLiteral("lock")));
    }

    void dbusInhibit_returnsCookie()
    {
        const QString skipReason = busSkipReasonIfAny();
        if (!skipReason.isEmpty()) QSKIP(qPrintable(skipReason));
        QDBusConnection bus = QDBusConnection::sessionBus();

        SessionStateManager manager(nullptr, nullptr);
        SessionStateService service(&manager);
        if (!service.serviceRegistered()) {
            QSKIP("unable to register org.slm.SessionState on session bus in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<uint> reply = iface.call(QStringLiteral("Inhibit"),
                                            QStringLiteral("test"),
                                            uint(0));
        QVERIFY(reply.isValid());
        QVERIFY(reply.value() > 0u);
    }

    void managerSignals_areForwardedOnBusService()
    {
        const QString skipReason = busSkipReasonIfAny();
        if (!skipReason.isEmpty()) QSKIP(qPrintable(skipReason));
        QDBusConnection bus = QDBusConnection::sessionBus();

        SessionStateManager manager(nullptr, nullptr);
        SessionStateService service(&manager);
        if (!service.serviceRegistered()) {
            QSKIP("unable to register org.slm.SessionState on session bus in this environment");
        }

        QSignalSpy idleSpy(&service, SIGNAL(IdleChanged(bool)));
        QSignalSpy lockedSpy(&service, SIGNAL(SessionLocked()));
        QSignalSpy unlockedSpy(&service, SIGNAL(SessionUnlocked()));
        QVERIFY(idleSpy.isValid());
        QVERIFY(lockedSpy.isValid());
        QVERIFY(unlockedSpy.isValid());

        QVERIFY(QMetaObject::invokeMethod(&manager,
                                          "onScreenSaverActiveChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, true)));
        QTRY_VERIFY_WITH_TIMEOUT(idleSpy.count() >= 1, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(lockedSpy.count() >= 1, 1000);
        QCOMPARE(idleSpy.at(0).at(0).toBool(), true);

        QVERIFY(QMetaObject::invokeMethod(&manager,
                                          "onScreenSaverActiveChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, false)));
        QTRY_VERIFY_WITH_TIMEOUT(idleSpy.count() >= 2, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(unlockedSpy.count() >= 1, 1000);
        QCOMPARE(idleSpy.at(1).at(0).toBool(), false);
    }

    void unlock_rateLimited_returnsRetryAfter()
    {
        const QString skipReason = busSkipReasonIfAny();
        if (!skipReason.isEmpty()) QSKIP(qPrintable(skipReason));

        QDBusConnection bus = QDBusConnection::sessionBus();
        const QString callerKey = bus.baseService().trimmed();
        if (callerKey.isEmpty()) {
            QSKIP("session bus caller base service is empty");
        }

        QHash<QString, Slm::Desktopd::SessionUnlockThrottleState> states;
        Slm::Desktopd::SessionUnlockThrottleState state;
        state.level = 2;
        state.failStreak = 0;
        state.lastFailureMs = QDateTime::currentMSecsSinceEpoch();
        state.lockoutUntilMs = state.lastFailureMs + 45 * 1000;
        states.insert(callerKey, state);
        QVERIFY(Slm::Desktopd::SessionUnlockThrottle::save(throttleStatePathForTest(), states));

        SessionStateManager manager(nullptr, nullptr);
        SessionStateService service(&manager);
        if (!service.serviceRegistered()) {
            QSKIP("unable to register org.slm.SessionState on session bus in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Unlock"),
                                                   QStringLiteral("does-not-matter"));
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("rate-limited"));
        QVERIFY(out.value(QStringLiteral("retry_after_sec")).toInt() > 0);
    }

    void unlock_authFailed_notRateLimited_hasZeroRetryAfter()
    {
        const QString skipReason = busSkipReasonIfAny();
        if (!skipReason.isEmpty()) QSKIP(qPrintable(skipReason));

        QDBusConnection bus = QDBusConnection::sessionBus();

        // Ensure no preloaded throttle state influences this case.
        QFile::remove(throttleStatePathForTest());

        SessionStateManager manager(nullptr, nullptr);
        SessionStateService service(&manager);
        if (!service.serviceRegistered()) {
            QSKIP("unable to register org.slm.SessionState on session bus in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Unlock"),
                                                   QStringLiteral("definitely-wrong-password"));
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        const QString err = out.value(QStringLiteral("error")).toString();
        QVERIFY(!err.trimmed().isEmpty());
        QVERIFY(err != QStringLiteral("rate-limited"));
        QCOMPARE(out.value(QStringLiteral("retry_after_sec"), 0).toInt(), 0);
    }

    void unlock_fiveFailures_triggersEscalationAndThenRateLimited()
    {
        const QString skipReason = busSkipReasonIfAny();
        if (!skipReason.isEmpty()) QSKIP(qPrintable(skipReason));

        QDBusConnection bus = QDBusConnection::sessionBus();
        QFile::remove(throttleStatePathForTest());

        SessionStateManager manager(nullptr, nullptr);
        SessionStateService service(&manager);
        if (!service.serviceRegistered()) {
            QSKIP("unable to register org.slm.SessionState on session bus in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        // First 4 failures: still not rate-limited.
        for (int i = 0; i < 4; ++i) {
            QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Unlock"),
                                                       QStringLiteral("wrong-password"));
            QVERIFY(reply.isValid());
            const QVariantMap out = reply.value();
            QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
            const QString err = out.value(QStringLiteral("error")).toString();
            QVERIFY(!err.trimmed().isEmpty());
            QVERIFY(err != QStringLiteral("rate-limited"));
            QCOMPARE(out.value(QStringLiteral("retry_after_sec"), 0).toInt(), 0);
        }

        // 5th failure crosses threshold; lockout should become active.
        QDBusReply<QVariantMap> fifthReply = iface.call(QStringLiteral("Unlock"),
                                                        QStringLiteral("wrong-password"));
        QVERIFY(fifthReply.isValid());
        const QVariantMap fifth = fifthReply.value();
        QCOMPARE(fifth.value(QStringLiteral("ok")).toBool(), false);
        QVERIFY(fifth.value(QStringLiteral("retry_after_sec"), 0).toInt() > 0);

        // Next attempt should be blocked by pre-check with explicit rate-limited error.
        QDBusReply<QVariantMap> blockedReply = iface.call(QStringLiteral("Unlock"),
                                                          QStringLiteral("wrong-password"));
        QVERIFY(blockedReply.isValid());
        const QVariantMap blocked = blockedReply.value();
        QCOMPARE(blocked.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(blocked.value(QStringLiteral("error")).toString(), QStringLiteral("rate-limited"));
        QVERIFY(blocked.value(QStringLiteral("retry_after_sec"), 0).toInt() > 0);
    }

    void unlock_successWithEnvPassword_resetsPersistedThrottle()
    {
        const QString skipReason = busSkipReasonIfAny();
        if (!skipReason.isEmpty()) QSKIP(qPrintable(skipReason));

        const QString testPassword = QProcessEnvironment::systemEnvironment()
                                         .value(QStringLiteral("SLM_TEST_UNLOCK_PASSWORD"));
        if (testPassword.isEmpty()) {
            QSKIP("Set SLM_TEST_UNLOCK_PASSWORD to run success-path unlock test");
        }

        QDBusConnection bus = QDBusConnection::sessionBus();
        const QString callerKey = bus.baseService().trimmed();
        if (callerKey.isEmpty()) {
            QSKIP("session bus caller base service is empty");
        }

        // Seed elevated throttle state (without active lockout) before service startup.
        QHash<QString, Slm::Desktopd::SessionUnlockThrottleState> states;
        Slm::Desktopd::SessionUnlockThrottleState seeded;
        seeded.failStreak = 4;
        seeded.level = 3;
        seeded.lockoutUntilMs = 0;
        seeded.lastFailureMs = QDateTime::currentMSecsSinceEpoch();
        states.insert(callerKey, seeded);
        QVERIFY(Slm::Desktopd::SessionUnlockThrottle::save(throttleStatePathForTest(), states));

        SessionStateManager manager(nullptr, nullptr);
        SessionStateService service(&manager);
        if (!service.serviceRegistered()) {
            QSKIP("unable to register org.slm.SessionState on session bus in this environment");
        }

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Unlock"), testPassword);
        QVERIFY(reply.isValid());
        const QVariantMap out = reply.value();
        if (!out.value(QStringLiteral("ok")).toBool()) {
            QSKIP("Unlock failed; ensure SLM_TEST_UNLOCK_PASSWORD matches current user password");
        }

        QHash<QString, Slm::Desktopd::SessionUnlockThrottleState> reloaded;
        bool changed = false;
        QVERIFY(Slm::Desktopd::SessionUnlockThrottle::load(throttleStatePathForTest(),
                                                           reloaded,
                                                           QDateTime::currentMSecsSinceEpoch(),
                                                           &changed));

        const Slm::Desktopd::SessionUnlockThrottleState after = reloaded.value(callerKey);
        QCOMPARE(after.failStreak, 0);
        QCOMPARE(after.level, 0);
        QCOMPARE(after.lockoutUntilMs, 0);
        QCOMPARE(after.lastFailureMs, 0);
    }
};

QTEST_MAIN(SessionStateServiceDbusTest)
#include "sessionstateservice_dbus_test.moc"
