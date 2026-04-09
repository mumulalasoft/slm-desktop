#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
    void init()
    {
        const QString testName = QString::fromLatin1(QTest::currentTestFunction());
        m_storePath = QDir::tempPath()
                + QStringLiteral("/slm-settingsd-test-")
                + QString::number(QCoreApplication::applicationPid())
                + QStringLiteral("-")
                + testName
                + QStringLiteral(".json");
        QFile::remove(m_storePath);
        QString lastGood = m_storePath;
        lastGood.chop(5);
        lastGood += QStringLiteral(".last-good.json");
        QFile::remove(lastGood);
        qputenv("SLM_SETTINGSD_STORE_PATH", m_storePath.toUtf8());
    }

    void cleanup()
    {
        QFile::remove(m_storePath);
        QString lastGood = m_storePath;
        if (lastGood.endsWith(QStringLiteral(".json"))) {
            lastGood.chop(5);
            lastGood += QStringLiteral(".last-good.json");
            QFile::remove(lastGood);
        }
    }

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
        const QVariantMap localSettings = service.GetSettings().value(QStringLiteral("settings")).toMap();
        QCOMPARE(localSettings.value(QStringLiteral("schemaVersion")).toInt(), 1);
        const QVariantMap settings = localSettings;
        QVERIFY(settings.contains(QStringLiteral("globalAppearance")));
        QVERIFY(settings.contains(QStringLiteral("contextAutomation")));
        QVERIFY(settings.contains(QStringLiteral("contextTime")));
        QVERIFY(settings.contains(QStringLiteral("dock")));
        QVERIFY(settings.contains(QStringLiteral("print")));
        QVERIFY(settings.contains(QStringLiteral("windowing")));
        QVERIFY(settings.contains(QStringLiteral("shortcuts")));
        QVERIFY(settings.contains(QStringLiteral("fonts")));
        QVERIFY(settings.contains(QStringLiteral("wallpaper")));
        QVERIFY(settings.contains(QStringLiteral("firewall")));
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
        const QVariantMap dock = settings.value(QStringLiteral("dock")).toMap();
        const QString motionPreset = dock.value(QStringLiteral("motionPreset")).toString();
        QVERIFY(motionPreset == QLatin1String("subtle") || motionPreset == QLatin1String("macos-lively"));
        const QString iconSize = dock.value(QStringLiteral("iconSize")).toString();
        QVERIFY(iconSize == QLatin1String("small")
                || iconSize == QLatin1String("medium")
                || iconSize == QLatin1String("large"));
        QVERIFY(dock.contains(QStringLiteral("magnificationEnabled")));
        QVERIFY(dock.value(QStringLiteral("dragThresholdMouse")).toInt() >= 2);
        QVERIFY(dock.value(QStringLiteral("dragThresholdMouse")).toInt() <= 24);
        QVERIFY(dock.value(QStringLiteral("dragThresholdTouchpad")).toInt() >= 2);
        QVERIFY(dock.value(QStringLiteral("dragThresholdTouchpad")).toInt() <= 24);
        const QVariantMap print = settings.value(QStringLiteral("print")).toMap();
        QVERIFY(print.contains(QStringLiteral("pdfFallbackPrinterId")));
        const QVariantMap windowing = settings.value(QStringLiteral("windowing")).toMap();
        QVERIFY(windowing.contains(QStringLiteral("animationEnabled")));
        QVERIFY(windowing.contains(QStringLiteral("controlsSide")));
        QVERIFY(windowing.contains(QStringLiteral("bindClose")));
        QVERIFY(windowing.contains(QStringLiteral("bindWorkspace")));
        const QVariantMap shortcuts = settings.value(QStringLiteral("shortcuts")).toMap();
        QVERIFY(shortcuts.contains(QStringLiteral("workspaceOverview")));
        const QVariantMap fonts = settings.value(QStringLiteral("fonts")).toMap();
        QVERIFY(fonts.contains(QStringLiteral("defaultFont")));
        QVERIFY(fonts.contains(QStringLiteral("documentFont")));
        QVERIFY(fonts.contains(QStringLiteral("monospaceFont")));
        QVERIFY(fonts.contains(QStringLiteral("titlebarFont")));
        const QVariantMap wallpaper = settings.value(QStringLiteral("wallpaper")).toMap();
        QVERIFY(wallpaper.contains(QStringLiteral("uri")));
        const QVariantMap firewall = settings.value(QStringLiteral("firewall")).toMap();
        QVERIFY(firewall.contains(QStringLiteral("enabled")));
        QVERIFY(firewall.contains(QStringLiteral("mode")));
        QVERIFY(firewall.contains(QStringLiteral("defaultIncomingPolicy")));
        QVERIFY(firewall.contains(QStringLiteral("defaultOutgoingPolicy")));
        QVERIFY(firewall.contains(QStringLiteral("networkProfiles")));
        QVERIFY(firewall.contains(QStringLiteral("rules")));
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
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("light")))));
        QVERIFY(setReply.isValid());
        QVERIFY(setReply.value().value(QStringLiteral("ok")).toBool());
        QTRY_VERIFY(appearanceSpy.count() >= 1);

        QDBusReply<QVariantMap> invalidReply =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("globalAppearance.colorMode"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("neon")))));
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
                       QVariant::fromValue(QDBusVariant(QVariant(false))));
        QVERIFY(setOk.isValid());
        QVERIFY(setOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextAutomation.autoDisableHeavyEffects"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("false")))));
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
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("sun")))));
        QVERIFY(setModeOk.isValid());
        QVERIFY(setModeOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setSunriseOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.sunriseHour"),
                       QVariant::fromValue(QDBusVariant(QVariant(5))));
        QVERIFY(setSunriseOk.isValid());
        QVERIFY(setSunriseOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setSunsetOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.sunsetHour"),
                       QVariant::fromValue(QDBusVariant(QVariant(19))));
        QVERIFY(setSunsetOk.isValid());
        QVERIFY(setSunsetOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setModeInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.mode"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("astronomical")))));
        QVERIFY(setModeInvalid.isValid());
        QVERIFY(!setModeInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setHourInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("contextTime.sunriseHour"),
                       QVariant::fromValue(QDBusVariant(QVariant(24))));
        QVERIFY(setHourInvalid.isValid());
        QVERIFY(!setHourInvalid.value().value(QStringLiteral("ok")).toBool());
    }

    void dock_settings_validation()
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

        QDBusReply<QVariantMap> setMotionOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("dock.motionPreset"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("macos-lively")))));
        QVERIFY(setMotionOk.isValid());
        QVERIFY(setMotionOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setMotionInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("dock.motionPreset"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("neon")))));
        QVERIFY(setMotionInvalid.isValid());
        QVERIFY(!setMotionInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setThresholdInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("dock.dragThresholdMouse"),
                       QVariant::fromValue(QDBusVariant(QVariant(40))));
        QVERIFY(setThresholdInvalid.isValid());
        QVERIFY(!setThresholdInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setIconSizeOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("dock.iconSize"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("large")))));
        QVERIFY(setIconSizeOk.isValid());
        QVERIFY(setIconSizeOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setIconSizeInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("dock.iconSize"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("xlarge")))));
        QVERIFY(setIconSizeInvalid.isValid());
        QVERIFY(!setIconSizeInvalid.value().value(QStringLiteral("ok")).toBool());
    }

    void print_and_shortcut_settings_validation()
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

        QDBusReply<QVariantMap> setPrintOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("print.pdfFallbackPrinterId"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("office-printer")))));
        QVERIFY(setPrintOk.isValid());
        QVERIFY(setPrintOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setShortcutOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("windowing.bindClose"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("Meta+Q")))));
        QVERIFY(setShortcutOk.isValid());
        QVERIFY(setShortcutOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setAnimationOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("windowing.animationEnabled"),
                       QVariant::fromValue(QDBusVariant(QVariant(false))));
        QVERIFY(setAnimationOk.isValid());
        QVERIFY(setAnimationOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setAnimationInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("windowing.animationEnabled"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("false")))));
        QVERIFY(setAnimationInvalid.isValid());
        QVERIFY(!setAnimationInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setControlsSideOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("windowing.controlsSide"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("left")))));
        QVERIFY(setControlsSideOk.isValid());
        QVERIFY(setControlsSideOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setControlsSideInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("windowing.controlsSide"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("center")))));
        QVERIFY(setControlsSideInvalid.isValid());
        QVERIFY(!setControlsSideInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setShortcutInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("shortcuts.workspaceOverview"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("   ")))));
        QVERIFY(setShortcutInvalid.isValid());
        QVERIFY(!setShortcutInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setFontOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("fonts.defaultFont"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("Noto Sans,Regular,11")))));
        QVERIFY(setFontOk.isValid());
        QVERIFY(setFontOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setFontInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("fonts.defaultFont"),
                       QVariant::fromValue(QDBusVariant(QVariant(QVariantMap{
                           {QStringLiteral("family"), QStringLiteral("Noto Sans")}
                       }))));
        QVERIFY(setFontInvalid.isValid());
        QVERIFY(!setFontInvalid.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setWallpaperOk =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("wallpaper.uri"),
                       QVariant::fromValue(QDBusVariant(QVariant(QStringLiteral("file:///tmp/wall.jpg")))));
        QVERIFY(setWallpaperOk.isValid());
        QVERIFY(setWallpaperOk.value().value(QStringLiteral("ok")).toBool());

        QDBusReply<QVariantMap> setWallpaperInvalid =
            iface.call(QStringLiteral("SetSetting"),
                       QStringLiteral("wallpaper.uri"),
                       QVariant::fromValue(QDBusVariant(QVariant(QVariantMap{
                           {QStringLiteral("uri"), QStringLiteral("file:///tmp/wall.jpg")}
                       }))));
        QVERIFY(setWallpaperInvalid.isValid());
        QVERIFY(!setWallpaperInvalid.value().value(QStringLiteral("ok")).toBool());
    }

private:
    QString m_storePath;
};

QTEST_MAIN(SettingsServiceDbusTest)
#include "settingsservice_dbus_test.moc"
