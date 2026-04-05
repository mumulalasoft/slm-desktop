#include <QtTest/QtTest>

#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "../src/apps/settings/desktopsettingsclient.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.Settings";
constexpr const char kPath[] = "/org/slm/Desktop/Settings";
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
        if (!bus.registerService(QString::fromLatin1(kService))) {
            return false;
        }
        if (!bus.registerObject(QString::fromLatin1(kPath),
                                this,
                                QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
            bus.unregisterService(QString::fromLatin1(kService));
            return false;
        }
        return true;
    }

    void unregisterFromBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.unregisterObject(QString::fromLatin1(kPath));
        bus.unregisterService(QString::fromLatin1(kService));
    }

public slots:
    QVariantMap GetSettings() const
    {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("settings"), QVariantMap{
                 {QStringLiteral("globalAppearance"), QVariantMap{
                      {QStringLiteral("colorMode"), QStringLiteral("dark")},
                      {QStringLiteral("accentColor"), QStringLiteral("#4aa3ff")},
                      {QStringLiteral("uiScale"), 1.0},
                  }},
                 {QStringLiteral("gtkThemeRouting"), QVariantMap{}},
                 {QStringLiteral("kdeThemeRouting"), QVariantMap{}},
                 {QStringLiteral("appThemePolicy"), QVariantMap{}},
                 {QStringLiteral("fallbackPolicy"), QVariantMap{}},
                 {QStringLiteral("contextAutomation"), QVariantMap{
                      {QStringLiteral("autoReduceAnimation"), true},
                      {QStringLiteral("autoDisableBlur"), true},
                      {QStringLiteral("autoDisableHeavyEffects"), true},
                  }},
                 {QStringLiteral("contextTime"), QVariantMap{
                      {QStringLiteral("mode"), m_mode},
                      {QStringLiteral("sunriseHour"), m_sunrise},
                      {QStringLiteral("sunsetHour"), m_sunset},
                  }},
             }},
        };
    }

    QVariantMap SetSetting(const QString &path, const QVariant &value)
    {
        if (path == QLatin1String("contextTime.mode")) {
            const QString mode = value.toString().trimmed().toLower();
            if (mode != QLatin1String("local") && mode != QLatin1String("sun")) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-mode")},
                };
            }
            m_mode = mode;
            emit SettingChanged(path, m_mode);
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("contextTime.sunriseHour")) {
            m_sunrise = qBound(0, value.toInt(), 23);
            emit SettingChanged(path, m_sunrise);
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("contextTime.sunsetHour")) {
            m_sunset = qBound(0, value.toInt(), 23);
            emit SettingChanged(path, m_sunset);
            return {{QStringLiteral("ok"), true}};
        }
        // Accept unrelated settings to keep client stable in test.
        emit SettingChanged(path, value);
        return {{QStringLiteral("ok"), true}};
    }

signals:
    void SettingChanged(const QString &path, const QVariant &value);
    void AppearanceModeChanged(const QString &mode);

private:
    QString m_mode = QStringLiteral("local");
    int m_sunrise = 6;
    int m_sunset = 18;
};

class DesktopSettingsClientTest : public QObject
{
    Q_OBJECT

private slots:
    void context_time_load_set_and_live_update()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }
        const QDBusConnectionInterface *iface = bus.interface();
        if (!iface) {
            QSKIP("session bus interface is unavailable");
        }
        if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
            QSKIP("org.slm.Desktop.Settings already active; skip fake-settings test");
        }

        FakeSettingsService fake;
        QVERIFY(fake.registerOnBus());

        DesktopSettingsClient client(nullptr);
        QTRY_VERIFY(client.available());
        QTRY_COMPARE(client.contextTimeMode(), QStringLiteral("local"));
        QTRY_COMPARE(client.contextTimeSunriseHour(), 6);
        QTRY_COMPARE(client.contextTimeSunsetHour(), 18);

        QSignalSpy timeSpy(&client, SIGNAL(contextTimeChanged()));
        QVERIFY(timeSpy.isValid());

        QVERIFY(client.setContextTimeMode(QStringLiteral("sun")));
        QTRY_COMPARE(client.contextTimeMode(), QStringLiteral("sun"));

        QVERIFY(client.setContextTimeSunriseHour(5));
        QTRY_COMPARE(client.contextTimeSunriseHour(), 5);

        QVERIFY(client.setContextTimeSunsetHour(19));
        QTRY_COMPARE(client.contextTimeSunsetHour(), 19);

        // Remote live update path through DBus signal.
        emit fake.SettingChanged(QStringLiteral("contextTime.mode"), QStringLiteral("local"));
        QTRY_COMPARE(client.contextTimeMode(), QStringLiteral("local"));

        QVERIFY(timeSpy.count() >= 1);
        fake.unregisterFromBus();
    }
};

QTEST_GUILESS_MAIN(DesktopSettingsClientTest)
#include "desktopsettingsclient_test.moc"

