#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#include "../src/services/context/contextservice.h"
#include "../src/services/context/ruleengine.h"

namespace {
constexpr const char kContextService[] = "org.desktop.Context";
constexpr const char kContextPath[] = "/org/desktop/Context";
constexpr const char kContextIface[] = "org.desktop.Context";

constexpr const char kSettingsService[] = "org.slm.Desktop.Settings";
constexpr const char kSettingsPath[] = "/org/slm/Desktop/Settings";
constexpr const char kSettingsIface[] = "org.slm.Desktop.Settings";
}

class FakeSettingsService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Settings")

public:
    explicit FakeSettingsService(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    bool registerOnBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.registerService(QString::fromLatin1(kSettingsService))) {
            return false;
        }
        if (!bus.registerObject(QString::fromLatin1(kSettingsPath),
                                this,
                                QDBusConnection::ExportAllSlots)) {
            bus.unregisterService(QString::fromLatin1(kSettingsService));
            return false;
        }
        return true;
    }

    void unregisterFromBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.unregisterObject(QString::fromLatin1(kSettingsPath));
        bus.unregisterService(QString::fromLatin1(kSettingsService));
    }

    void setColorMode(const QString &mode)
    {
        m_colorMode = mode;
    }

    void setContextAutomation(bool autoReduceAnimation,
                              bool autoDisableBlur,
                              bool autoDisableHeavyEffects)
    {
        m_autoReduceAnimation = autoReduceAnimation;
        m_autoDisableBlur = autoDisableBlur;
        m_autoDisableHeavyEffects = autoDisableHeavyEffects;
    }

    void setContextTimePolicy(const QString &mode, int sunriseHour, int sunsetHour)
    {
        m_timeMode = mode;
        m_sunriseHour = sunriseHour;
        m_sunsetHour = sunsetHour;
    }

public slots:
    QVariantMap GetSettings() const
    {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("settings"), QVariantMap{
                 {QStringLiteral("globalAppearance"), QVariantMap{
                      {QStringLiteral("colorMode"), m_colorMode},
                  }},
                 {QStringLiteral("contextAutomation"), QVariantMap{
                      {QStringLiteral("autoReduceAnimation"), m_autoReduceAnimation},
                      {QStringLiteral("autoDisableBlur"), m_autoDisableBlur},
                      {QStringLiteral("autoDisableHeavyEffects"), m_autoDisableHeavyEffects},
                  }},
                 {QStringLiteral("contextTime"), QVariantMap{
                      {QStringLiteral("mode"), m_timeMode},
                      {QStringLiteral("sunriseHour"), m_sunriseHour},
                      {QStringLiteral("sunsetHour"), m_sunsetHour},
                  }},
             }},
        };
    }

private:
    QString m_colorMode = QStringLiteral("auto");
    bool m_autoReduceAnimation = true;
    bool m_autoDisableBlur = true;
    bool m_autoDisableHeavyEffects = true;
    QString m_timeMode = QStringLiteral("local");
    int m_sunriseHour = 6;
    int m_sunsetHour = 18;
};

class ContextServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void power_signals_emit_on_power_diff()
    {
        Slm::Context::ContextService service;
        QSignalSpy contextSpy(&service, SIGNAL(ContextChanged(QVariantMap)));
        QSignalSpy powerSpy(&service, SIGNAL(PowerChanged(QVariantMap)));
        QSignalSpy legacyPowerSpy(&service, SIGNAL(PowerStateChanged(QVariantMap)));

        const QVariantMap base{
            {QStringLiteral("time"), QVariantMap{{QStringLiteral("period"), QStringLiteral("day")}}},
            {QStringLiteral("power"), QVariantMap{
                 {QStringLiteral("mode"), QStringLiteral("ac")},
                 {QStringLiteral("level"), 95},
                 {QStringLiteral("lowPower"), false},
             }},
            {QStringLiteral("display"), QVariantMap{{QStringLiteral("fullscreen"), false}}},
            {QStringLiteral("activity"), QVariantMap{{QStringLiteral("source"), QStringLiteral("fallback")}}},
            {QStringLiteral("system"), QVariantMap{{QStringLiteral("performance"), QStringLiteral("normal")}}},
            {QStringLiteral("rules"), QVariantMap{{QStringLiteral("actions"), QVariantMap{}}}},
        };
        const QVariantMap changedPower{
            {QStringLiteral("time"), QVariantMap{{QStringLiteral("period"), QStringLiteral("day")}}},
            {QStringLiteral("power"), QVariantMap{
                 {QStringLiteral("mode"), QStringLiteral("battery")},
                 {QStringLiteral("level"), 15},
                 {QStringLiteral("lowPower"), true},
             }},
            {QStringLiteral("display"), QVariantMap{{QStringLiteral("fullscreen"), false}}},
            {QStringLiteral("activity"), QVariantMap{{QStringLiteral("source"), QStringLiteral("fallback")}}},
            {QStringLiteral("system"), QVariantMap{{QStringLiteral("performance"), QStringLiteral("normal")}}},
            {QStringLiteral("rules"), QVariantMap{{QStringLiteral("actions"), QVariantMap{}}}},
        };

        QMetaObject::invokeMethod(&service,
                                  "InjectContextForTest",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariantMap, base));
        contextSpy.clear();
        powerSpy.clear();
        legacyPowerSpy.clear();

        // Same payload should not emit semantic-change signals.
        QMetaObject::invokeMethod(&service,
                                  "InjectContextForTest",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariantMap, base));
        QCOMPARE(contextSpy.count(), 0);
        QCOMPARE(powerSpy.count(), 0);
        QCOMPARE(legacyPowerSpy.count(), 0);

        // Power diff should emit both canonical and legacy power signals.
        QMetaObject::invokeMethod(&service,
                                  "InjectContextForTest",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariantMap, changedPower));
        QCOMPARE(contextSpy.count(), 1);
        QCOMPARE(powerSpy.count(), 1);
        QCOMPARE(legacyPowerSpy.count(), 1);
    }

    void rule_engine_heavy_effects_policy()
    {
        Slm::Context::RuleEngine engine;

        const QVariantMap settingsAuto = {
            {QStringLiteral("globalAppearance"), QVariantMap{
                 {QStringLiteral("colorMode"), QStringLiteral("auto")},
             }},
        };

        auto buildContext = [](bool lowPower, const QString &performance) {
            return QVariantMap{
                {QStringLiteral("time"), QVariantMap{
                     {QStringLiteral("period"), QStringLiteral("day")},
                 }},
                {QStringLiteral("power"), QVariantMap{
                     {QStringLiteral("lowPower"), lowPower},
                 }},
                {QStringLiteral("display"), QVariantMap{
                     {QStringLiteral("fullscreen"), false},
                 }},
                {QStringLiteral("system"), QVariantMap{
                     {QStringLiteral("performance"), performance},
                 }},
            };
        };

        const QVariantMap normal = engine.evaluate(buildContext(false, QStringLiteral("normal")),
                                                   settingsAuto);
        const QVariantMap normalActions = normal.value(QStringLiteral("actions")).toMap();
        QVERIFY(!normalActions.value(QStringLiteral("ui.disableHeavyEffects")).toBool());

        const QVariantMap lowPowerOnly = engine.evaluate(buildContext(true, QStringLiteral("normal")),
                                                         settingsAuto);
        const QVariantMap lowPowerActions = lowPowerOnly.value(QStringLiteral("actions")).toMap();
        QVERIFY(lowPowerActions.value(QStringLiteral("ui.disableHeavyEffects")).toBool());

        const QVariantMap lowPerfOnly = engine.evaluate(buildContext(false, QStringLiteral("low")),
                                                        settingsAuto);
        const QVariantMap lowPerfActions = lowPerfOnly.value(QStringLiteral("actions")).toMap();
        QVERIFY(lowPerfActions.value(QStringLiteral("ui.disableHeavyEffects")).toBool());

        const QVariantMap bothLow = engine.evaluate(buildContext(true, QStringLiteral("low")),
                                                    settingsAuto);
        const QVariantMap bothActions = bothLow.value(QStringLiteral("actions")).toMap();
        QVERIFY(bothActions.value(QStringLiteral("ui.disableHeavyEffects")).toBool());
    }

    void contract_ping_subscribe_getContext()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface is unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kContextService)).value()) {
            QSKIP("org.desktop.Context already active; skip contract test");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSettingsService)).value()) {
            QSKIP("org.slm.Desktop.Settings already active; skip fake-settings test");
        }

        FakeSettingsService fakeSettings;
        fakeSettings.setColorMode(QStringLiteral("auto"));
        QVERIFY(fakeSettings.registerOnBus());

        Slm::Context::ContextService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface contextIface(QString::fromLatin1(kContextService),
                                    QString::fromLatin1(kContextPath),
                                    QString::fromLatin1(kContextIface),
                                    bus);
        QVERIFY(contextIface.isValid());

        QDBusReply<QVariantMap> ping = contextIface.call(QStringLiteral("Ping"));
        QVERIFY(ping.isValid());
        QVERIFY(ping.value().value(QStringLiteral("ok")).toBool());
        QCOMPARE(ping.value().value(QStringLiteral("service")).toString(), QString::fromLatin1(kContextService));

        QDBusReply<QVariantMap> subscribe = contextIface.call(QStringLiteral("Subscribe"));
        QVERIFY(subscribe.isValid());
        QVERIFY(subscribe.value().value(QStringLiteral("ok")).toBool());
        const QStringList signalNames = subscribe.value().value(QStringLiteral("signals")).toStringList();
        QVERIFY(signalNames.contains(QStringLiteral("ContextChanged")));
        QVERIFY(signalNames.contains(QStringLiteral("PowerChanged")));
        QVERIFY(signalNames.contains(QStringLiteral("PowerStateChanged")));

        QDBusReply<QVariantMap> get = contextIface.call(QStringLiteral("GetContext"));
        QVERIFY(get.isValid());
        QVERIFY(get.value().value(QStringLiteral("ok")).toBool());
        const QVariantMap context = get.value().value(QStringLiteral("context")).toMap();
        QVERIFY(context.contains(QStringLiteral("time")));
        QVERIFY(context.contains(QStringLiteral("power")));
        QVERIFY(context.contains(QStringLiteral("display")));
        QVERIFY(context.contains(QStringLiteral("system")));
        QVERIFY(context.contains(QStringLiteral("network")));
        QVERIFY(context.contains(QStringLiteral("attention")));
        QVERIFY(context.contains(QStringLiteral("activity")));
        QVERIFY(context.contains(QStringLiteral("rules")));
        const QVariantMap display = context.value(QStringLiteral("display")).toMap();
        const QString fullscreenSource = display.value(QStringLiteral("fullscreenSource")).toString();
        QVERIFY(fullscreenSource == QLatin1String("kwin") || fullscreenSource == QLatin1String("fallback"));
        const QVariantMap system = context.value(QStringLiteral("system")).toMap();
        const QString performance = system.value(QStringLiteral("performance")).toString();
        QVERIFY(performance == QLatin1String("high")
                || performance == QLatin1String("normal")
                || performance == QLatin1String("low"));
        const QString cpuSource = system.value(QStringLiteral("cpuSource")).toString();
        const QString memorySource = system.value(QStringLiteral("memorySource")).toString();
        QVERIFY(cpuSource == QLatin1String("loadavg") || cpuSource == QLatin1String("fallback"));
        QVERIFY(memorySource == QLatin1String("meminfo") || memorySource == QLatin1String("fallback"));

        const QVariantMap network = context.value(QStringLiteral("network")).toMap();
        const QString networkState = network.value(QStringLiteral("state")).toString();
        const QString networkQuality = network.value(QStringLiteral("quality")).toString();
        const QString networkSource = network.value(QStringLiteral("source")).toString();
        QVERIFY(networkState == QLatin1String("online") || networkState == QLatin1String("offline"));
        QVERIFY(networkQuality == QLatin1String("normal")
                || networkQuality == QLatin1String("limited")
                || networkQuality == QLatin1String("offline"));
        QVERIFY(networkSource == QLatin1String("networkmanager")
                || networkSource == QLatin1String("fallback"));

        const QVariantMap attention = context.value(QStringLiteral("attention")).toMap();
        QVERIFY(attention.contains(QStringLiteral("idle")));
        QVERIFY(attention.contains(QStringLiteral("idleTimeSec")));
        QVERIFY(attention.contains(QStringLiteral("fullscreenSuppression")));
        const QString attentionSource = attention.value(QStringLiteral("source")).toString();
        QVERIFY(attentionSource == QLatin1String("screensaver")
                || attentionSource == QLatin1String("fallback"));

        const QVariantMap activity = context.value(QStringLiteral("activity")).toMap();
        const QString activitySource = activity.value(QStringLiteral("source")).toString();
        QVERIFY(activitySource == QLatin1String("kwin")
                || activitySource == QLatin1String("fallback"));

        const QVariantMap rules = context.value(QStringLiteral("rules")).toMap();
        const QVariantMap actions = rules.value(QStringLiteral("actions")).toMap();
        const QString suggested = actions.value(QStringLiteral("appearance.modeSuggestion")).toString();
        QVERIFY(suggested == QLatin1String("dark") || suggested == QLatin1String("light"));

        const QVariantMap time = context.value(QStringLiteral("time")).toMap();
        QVERIFY(time.contains(QStringLiteral("source")));
        QCOMPARE(time.value(QStringLiteral("mode")).toString(), QStringLiteral("local"));
        QCOMPARE(time.value(QStringLiteral("sunriseHour")).toInt(), 6);
        QCOMPARE(time.value(QStringLiteral("sunsetHour")).toInt(), 18);

        fakeSettings.unregisterFromBus();
    }

    void time_context_uses_sun_policy_when_enabled()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface is unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kContextService)).value()) {
            QSKIP("org.desktop.Context already active; skip contract test");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSettingsService)).value()) {
            QSKIP("org.slm.Desktop.Settings already active; skip fake-settings test");
        }

        FakeSettingsService fakeSettings;
        fakeSettings.setColorMode(QStringLiteral("auto"));
        fakeSettings.setContextTimePolicy(QStringLiteral("sun"), 5, 19);
        QVERIFY(fakeSettings.registerOnBus());

        Slm::Context::ContextService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface contextIface(QString::fromLatin1(kContextService),
                                    QString::fromLatin1(kContextPath),
                                    QString::fromLatin1(kContextIface),
                                    bus);
        QVERIFY(contextIface.isValid());

        QDBusReply<QVariantMap> get = contextIface.call(QStringLiteral("GetContext"));
        QVERIFY(get.isValid());
        QVERIFY(get.value().value(QStringLiteral("ok")).toBool());

        const QVariantMap context = get.value().value(QStringLiteral("context")).toMap();
        const QVariantMap time = context.value(QStringLiteral("time")).toMap();
        QCOMPARE(time.value(QStringLiteral("mode")).toString(), QStringLiteral("sun"));
        QCOMPARE(time.value(QStringLiteral("sunriseHour")).toInt(), 5);
        QCOMPARE(time.value(QStringLiteral("sunsetHour")).toInt(), 19);
        QCOMPARE(time.value(QStringLiteral("source")).toString(), QStringLiteral("sun-hours"));

        fakeSettings.unregisterFromBus();
    }

    void manual_mode_blocks_time_override()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface is unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kContextService)).value()) {
            QSKIP("org.desktop.Context already active; skip contract test");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSettingsService)).value()) {
            QSKIP("org.slm.Desktop.Settings already active; skip fake-settings test");
        }

        FakeSettingsService fakeSettings;
        fakeSettings.setColorMode(QStringLiteral("light"));
        QVERIFY(fakeSettings.registerOnBus());

        Slm::Context::ContextService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface contextIface(QString::fromLatin1(kContextService),
                                    QString::fromLatin1(kContextPath),
                                    QString::fromLatin1(kContextIface),
                                    bus);
        QVERIFY(contextIface.isValid());

        QDBusReply<QVariantMap> get = contextIface.call(QStringLiteral("GetContext"));
        QVERIFY(get.isValid());
        QVERIFY(get.value().value(QStringLiteral("ok")).toBool());

        const QVariantMap context = get.value().value(QStringLiteral("context")).toMap();
        const QVariantMap rules = context.value(QStringLiteral("rules")).toMap();
        const QVariantMap guard = rules.value(QStringLiteral("guard")).toMap();
        const QVariantMap actions = rules.value(QStringLiteral("actions")).toMap();

        QCOMPARE(guard.value(QStringLiteral("userMode")).toString(), QStringLiteral("light"));
        QVERIFY(!guard.value(QStringLiteral("autoThemeEnabled")).toBool());
        QCOMPARE(actions.value(QStringLiteral("appearance.modeSuggestion")).toString(), QStringLiteral("light"));
        QCOMPARE(actions.value(QStringLiteral("appearance.modeReason")).toString(),
                 QStringLiteral("manual-user-setting"));

        fakeSettings.unregisterFromBus();
    }

    void context_automation_gate_blocks_actions()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface is unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kContextService)).value()) {
            QSKIP("org.desktop.Context already active; skip contract test");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSettingsService)).value()) {
            QSKIP("org.slm.Desktop.Settings already active; skip fake-settings test");
        }

        FakeSettingsService fakeSettings;
        fakeSettings.setColorMode(QStringLiteral("auto"));
        fakeSettings.setContextAutomation(false, false, false);
        QVERIFY(fakeSettings.registerOnBus());

        Slm::Context::ContextService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface contextIface(QString::fromLatin1(kContextService),
                                    QString::fromLatin1(kContextPath),
                                    QString::fromLatin1(kContextIface),
                                    bus);
        QVERIFY(contextIface.isValid());

        QDBusReply<QVariantMap> get = contextIface.call(QStringLiteral("GetContext"));
        QVERIFY(get.isValid());
        QVERIFY(get.value().value(QStringLiteral("ok")).toBool());

        const QVariantMap context = get.value().value(QStringLiteral("context")).toMap();
        const QVariantMap rules = context.value(QStringLiteral("rules")).toMap();
        const QVariantMap actions = rules.value(QStringLiteral("actions")).toMap();
        const QVariantMap guard = rules.value(QStringLiteral("guard")).toMap();

        QVERIFY(!guard.value(QStringLiteral("context.autoReduceAnimation")).toBool());
        QVERIFY(!guard.value(QStringLiteral("context.autoDisableBlur")).toBool());
        QVERIFY(!guard.value(QStringLiteral("context.autoDisableHeavyEffects")).toBool());
        QVERIFY(!actions.value(QStringLiteral("ui.reduceAnimation")).toBool());
        QVERIFY(!actions.value(QStringLiteral("ui.disableBlur")).toBool());
        QVERIFY(!actions.value(QStringLiteral("ui.disableHeavyEffects")).toBool());

        fakeSettings.unregisterFromBus();
    }

    void context_automation_guard_reflects_enabled_state()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        const QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface is unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kContextService)).value()) {
            QSKIP("org.desktop.Context already active; skip contract test");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kSettingsService)).value()) {
            QSKIP("org.slm.Desktop.Settings already active; skip fake-settings test");
        }

        FakeSettingsService fakeSettings;
        fakeSettings.setColorMode(QStringLiteral("auto"));
        fakeSettings.setContextAutomation(true, true, true);
        QVERIFY(fakeSettings.registerOnBus());

        Slm::Context::ContextService service;
        QString error;
        QVERIFY2(service.start(&error), qPrintable(error));
        QVERIFY(service.serviceRegistered());

        QDBusInterface contextIface(QString::fromLatin1(kContextService),
                                    QString::fromLatin1(kContextPath),
                                    QString::fromLatin1(kContextIface),
                                    bus);
        QVERIFY(contextIface.isValid());

        QDBusReply<QVariantMap> get = contextIface.call(QStringLiteral("GetContext"));
        QVERIFY(get.isValid());
        QVERIFY(get.value().value(QStringLiteral("ok")).toBool());

        const QVariantMap context = get.value().value(QStringLiteral("context")).toMap();
        const QVariantMap rules = context.value(QStringLiteral("rules")).toMap();
        const QVariantMap guard = rules.value(QStringLiteral("guard")).toMap();

        QVERIFY(guard.value(QStringLiteral("context.autoReduceAnimation")).toBool());
        QVERIFY(guard.value(QStringLiteral("context.autoDisableBlur")).toBool());
        QVERIFY(guard.value(QStringLiteral("context.autoDisableHeavyEffects")).toBool());

        fakeSettings.unregisterFromBus();
    }
};

QTEST_MAIN(ContextServiceDbusTest)
#include "contextservice_dbus_test.moc"
