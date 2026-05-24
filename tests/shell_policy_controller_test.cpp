#include <QtTest/QtTest>

#include "../src/core/shell/shellpolicycontroller.h"
#include "../src/core/shell/shellstatecontroller.h"

namespace {
QVariantMap focusedFullscreenWindow(const QString &appId)
{
    return {
        { QStringLiteral("viewId"), QStringLiteral("view-1") },
        { QStringLiteral("title"), QStringLiteral("Focused Fullscreen") },
        { QStringLiteral("appId"), appId },
        { QStringLiteral("x"), 0 },
        { QStringLiteral("y"), 0 },
        { QStringLiteral("width"), 1920 },
        { QStringLiteral("height"), 1080 },
        { QStringLiteral("mapped"), true },
        { QStringLiteral("minimized"), false },
        { QStringLiteral("focused"), true },
        { QStringLiteral("fullscreen"), true },
    };
}

QVariantMap focusedFullscreenShellSurface(const QString &title)
{
    QVariantMap window = focusedFullscreenWindow(QString());
    window[QStringLiteral("title")] = title;
    return window;
}
}

class ShellPolicyControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultPolicy_isNormal()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);

        QCOMPARE(policy.visibilityPolicy(), ShellPolicyController::Normal);
        QCOMPARE(policy.normalPolicy(), int(ShellPolicyController::Normal));
        QCOMPARE(policy.appFullscreenPolicy(), int(ShellPolicyController::AppFullscreen));
        QCOMPARE(policy.immersiveFullscreenPolicy(), int(ShellPolicyController::ImmersiveFullscreen));
        QCOMPARE(policy.presentationPolicy(), int(ShellPolicyController::Presentation));
        QCOMPARE(policy.lockedPolicy(), int(ShellPolicyController::Locked));
        QVERIFY(policy.crownSurfaceVisible());
        QVERIFY(policy.crownContentVisible());
        QVERIFY(policy.appDeckSurfaceVisible());
        QVERIFY(policy.appDeckContentVisible());
        QVERIFY(policy.notificationsAllowed());
        QVERIFY(!policy.edgeRevealEnabled());
    }

    void appFullscreen_autoHidesShellAndAllowsEdgeReveal()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);

        policy.setWindowSnapshot({ focusedFullscreenWindow(QStringLiteral("firefox")) },
                                 QStringLiteral("test"));

        QCOMPARE(policy.visibilityPolicy(), ShellPolicyController::AppFullscreen);
        QVERIFY(policy.fullscreenActive());
        QVERIFY(policy.crownSurfaceVisible());
        QVERIFY(!policy.crownContentVisible());
        QVERIFY(policy.appDeckSurfaceVisible());
        QVERIFY(!policy.appDeckContentVisible());
        QVERIFY(policy.notificationsAllowed());
        QVERIFY(policy.compactNotifications());
        QVERIFY(policy.edgeRevealEnabled());

        policy.requestTopEdgeReveal(QStringLiteral("test-top"));
        QVERIFY(policy.crownContentVisible());
        QVERIFY(!policy.appDeckContentVisible());

        policy.requestBottomEdgeReveal(QStringLiteral("test-bottom"));
        QVERIFY(!policy.crownContentVisible());
        QVERIFY(policy.appDeckContentVisible());

        policy.clearReveal(QStringLiteral("test-clear"));
        QVERIFY(!policy.crownContentVisible());
        QVERIFY(!policy.appDeckContentVisible());
    }

    void immersiveFullscreen_suppressesShellAndReveal()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);

        policy.setWindowSnapshot({ focusedFullscreenWindow(QStringLiteral("steam")) },
                                 QStringLiteral("test"));

        QCOMPARE(policy.visibilityPolicy(), ShellPolicyController::ImmersiveFullscreen);
        QVERIFY(policy.immersiveFullscreen());
        QVERIFY(!policy.crownSurfaceVisible());
        QVERIFY(!policy.appDeckSurfaceVisible());
        QVERIFY(policy.notificationsSuppressed());
        QVERIFY(!policy.edgeRevealEnabled());

        policy.requestTopEdgeReveal(QStringLiteral("test-top"));
        QVERIFY(!policy.crownContentVisible());
    }

    void presentationFullscreen_hidesShellWithoutReveal()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);

        policy.setWindowSnapshot({ focusedFullscreenWindow(QStringLiteral("libreoffice-impress")) },
                                 QStringLiteral("test"));

        QCOMPARE(policy.visibilityPolicy(), ShellPolicyController::Presentation);
        QVERIFY(policy.presentationActive());
        QVERIFY(!policy.crownSurfaceVisible());
        QVERIFY(!policy.appDeckSurfaceVisible());
        QVERIFY(policy.notificationsSuppressed());
        QVERIFY(!policy.edgeRevealEnabled());
    }

    void lockScreen_overridesFullscreenPolicy()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);

        policy.setWindowSnapshot({ focusedFullscreenWindow(QStringLiteral("firefox")) },
                                 QStringLiteral("test"));
        QCOMPARE(policy.visibilityPolicy(), ShellPolicyController::AppFullscreen);

        state.setLockScreenActive(true);
        QCOMPARE(policy.visibilityPolicy(), ShellPolicyController::Locked);
        QVERIFY(policy.locked());
        QVERIFY(!policy.crownSurfaceVisible());
        QVERIFY(!policy.appDeckSurfaceVisible());
        QVERIFY(policy.notificationsSuppressed());
    }

    void shellLayerSurfaceTitle_isIgnoredForFullscreenPolicy()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);

        policy.setWindowSnapshot({ focusedFullscreenShellSurface(QStringLiteral("SLM AppDeck Surface")) },
                                 QStringLiteral("test"));

        QCOMPARE(policy.visibilityPolicy(), ShellPolicyController::Normal);
        QVERIFY(!policy.fullscreenActive());
        QVERIFY(policy.appDeckSurfaceVisible());
        QVERIFY(policy.appDeckContentVisible());
    }
};

QTEST_MAIN(ShellPolicyControllerTest)
#include "shell_policy_controller_test.moc"
