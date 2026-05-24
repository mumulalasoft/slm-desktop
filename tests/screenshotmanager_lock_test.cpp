#include <QtTest/QtTest>

#include "../src/services/screenshot/screenshotmanager.h"

class ScreenshotManagerLockTest : public QObject
{
    Q_OBJECT

private:
    static void verifyLocked(const QVariantMap &result, const QString &expectedMode)
    {
        QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(result.value(QStringLiteral("mode")).toString(), expectedMode);
        QCOMPARE(result.value(QStringLiteral("error")).toString(),
                 QStringLiteral("session-locked"));
    }

private slots:
    void noProvider_doesNotBlock()
    {
        ScreenshotManager mgr;
        // Without a provider, the fullscreen path will fail for non-lock reasons
        // (no compositor IPC available in tests). The guard must not preempt
        // that with "session-locked".
        const QVariantMap result = mgr.captureFullscreen(QStringLiteral("/tmp/slm-screenshot-test-noprov.png"));
        QVERIFY(result.value(QStringLiteral("error")).toString() !=
                QStringLiteral("session-locked"));
    }

    void unlocked_doesNotBlock()
    {
        ScreenshotManager mgr;
        mgr.setLockedProvider([]() { return false; });
        const QVariantMap result = mgr.captureFullscreen(QStringLiteral("/tmp/slm-screenshot-test-unlocked.png"));
        QVERIFY(result.value(QStringLiteral("error")).toString() !=
                QStringLiteral("session-locked"));
    }

    void locked_blocksDispatch_capture_fullscreen()
    {
        ScreenshotManager mgr;
        mgr.setLockedProvider([]() { return true; });

        verifyLocked(mgr.capture(QStringLiteral("fullscreen")), QStringLiteral("fullscreen"));
        verifyLocked(mgr.captureFullscreen(), QStringLiteral("fullscreen"));
    }

    void locked_blocksDispatch_capture_window()
    {
        ScreenshotManager mgr;
        mgr.setLockedProvider([]() { return true; });

        verifyLocked(mgr.capture(QStringLiteral("window"), 0, 0, 100, 100),
                     QStringLiteral("window"));
        verifyLocked(mgr.captureWindow(0, 0, 100, 100), QStringLiteral("window"));
    }

    void locked_blocksDispatch_capture_area()
    {
        ScreenshotManager mgr;
        mgr.setLockedProvider([]() { return true; });

        verifyLocked(mgr.capture(QStringLiteral("area"), 0, 0, 100, 100),
                     QStringLiteral("area"));
        verifyLocked(mgr.capture(QStringLiteral("area")), QStringLiteral("area"));
        verifyLocked(mgr.captureArea(), QStringLiteral("area"));
        verifyLocked(mgr.captureAreaRect(0, 0, 100, 100), QStringLiteral("area"));
    }

    void locked_dispatch_caseInsensitive_andTrimmed()
    {
        ScreenshotManager mgr;
        mgr.setLockedProvider([]() { return true; });

        // The `capture` dispatcher trims/lowercases mode; the locked result
        // should reflect the normalized mode string.
        verifyLocked(mgr.capture(QStringLiteral("  Fullscreen  ")),
                     QStringLiteral("fullscreen"));
    }

    void provider_polled_perCall()
    {
        ScreenshotManager mgr;
        bool locked = true;
        mgr.setLockedProvider([&locked]() { return locked; });

        verifyLocked(mgr.captureFullscreen(), QStringLiteral("fullscreen"));

        locked = false;
        const QVariantMap result = mgr.captureFullscreen(QStringLiteral("/tmp/slm-screenshot-test-toggle.png"));
        QVERIFY(result.value(QStringLiteral("error")).toString() !=
                QStringLiteral("session-locked"));
    }
};

QTEST_MAIN(ScreenshotManagerLockTest)
#include "screenshotmanager_lock_test.moc"
