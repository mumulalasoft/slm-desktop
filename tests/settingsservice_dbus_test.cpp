#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSignalSpy>

#include "../src/services/settingsd/settingsservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Settings";
constexpr const char kPath[] = "/org/slm/Desktop/Settings";
constexpr const char kIface[] = "org.slm.Desktop.Settings";
}

class SettingsServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void ping_and_get_settings_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        SettingsService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> ping = iface.call(QStringLiteral("Ping"));
        QVERIFY(ping.isValid());
        QVERIFY(ping.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(ping.value().value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));

        QDBusReply<QVariantMap> get = iface.call(QStringLiteral("GetSettings"));
        QVERIFY(get.isValid());
        QVERIFY(get.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap settings = get.value().value(QStringLiteral("settings")).toMap();
        QCOMPARE(settings.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QVERIFY(settings.contains(QStringLiteral("globalAppearance")));
        QVERIFY(settings.contains(QStringLiteral("contextAutomation")));
        QVERIFY(settings.contains(QStringLiteral("contextTime")));
        const QVariantMap contextAutomation = settings.value(QStringLiteral("contextAutomation")).toMap();
        QVERIFY(contextAutomation.value(QStringLiteral("autoReduceAnimation"), true).toBool());
        QVERIFY(contextAutomation.value(QStringLiteral("autoDisableBlur"), true).toBool());
        QVERIFY(contextAutomation.value(QStringLiteral("autoDisableHeavyEffects"), true).toBool());
        const QVariantMap contextTime = settings.value(QStringLiteral("contextTime")).toMap();
        const QString mode = contextTime.value(QStringLiteral("mode")).toString();
        QVERIFY(mode == QLatin1String("local") || mode == QLatin1String("sun"));
        const int sunriseHour = contextTime.value(QStringLiteral("sunriseHour")).toInt();
        const int sunsetHour = contextTime.value(QStringLiteral("sunsetHour")).toInt();
        QVERIFY(sunriseHour >= 0 && sunriseHour <= 23);
        QVERIFY(sunsetHour >= 0 && sunsetHour <= 23);
    }

    void set_setting_emits_semantic_signal()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        SettingsService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QSignalSpy appearanceSpy(&service, SIGNAL(AppearanceModeChanged(QString)));
        QVERIFY(appearanceSpy.isValid());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> setReply =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("globalAppearance.colorMode"),
                       QVariant(QStringLiteral("light")));
        QVERIFY(setReply.isValid());
        QVERIFY(setReply.value().value(QStringLiteral("ok")).toBool());
        QTRY_VERIFY(appearanceSpy.count() >= 1);

        QDBusReply<QVariantMap> invalidReply =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("globalAppearance.colorMode"),
                       QVariant(QStringLiteral("neon")));
        QVERIFY(invalidReply.isValid());
        QVERIFY(!invalidReply.value().value(QStringLiteral("ok")).toBool());
    }

    void resolve_theme_for_app_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        SettingsService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QVariantMap app{
            {QStringLiteral("appId"), QStringLiteral("org.kde.okular")},
            {QStringLiteral("categories"), QStringList{QStringLiteral("KDE"), QStringLiteral("Office")}},
            {QStringLiteral("desktopKeys"), QStringList{QStringLiteral("X-KDE-PluginInfo-Name")}},
            {QStringLiteral("executable"), QStringLiteral("/usr/bin/okular")},
        };
        QDBusReply<QVariantMap> classify = iface.call(QStringLiteral("ClassifyApp"), app);
        QVERIFY(classify.isValid());
        QVERIFY(classify.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(classify.value().value(QStringLiteral("category")).toString(), QStringLiteral("kde"));

        QVariantMap options{
            {QStringLiteral("qtKdeCompatible"), true},
        };
        QDBusReply<QVariantMap> resolved = iface.call(QStringLiteral("ResolveThemeForApp"), app, options);
        QVERIFY(resolved.isValid());
        QVERIFY(resolved.value().value(QStringLiteral("ok")).toBool());
        QVERIFY(!resolved.value().value(QStringLiteral("themeSource")).toString().isEmpty());
        QVERIFY(!resolved.value().value(QStringLiteral("iconSource")).toString().isEmpty());

        QVariantMap overridePatch{
            {QStringLiteral("perAppOverride.rule1.matchAppId"), QStringLiteral("org.kde.okular")},
            {QStringLiteral("perAppOverride.rule1.category"), QStringLiteral("gtk")},
        };
        QDBusReply<QVariantMap> patchReply = iface.call(QStringLiteral("SetSettingsPatch"), overridePatch);
        QVERIFY(patchReply.isValid());
        QVERIFY(patchReply.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> classifyAfterOverride = iface.call(QStringLiteral("ClassifyApp"), app);
        QVERIFY(classifyAfterOverride.isValid());
        QVERIFY(classifyAfterOverride.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(classifyAfterOverride.value().value(QStringLiteral("category")).toString(),
                 QStringLiteral("gtk"));
    }

    void context_automation_flags_validation()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        SettingsService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> setOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextAutomation.autoDisableHeavyEffects"),
                       QVariant(false));
        QVERIFY(setOk.isValid());
        QVERIFY(setOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextAutomation.autoDisableHeavyEffects"),
                       QVariant(QStringLiteral("false")));
        QVERIFY(setInvalid.isValid());
        QVERIFY(!setInvalid.value().value(QStringLiteral("ok")).toBool());
    }

    void context_time_policy_validation()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        SettingsService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> setModeOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.mode"),
                       QVariant(QStringLiteral("sun")));
        QVERIFY(setModeOk.isValid());
        QVERIFY(setModeOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setSunriseOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.sunriseHour"),
                       QVariant(5));
        QVERIFY(setSunriseOk.isValid());
        QVERIFY(setSunriseOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setSunsetOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.sunsetHour"),
                       QVariant(19));
        QVERIFY(setSunsetOk.isValid());
        QVERIFY(setSunsetOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setModeInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.mode"),
                       QVariant(QStringLiteral("astronomical")));
        QVERIFY(setModeInvalid.isValid());
        QVERIFY(!setModeInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setHourInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.sunriseHour"),
                       QVariant(24));
        QVERIFY(setHourInvalid.isValid());
        QVERIFY(!setHourInvalid.value().value(QStringLiteral("ok")).toBool());
    }
};

QTEST_MAIN(SettingsServiceDbusTest)
#include "settingsservice_dbus_test.moc"
