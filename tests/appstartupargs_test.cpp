#include <QtTest>

#include "../src/bootstrap/appstartupargs.h"

class AppStartupArgsTest : public QObject
{
    Q_OBJECT

private slots:
    void defaults_whenNoArgs()
    {
        const AppStartupArgs out = parseAppStartupArgs(QStringList{QStringLiteral("appSlm_Desktop")});
        QVERIFY(!out.startWindowed);
        QCOMPARE(out.windowWidth, 0);
        QCOMPARE(out.windowHeight, 0);
    }

    void parsesWindowedFlag()
    {
        const AppStartupArgs out = parseAppStartupArgs(
            QStringList{QStringLiteral("appSlm_Desktop"), QStringLiteral("--windowed")});
        QVERIFY(out.startWindowed);
        QCOMPARE(out.windowWidth, 0);
        QCOMPARE(out.windowHeight, 0);
    }

    void parsesWindowSize_validFormats()
    {
        const AppStartupArgs lower = parseAppStartupArgs(
            QStringList{QStringLiteral("appSlm_Desktop"), QStringLiteral("--window-size=1480x900")});
        QCOMPARE(lower.windowWidth, 1480);
        QCOMPARE(lower.windowHeight, 900);

        const AppStartupArgs upper = parseAppStartupArgs(
            QStringList{QStringLiteral("appSlm_Desktop"), QStringLiteral("--window-size=1200X800")});
        QCOMPARE(upper.windowWidth, 1200);
        QCOMPARE(upper.windowHeight, 800);
    }

    void ignoresInvalidWindowSize()
    {
        const AppStartupArgs out = parseAppStartupArgs(
            QStringList{QStringLiteral("appSlm_Desktop"), QStringLiteral("--window-size=abcx900")});
        QCOMPARE(out.windowWidth, 0);
        QCOMPARE(out.windowHeight, 0);
    }

    void ignoresTooSmallDigitWindowSize()
    {
        const AppStartupArgs out = parseAppStartupArgs(
            QStringList{QStringLiteral("appSlm_Desktop"), QStringLiteral("--window-size=1x2")});
        QCOMPARE(out.windowWidth, 0);
        QCOMPARE(out.windowHeight, 0);
    }

    void takesFirstValidWindowSize()
    {
        const AppStartupArgs out = parseAppStartupArgs(QStringList{
            QStringLiteral("appSlm_Desktop"),
            QStringLiteral("--window-size=1280x720"),
            QStringLiteral("--window-size=1920x1080"),
        });
        QCOMPARE(out.windowWidth, 1280);
        QCOMPARE(out.windowHeight, 720);
    }
};

QTEST_MAIN(AppStartupArgsTest)
#include "appstartupargs_test.moc"
