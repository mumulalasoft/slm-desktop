#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "src/login/libslmlogin/slmplatformcheck.h"

using namespace Slm::Login;

namespace {

bool writeExecutable(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (file.write(content) != content.size()) {
        file.close();
        return false;
    }
    file.close();
    return QFile::setPermissions(path, QFileDevice::ReadOwner
                                       | QFileDevice::WriteOwner
                                       | QFileDevice::ExeOwner
                                       | QFileDevice::ReadUser
                                       | QFileDevice::ExeUser
                                       | QFileDevice::ReadGroup
                                       | QFileDevice::ExeGroup
                                       | QFileDevice::ReadOther
                                       | QFileDevice::ExeOther);
}

QByteArray stubScript()
{
    return QByteArrayLiteral("#!/bin/sh\nexit 0\n");
}

} // namespace

class SlmPlatformCheckCompatTest : public QObject
{
    Q_OBJECT

private slots:
    void checkAll_writesCompatibilityReport();
    void checkAll_missingRuntimeDir_fails();
};

void SlmPlatformCheckCompatTest::checkAll_writesCompatibilityReport()
{
    QTemporaryDir temp;
    QVERIFY2(temp.isValid(), "temp dir invalid");

    const QString binDir = temp.path() + QStringLiteral("/bin");
    const QString runtimeDir = temp.path() + QStringLiteral("/runtime");
    const QString configRoot = temp.path() + QStringLiteral("/config");
    QVERIFY(QDir().mkpath(binDir));
    QVERIFY(QDir().mkpath(runtimeDir));
    QVERIFY(QDir().mkpath(configRoot));

    QVERIFY(writeExecutable(binDir + QStringLiteral("/kwin_wayland"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/slm-shell"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/desktopd"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/slm-svcmgrd"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/slm-loggerd"), stubScript()));

    qputenv("PATH", (binDir + QStringLiteral(":/usr/bin:/bin")).toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", configRoot.toLocal8Bit());
    qputenv("XDG_RUNTIME_DIR", runtimeDir.toLocal8Bit());
    qputenv("XDG_SESSION_TYPE", QByteArrayLiteral("wayland"));
    qputenv("XDG_CURRENT_DESKTOP", QByteArrayLiteral("SLM"));
    qputenv("SLM_OFFICIAL_SESSION", QByteArrayLiteral("1"));
    qputenv("SLM_TEST_ALLOW_NONSTANDARD_RUNTIME_DIR", QByteArrayLiteral("1"));

    const PlatformChecker checker;
    const PlatformStatus status = checker.checkAll(QStringLiteral("kwin_wayland"),
                                                   QStringLiteral("slm-shell"));
    QVERIFY2(status.issues.isEmpty(), qPrintable(status.issues.join(QLatin1String("; "))));
    QVERIFY(!status.reportPath.isEmpty());
    QVERIFY(QFileInfo::exists(status.reportPath));

    QFile report(status.reportPath);
    QVERIFY(report.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(report.readAll());
    QVERIFY(doc.isObject());
    const QJsonObject obj = doc.object();
    QVERIFY(obj.contains(QStringLiteral("issues")));
    QVERIFY(obj.contains(QStringLiteral("warnings")));
    QVERIFY(obj.contains(QStringLiteral("checklist")));
}

void SlmPlatformCheckCompatTest::checkAll_missingRuntimeDir_fails()
{
    QTemporaryDir temp;
    QVERIFY2(temp.isValid(), "temp dir invalid");

    const QString binDir = temp.path() + QStringLiteral("/bin");
    QVERIFY(QDir().mkpath(binDir));

    QVERIFY(writeExecutable(binDir + QStringLiteral("/kwin_wayland"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/slm-shell"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/desktopd"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/slm-svcmgrd"), stubScript()));
    QVERIFY(writeExecutable(binDir + QStringLiteral("/slm-loggerd"), stubScript()));

    qputenv("PATH", (binDir + QStringLiteral(":/usr/bin:/bin")).toLocal8Bit());
    qputenv("SLM_OFFICIAL_SESSION", QByteArrayLiteral("1"));
    qunsetenv("XDG_RUNTIME_DIR");
    qunsetenv("SLM_TEST_ALLOW_NONSTANDARD_RUNTIME_DIR");
    qunsetenv("SLM_TEST_ACCEPT_REGULAR_WAYLAND_SOCKET");

    const PlatformChecker checker;
    const PlatformStatus status = checker.checkAll(QStringLiteral("kwin_wayland"),
                                                   QStringLiteral("slm-shell"));
    QVERIFY(!status.ok);
    QVERIFY(!status.issues.isEmpty());
    QVERIFY(status.issues.join(QLatin1Char('\n')).contains(QStringLiteral("XDG_RUNTIME_DIR")));
}

QTEST_MAIN(SlmPlatformCheckCompatTest)
#include "slmplatformcheck_compat_test.moc"
