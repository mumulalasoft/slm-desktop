#include <QtTest/QtTest>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "../portalmethodnames.h"
#include "../src/daemon/portald/portalmanager.h"

class PortalManagerOpenUriContractTest : public QObject
{
    Q_OBJECT

private slots:
    void openUri_dryRun_validDefaultScheme_returnsOk()
    {
        qunsetenv("SLM_PORTAL_OPENURI_ALLOWED_SCHEMES");
        PortalManager manager;

        const QString uri = QStringLiteral("https://example.com/docs?id=42");
        const QVariantMap options{
            {QStringLiteral("activate"), true},
            {QStringLiteral("source"), QStringLiteral("topbar")},
            {QStringLiteral("dryRun"), true},
        };

        const QVariantMap out = manager.OpenURI(uri, options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("uri")).toString(), uri);
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
        QCOMPARE(out.value(QStringLiteral("dryRun")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("launched")).toBool(), false);
    }

    void openUri_empty_returnsInvalidArgument()
    {
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("activate"), true},
        };

        const QVariantMap out = manager.OpenURI(QStringLiteral("   "), options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
        QCOMPARE(out.value(QStringLiteral("uri")).toString(), QStringLiteral("   "));
        QCOMPARE(out.value(QStringLiteral("options")).toMap(), options);
    }

    void openUri_disallowedScheme_returnsInvalidArgument()
    {
        qunsetenv("SLM_PORTAL_OPENURI_ALLOWED_SCHEMES");
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("dryRun"), true},
        };

        const QVariantMap out = manager.OpenURI(QStringLiteral("javascript:alert(1)"), options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(out.value(QStringLiteral("error")).toString(), QStringLiteral("invalid-argument"));
    }

    void openUri_allowedSchemesOption_overridesDefault()
    {
        qunsetenv("SLM_PORTAL_OPENURI_ALLOWED_SCHEMES");
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("dryRun"), true},
            {QStringLiteral("allowedSchemes"), QStringList{QStringLiteral("custom")}},
        };

        const QVariantMap out = manager.OpenURI(QStringLiteral("custom://example/resource"), options);

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("dryRun")).toBool(), true);
    }

    void openUri_allowedSchemesEnv_applies()
    {
        qputenv("SLM_PORTAL_OPENURI_ALLOWED_SCHEMES", QByteArray("custom,https"));
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("dryRun"), true},
        };

        const QVariantMap out = manager.OpenURI(QStringLiteral("custom://host/path"), options);
        qunsetenv("SLM_PORTAL_OPENURI_ALLOWED_SCHEMES");

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("dryRun")).toBool(), true);
    }

    void openUri_allowedSchemesConfig_applies()
    {
        qunsetenv("SLM_PORTAL_OPENURI_ALLOWED_SCHEMES");
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString slmDir = QDir(tempDir.path()).filePath(QStringLiteral("slm"));
        QVERIFY(QDir().mkpath(slmDir));
        const QString confPath = QDir(slmDir).filePath(QStringLiteral("portald.conf"));
        QFile conf(confPath);
        QVERIFY(conf.open(QIODevice::WriteOnly | QIODevice::Text));
        conf.write("openuri_allowed_schemes=custom,https\n");
        conf.close();

        qputenv("XDG_CONFIG_HOME", tempDir.path().toUtf8());
        PortalManager manager;
        const QVariantMap options{
            {QStringLiteral("dryRun"), true},
        };

        const QVariantMap out = manager.OpenURI(QStringLiteral("custom://from-config"), options);
        qunsetenv("XDG_CONFIG_HOME");

        QCOMPARE(out.value(QStringLiteral("method")).toString(),
                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
        QCOMPARE(out.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("dryRun")).toBool(), true);
    }
};

QTEST_MAIN(PortalManagerOpenUriContractTest)
#include "portalmanager_openuri_contract_test.moc"
