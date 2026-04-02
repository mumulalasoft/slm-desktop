#include <QtTest>

#include "src/daemon/desktopd/sessionunlockthrottle.h"

#include <QDir>
#include <QTemporaryDir>

using Slm::Desktopd::SessionUnlockThrottle;
using Slm::Desktopd::SessionUnlockThrottleState;

class SessionUnlockThrottleTest : public QObject
{
    Q_OBJECT

private slots:
    void lockoutEscalationAndDecay();
    void persistenceRoundTrip();
    void recordSuccess_resetsThrottleState();
};

void SessionUnlockThrottleTest::lockoutEscalationAndDecay()
{
    SessionUnlockThrottleState state;
    const qint64 t0 = 1'000'000;

    for (int i = 0; i < 5; ++i) {
        SessionUnlockThrottle::recordFailure(state, t0 + i * 1000);
    }
    QCOMPARE(state.level, 1);
    QVERIFY(state.lockoutUntilMs > t0);
    QCOMPARE(state.failStreak, 0);

    const qint64 noDecayYet = state.lastFailureMs + qint64(SessionUnlockThrottle::decayWindowSeconds() - 1) * 1000;
    SessionUnlockThrottleState unchanged = state;
    QVERIFY(!SessionUnlockThrottle::applyDecay(unchanged, noDecayYet));
    QCOMPARE(unchanged.level, state.level);

    const qint64 decayTime = state.lastFailureMs + qint64(SessionUnlockThrottle::decayWindowSeconds()) * 1000;
    QVERIFY(SessionUnlockThrottle::applyDecay(state, decayTime));
    QCOMPARE(state.level, 0);
    QCOMPARE(state.failStreak, 0);
    QCOMPARE(state.lockoutUntilMs, 0);
    QCOMPARE(state.lastFailureMs, 0);
}

void SessionUnlockThrottleTest::persistenceRoundTrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = QDir(tmp.path()).filePath(QStringLiteral("throttle.json"));

    QHash<QString, SessionUnlockThrottleState> in;
    SessionUnlockThrottleState a;
    a.failStreak = 3;
    a.level = 2;
    a.lockoutUntilMs = 123456;
    a.lastFailureMs = 120000;
    in.insert(QStringLiteral("caller.a"), a);

    QVERIFY(SessionUnlockThrottle::save(path, in));

    QHash<QString, SessionUnlockThrottleState> out;
    bool changed = false;
    QVERIFY(SessionUnlockThrottle::load(path, out, 120001, &changed));
    QVERIFY(!changed);
    QCOMPARE(out.size(), 1);
    const SessionUnlockThrottleState loaded = out.value(QStringLiteral("caller.a"));
    QCOMPARE(loaded.failStreak, a.failStreak);
    QCOMPARE(loaded.level, a.level);
    QCOMPARE(loaded.lockoutUntilMs, a.lockoutUntilMs);
    QCOMPARE(loaded.lastFailureMs, a.lastFailureMs);

    QHash<QString, SessionUnlockThrottleState> out2;
    changed = false;
    const qint64 decayNow = a.lastFailureMs + qint64(SessionUnlockThrottle::decayWindowSeconds()) * 1000;
    QVERIFY(SessionUnlockThrottle::load(path, out2, decayNow, &changed));
    QVERIFY(changed);
    const SessionUnlockThrottleState decayed = out2.value(QStringLiteral("caller.a"));
    QCOMPARE(decayed.level, 1);
}

void SessionUnlockThrottleTest::recordSuccess_resetsThrottleState()
{
    SessionUnlockThrottleState state;
    const qint64 now = 2'000'000;
    for (int i = 0; i < 7; ++i) {
        SessionUnlockThrottle::recordFailure(state, now + i * 1000);
    }
    QVERIFY(state.level >= 1);
    QVERIFY(state.lastFailureMs > 0);
    QVERIFY(state.lockoutUntilMs > 0);

    SessionUnlockThrottle::recordSuccess(state);
    QCOMPARE(state.failStreak, 0);
    QCOMPARE(state.level, 0);
    QCOMPARE(state.lockoutUntilMs, 0);
    QCOMPARE(state.lastFailureMs, 0);
}

QTEST_MAIN(SessionUnlockThrottleTest)
#include "sessionunlockthrottle_test.moc"
