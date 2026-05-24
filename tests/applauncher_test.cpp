#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include "../src/core/launcher/applauncher.h"

class AppLauncherTest : public QObject
{
    Q_OBJECT

private slots:
    void expandsDesktopExecWithoutFileArguments();
    void flatpakExecWithoutFileArgumentsDropsForwardingMarkers();
    void keepsPortalEnvironmentByDefaultInSlmSession();
    void mapsInternalWaylandDisplayToCompatibilitySocket();
    void keepsInternalWaylandDisplayWhenCompatibilitySocketMissing();
    void desktopLaunchDoesNotWrapPrivateDbusSession();
    void flatpakDesktopIgnoresUnavailableTryExec();
};

void AppLauncherTest::expandsDesktopExecWithoutFileArguments()
{
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "Temporary directory was not created");

    const QString desktopPath = dir.filePath(QStringLiteral("sample.desktop"));
    QFile file(desktopPath);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write desktop file");
    file.write(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Sample App\n"
        "Exec=/usr/bin/sample-app --flag %F %u %% %k\n"
        "Terminal=false\n");
    file.close();

    const QString expanded = AppLauncher::expandDesktopExecCommand(desktopPath);
    QVERIFY2(!expanded.isEmpty(), "Expanded command must not be empty");
    QVERIFY(expanded.contains(QStringLiteral("/usr/bin/sample-app")));
    QVERIFY(expanded.contains(QStringLiteral("--flag")));
    QVERIFY(expanded.contains(QFileInfo(desktopPath).absoluteFilePath()));
    QVERIFY(!expanded.contains(QStringLiteral("%F")));
    QVERIFY(!expanded.contains(QStringLiteral("%u")));
    QVERIFY(expanded.contains(QStringLiteral("%")));
}

void AppLauncherTest::flatpakExecWithoutFileArgumentsDropsForwardingMarkers()
{
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "Temporary directory was not created");

    const QString desktopPath = dir.filePath(QStringLiteral("flatpak-sample.desktop"));
    QFile file(desktopPath);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write desktop file");
    file.write(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Flatpak Sample\n"
        "Exec=/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=sample --file-forwarding org.example.Sample @@u %U @@\n"
        "Terminal=false\n"
        "X-Flatpak=org.example.Sample\n");
    file.close();

    const QString expanded = AppLauncher::expandDesktopExecCommand(desktopPath);
    QVERIFY2(!expanded.isEmpty(), "Expanded command must not be empty");
    QVERIFY(expanded.contains(QStringLiteral("/usr/bin/flatpak")));
    QVERIFY(expanded.contains(QStringLiteral("org.example.Sample")));
    QVERIFY(!expanded.contains(QStringLiteral("%U")));
    QVERIFY(!expanded.contains(QStringLiteral("@@")));
    QVERIFY(!expanded.contains(QStringLiteral("--file-forwarding")));
}

void AppLauncherTest::keepsPortalEnvironmentByDefaultInSlmSession()
{
    QProcessEnvironment env;
    env.insert(QStringLiteral("SLM_OFFICIAL_SESSION"), QStringLiteral("1"));
    env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("SLM"));
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("wayland-0"));

    const QProcessEnvironment prepared = AppLauncher::prepareLaunchEnvironment(env);

    QVERIFY(prepared.value(QStringLiteral("GTK_USE_PORTAL")).isEmpty());
    QVERIFY(prepared.value(QStringLiteral("GIO_USE_PORTALS")).isEmpty());
    QVERIFY(prepared.value(QStringLiteral("QT_NO_XDG_DESKTOP_PORTAL")).isEmpty());
}

void AppLauncherTest::mapsInternalWaylandDisplayToCompatibilitySocket()
{
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "Temporary directory was not created");

    QFile compatSocketPlaceholder(dir.filePath(QStringLiteral("wayland-0")));
    QVERIFY(compatSocketPlaceholder.open(QIODevice::WriteOnly | QIODevice::Truncate));
    compatSocketPlaceholder.write("socket-placeholder\n");
    compatSocketPlaceholder.close();

    QProcessEnvironment env;
    env.insert(QStringLiteral("SLM_OFFICIAL_SESSION"), QStringLiteral("1"));
    env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("SLM"));
    env.insert(QStringLiteral("XDG_RUNTIME_DIR"), dir.path());
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("slm-wayland-0"));

    const QProcessEnvironment prepared = AppLauncher::prepareLaunchEnvironment(env);
    QCOMPARE(prepared.value(QStringLiteral("WAYLAND_DISPLAY")), QStringLiteral("wayland-0"));
}

void AppLauncherTest::keepsInternalWaylandDisplayWhenCompatibilitySocketMissing()
{
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "Temporary directory was not created");

    QProcessEnvironment env;
    env.insert(QStringLiteral("SLM_OFFICIAL_SESSION"), QStringLiteral("1"));
    env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("SLM"));
    env.insert(QStringLiteral("XDG_RUNTIME_DIR"), dir.path());
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("slm-wayland-0"));

    const QProcessEnvironment prepared = AppLauncher::prepareLaunchEnvironment(env);
    QCOMPARE(prepared.value(QStringLiteral("WAYLAND_DISPLAY")), QStringLiteral("slm-wayland-0"));
}

void AppLauncherTest::desktopLaunchDoesNotWrapPrivateDbusSession()
{
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "Temporary directory was not created");

    const QString fakeAppPath = dir.filePath(QStringLiteral("sample-gui-app"));
    QFile fakeApp(fakeAppPath);
    QVERIFY2(fakeApp.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write fake app");
    fakeApp.write("#!/bin/sh\nexit 0\n");
    fakeApp.close();
    QVERIFY(QFile::setPermissions(fakeAppPath,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                      | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                      | QFileDevice::ReadOther | QFileDevice::ExeOther));

    const QString desktopPath = dir.filePath(QStringLiteral("sample-gui.desktop"));
    QFile desktop(desktopPath);
    QVERIFY2(desktop.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write desktop file");
    const QString desktopBody = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Sample GUI\n"
        "Exec=%1\n"
        "Terminal=false\n").arg(fakeAppPath);
    desktop.write(desktopBody.toUtf8());
    desktop.close();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("SLM_OFFICIAL_SESSION"), QStringLiteral("1"));
    env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("SLM"));

    const AppLaunchResult result = AppLauncher::launchDesktopFile(desktopPath, env);

    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(!result.privateSessionBus);
    QVERIFY(!result.launchCommand.contains(QStringLiteral("dbus-run-session")));
}

void AppLauncherTest::flatpakDesktopIgnoresUnavailableTryExec()
{
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "Temporary directory was not created");

    const QString fakeFlatpakPath = dir.filePath(QStringLiteral("flatpak"));
    QFile fakeFlatpak(fakeFlatpakPath);
    QVERIFY2(fakeFlatpak.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write fake flatpak");
    fakeFlatpak.write("#!/bin/sh\nexit 0\n");
    fakeFlatpak.close();
    QVERIFY(QFile::setPermissions(fakeFlatpakPath,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                      | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                      | QFileDevice::ReadOther | QFileDevice::ExeOther));

    const QString desktopPath = dir.filePath(QStringLiteral("flatpak-launch.desktop"));
    QFile desktop(desktopPath);
    QVERIFY2(desktop.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write desktop file");
    const QString desktopBody = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Flatpak Launch\n"
        "Exec=%1 run --file-forwarding org.example.Sample @@u %%U @@\n"
        "TryExec=org.example.DoesNotExist\n"
        "Terminal=false\n"
        "X-Flatpak=org.example.Sample\n").arg(fakeFlatpakPath);
    desktop.write(desktopBody.toUtf8());
    desktop.close();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("SLM_OFFICIAL_SESSION"), QStringLiteral("1"));
    env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("SLM"));

    const AppLaunchResult result = AppLauncher::launchDesktopFile(desktopPath, env);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(!result.launchCommand.contains(QStringLiteral("--file-forwarding")));
    QVERIFY(!result.launchCommand.contains(QStringLiteral("@@")));
}

QTEST_MAIN(AppLauncherTest)
#include "applauncher_test.moc"
