#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QCryptographicHash>

class PortalInstallScriptIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void installsBetaProfileIntoUserScopedPaths();
    void respectsPortalsConfNoOverwriteUnlessForced();
    void dryRunOutputContract_includesDeterministicStatusLines();
    void invalidProfile_returnsNonZeroAndHelpfulError();
    void legacyProfile_isSupportedAndUsesLegacyTemplate();
    void profileArgument_isCaseInsensitive();
    void cliProfileArgument_overridesEnvProfile();
    void envOnlyProfile_isUsedWhenCliProfileMissing();
    void envOnlyProfile_isCaseInsensitiveWhenCliProfileMissing();
    void prodAlias_isSupportedAndMapsToProduction();
    void envOnlyProdAlias_isSupported();
    void cliWhitespaceProfile_overridesEnvAndFails();
    void dryRunOutput_reportsCanonicalInstalledPathsForCustomXdgHomes();
    void productionNoDefaultOverwrite_updatesOnlySlmPortalsConf_andLogsRemainConsistent();
    void repeatedInstall_sameProfile_isIdempotentForGeneratedArtifacts();
    void emptyProfile_fallsBackToMvp_andWhitespaceProfileIsRejected();
    void customBinaryPath_isSubstitutedIntoSystemdUnitAndDbusService();

private:
    static QString readValue(const QString &path, const QString &key)
    {
        QSettings s(path, QSettings::IniFormat);
        s.beginGroup(QStringLiteral("preferred"));
        const QString value = s.value(key).toString();
        s.endGroup();
        return value;
    }

    static QByteArray runInstaller(const QStringList &args,
                                   const QProcessEnvironment &env,
                                   int *exitCodeOut = nullptr)
    {
        QProcess p;
        p.setProgram(QStringLiteral("bash"));
        QStringList allArgs;
        allArgs << QStringLiteral(SLM_INSTALL_PORTAL_SCRIPT);
        allArgs << args;
        p.setArguments(allArgs);
        p.setProcessEnvironment(env);
        p.start();
        if (!p.waitForFinished(30000)) {
            if (exitCodeOut) {
                *exitCodeOut = -1;
            }
            return QByteArrayLiteral("installer script did not finish in time");
        }
        if (exitCodeOut) {
            *exitCodeOut = p.exitCode();
        }
        const QByteArray out = p.readAllStandardOutput() + p.readAllStandardError();
        return out;
    }

    static QByteArray sha256File(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
    }

    static QByteArray readFile(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return {};
        }
        return f.readAll();
    }
};

void PortalInstallScriptIntegrationTest::installsBetaProfileIntoUserScopedPaths()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("beta")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString defaultConf = QDir(confDir).filePath(QStringLiteral("portals.conf"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    const QString portalFile =
        QDir(xdgDataHome).filePath(QStringLiteral("xdg-desktop-portal/portals/slm.portal"));
    const QString dbusFile =
        QDir(xdgDataHome).filePath(
            QStringLiteral("dbus-1/services/org.freedesktop.impl.portal.desktop.slm.service"));

    QVERIFY2(QFile::exists(defaultConf), qPrintable(defaultConf));
    QVERIFY2(QFile::exists(slmConf), qPrintable(slmConf));
    QVERIFY2(QFile::exists(portalFile), qPrintable(portalFile));
    QVERIFY2(QFile::exists(dbusFile), qPrintable(dbusFile));

    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("gtk"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.Documents")),
             QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.GlobalShortcuts")),
             QStringLiteral("slm"));
}

void PortalInstallScriptIntegrationTest::respectsPortalsConfNoOverwriteUnlessForced()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    QVERIFY(QDir().mkpath(confDir));
    QVERIFY(QDir().mkpath(xdgDataHome));

    const QString defaultConf = QDir(confDir).filePath(QStringLiteral("portals.conf"));
    {
        QFile f(defaultConf);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("[preferred]\ndefault=wlr\n");
        f.close();
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));

    int exitCode = -1;
    QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("production")},
                                  env,
                                  &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    // Existing portals.conf must remain unchanged when SLM_PORTAL_SET_DEFAULT is not set.
    QCOMPARE(readValue(defaultConf, QStringLiteral("default")), QStringLiteral("wlr"));

    // slm-portals.conf still receives selected profile content.
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("slm"));

    // Force overwrite and verify portals.conf now follows selected profile.
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));
    log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("production")},
                       env,
                       &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QCOMPARE(readValue(defaultConf, QStringLiteral("default")), QStringLiteral("slm"));
}

void PortalInstallScriptIntegrationTest::dryRunOutputContract_includesDeterministicStatusLines()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("beta")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    QVERIFY2(log.contains("[install-portald] skipping systemctl operations (SLM_PORTAL_SKIP_SYSTEMCTL=1)"),
             log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: beta"), log.constData());
    QVERIFY2(log.contains("[install-portald] set default portals.conf: 1"), log.constData());
    QVERIFY2(log.contains("[install-portald] skip systemctl: 1"), log.constData());
    QVERIFY2(log.contains("[install-portald] installed: "), log.constData());
}

void PortalInstallScriptIntegrationTest::invalidProfile_returnsNonZeroAndHelpfulError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("unknown-profile")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode != 0, log.constData());
    QVERIFY2(log.contains("[install-portald] unknown portal profile:"), log.constData());
    QVERIFY2(log.contains("use one of: mvp | beta | production | legacy"), log.constData());
}

void PortalInstallScriptIntegrationTest::legacyProfile_isSupportedAndUsesLegacyTemplate()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("legacy")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: legacy"), log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString defaultConf = QDir(confDir).filePath(QStringLiteral("portals.conf"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QVERIFY2(QFile::exists(defaultConf), qPrintable(defaultConf));
    QVERIFY2(QFile::exists(slmConf), qPrintable(slmConf));

    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("gtk"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.FileChooser")),
             QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.Screenshot")),
             QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral(""));
}

void PortalInstallScriptIntegrationTest::profileArgument_isCaseInsensitive()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("BETA")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: beta"), log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("gtk"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral("slm"));
}

void PortalInstallScriptIntegrationTest::cliProfileArgument_overridesEnvProfile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_PROFILE"), QStringLiteral("production"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("mvp")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: mvp"), log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("gtk"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral(""));
}

void PortalInstallScriptIntegrationTest::envOnlyProfile_isUsedWhenCliProfileMissing()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_PROFILE"), QStringLiteral("production"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: production"), log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral("slm"));
}

void PortalInstallScriptIntegrationTest::envOnlyProfile_isCaseInsensitiveWhenCliProfileMissing()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_PROFILE"), QStringLiteral("PrOdUcTiOn"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: production"), log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral("slm"));
}

void PortalInstallScriptIntegrationTest::prodAlias_isSupportedAndMapsToProduction()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("prod")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: prod"), log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral("slm"));
    // Production-specific fallback still applies.
    const QString remoteDesktop = readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.RemoteDesktop"));
    QVERIFY(!remoteDesktop.contains(QStringLiteral("slm")));
}

void PortalInstallScriptIntegrationTest::envOnlyProdAlias_isSupported()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_PROFILE"), QStringLiteral("prod"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true")}, env, &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: prod"), log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("slm"));
}

void PortalInstallScriptIntegrationTest::cliWhitespaceProfile_overridesEnvAndFails()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_PROFILE"), QStringLiteral("production"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("   ")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode != 0, log.constData());
    QVERIFY2(log.contains("[install-portald] unknown portal profile:"), log.constData());
    QVERIFY2(log.contains("use one of: mvp | beta | production | legacy"), log.constData());
}

void PortalInstallScriptIntegrationTest::dryRunOutput_reportsCanonicalInstalledPathsForCustomXdgHomes()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("cfg-root"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data-root"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("beta")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    const QString unitPath = QDir(xdgConfigHome).filePath(QStringLiteral("systemd/user/slm-portald.service"));
    const QString defaultConfPath = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal/portals.conf"));
    const QString slmConfPath = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal/slm-portals.conf"));
    const QString portalPath = QDir(xdgDataHome).filePath(QStringLiteral("xdg-desktop-portal/portals/slm.portal"));
    const QString dbusServicePath =
        QDir(xdgDataHome).filePath(QStringLiteral("dbus-1/services/org.freedesktop.impl.portal.desktop.slm.service"));

    QVERIFY2(log.contains(QStringLiteral("[install-portald] installed: %1").arg(unitPath).toUtf8()), log.constData());
    QVERIFY2(log.contains(QStringLiteral("[install-portald] installed: %1").arg(defaultConfPath).toUtf8()), log.constData());
    QVERIFY2(log.contains(QStringLiteral("[install-portald] installed: %1").arg(slmConfPath).toUtf8()), log.constData());
    QVERIFY2(log.contains(QStringLiteral("[install-portald] installed: %1").arg(portalPath).toUtf8()), log.constData());
    QVERIFY2(log.contains(QStringLiteral("[install-portald] installed: %1").arg(dbusServicePath).toUtf8()),
             log.constData());
}

void PortalInstallScriptIntegrationTest::productionNoDefaultOverwrite_updatesOnlySlmPortalsConf_andLogsRemainConsistent()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    QVERIFY(QDir().mkpath(confDir));
    QVERIFY(QDir().mkpath(xdgDataHome));

    const QString defaultConf = QDir(confDir).filePath(QStringLiteral("portals.conf"));
    {
        QFile f(defaultConf);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("[preferred]\ndefault=wlr\n");
        f.close();
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("0"));

    int exitCode = -1;
    const QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("production")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    QCOMPARE(readValue(defaultConf, QStringLiteral("default")), QStringLiteral("wlr"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    QCOMPARE(readValue(slmConf, QStringLiteral("default")), QStringLiteral("slm"));
    QCOMPARE(readValue(slmConf, QStringLiteral("org.freedesktop.impl.portal.ScreenCast")),
             QStringLiteral("slm"));

    QVERIFY2(log.contains("[install-portald] portal profile: production"), log.constData());
    QVERIFY2(log.contains("[install-portald] set default portals.conf: 0"), log.constData());
    QVERIFY2(log.contains("[install-portald] skip systemctl: 1"), log.constData());
}

void PortalInstallScriptIntegrationTest::repeatedInstall_sameProfile_isIdempotentForGeneratedArtifacts()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));

    int exitCode = -1;
    QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("beta")},
                                  env,
                                  &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    const QString confDir = QDir(xdgConfigHome).filePath(QStringLiteral("xdg-desktop-portal"));
    const QString slmConf = QDir(confDir).filePath(QStringLiteral("slm-portals.conf"));
    const QString defaultConf = QDir(confDir).filePath(QStringLiteral("portals.conf"));
    const QString portalFile =
        QDir(xdgDataHome).filePath(QStringLiteral("xdg-desktop-portal/portals/slm.portal"));
    const QString dbusFile =
        QDir(xdgDataHome).filePath(QStringLiteral("dbus-1/services/org.freedesktop.impl.portal.desktop.slm.service"));
    const QString unitFile =
        QDir(xdgConfigHome).filePath(QStringLiteral("systemd/user/slm-portald.service"));

    const QByteArray slmConfHash1 = sha256File(slmConf);
    const QByteArray defaultConfHash1 = sha256File(defaultConf);
    const QByteArray portalHash1 = sha256File(portalFile);
    const QByteArray dbusHash1 = sha256File(dbusFile);
    const QByteArray unitHash1 = sha256File(unitFile);
    QVERIFY(!slmConfHash1.isEmpty());
    QVERIFY(!defaultConfHash1.isEmpty());
    QVERIFY(!portalHash1.isEmpty());
    QVERIFY(!dbusHash1.isEmpty());
    QVERIFY(!unitHash1.isEmpty());

    log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("beta")},
                       env,
                       &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    QCOMPARE(sha256File(slmConf), slmConfHash1);
    QCOMPARE(sha256File(defaultConf), defaultConfHash1);
    QCOMPARE(sha256File(portalFile), portalHash1);
    QCOMPARE(sha256File(dbusFile), dbusHash1);
    QCOMPARE(sha256File(unitFile), unitHash1);
}

void PortalInstallScriptIntegrationTest::emptyProfile_fallsBackToMvp_andWhitespaceProfileIsRejected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));

    int exitCode = -1;
    QByteArray log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("")}, env, &exitCode);
    QVERIFY2(exitCode == 0, log.constData());
    QVERIFY2(log.contains("[install-portald] portal profile: mvp"), log.constData());

    log = runInstaller({QStringLiteral("/bin/true"), QStringLiteral("   ")}, env, &exitCode);
    QVERIFY2(exitCode != 0, log.constData());
    QVERIFY2(log.contains("[install-portald] unknown portal profile:"), log.constData());
    QVERIFY2(log.contains("use one of: mvp | beta | production | legacy"), log.constData());
}

void PortalInstallScriptIntegrationTest::customBinaryPath_isSubstitutedIntoSystemdUnitAndDbusService()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString xdgConfigHome = QDir(tmp.path()).filePath(QStringLiteral("config"));
    const QString xdgDataHome = QDir(tmp.path()).filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(xdgConfigHome));
    QVERIFY(QDir().mkpath(xdgDataHome));

    // Use existing executable as custom BIN_PATH so installer validation passes.
    const QString customBinPath = QStringLiteral("/usr/bin/env");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), xdgConfigHome);
    env.insert(QStringLiteral("XDG_DATA_HOME"), xdgDataHome);
    env.insert(QStringLiteral("SLM_PORTAL_SKIP_SYSTEMCTL"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_PORTAL_SET_DEFAULT"), QStringLiteral("1"));

    int exitCode = -1;
    const QByteArray log = runInstaller({customBinPath, QStringLiteral("beta")},
                                        env,
                                        &exitCode);
    QVERIFY2(exitCode == 0, log.constData());

    const QString unitFile =
        QDir(xdgConfigHome).filePath(QStringLiteral("systemd/user/slm-portald.service"));
    const QString dbusFile =
        QDir(xdgDataHome).filePath(QStringLiteral("dbus-1/services/org.freedesktop.impl.portal.desktop.slm.service"));

    const QByteArray unitRaw = readFile(unitFile);
    const QByteArray dbusRaw = readFile(dbusFile);
    QVERIFY2(!unitRaw.isEmpty(), qPrintable(unitFile));
    QVERIFY2(!dbusRaw.isEmpty(), qPrintable(dbusFile));

    QVERIFY2(unitRaw.contains(QByteArrayLiteral("ExecStart=") + customBinPath.toUtf8()), unitRaw.constData());
    QVERIFY2(dbusRaw.contains(QByteArrayLiteral("Exec=") + customBinPath.toUtf8()), dbusRaw.constData());
}

QTEST_GUILESS_MAIN(PortalInstallScriptIntegrationTest)
#include "portal_install_script_integration_test.moc"
