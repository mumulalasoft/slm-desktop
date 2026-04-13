#include <QtTest/QtTest>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusVariant>

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
                 {QStringLiteral("dock"), QVariantMap{
                      {QStringLiteral("motionPreset"), m_dockMotion},
                      {QStringLiteral("autoHideEnabled"), m_dockAutoHide},
                      {QStringLiteral("dropPulseEnabled"), m_dockDropPulse},
                      {QStringLiteral("dragThresholdMouse"), m_dockDragMouse},
                      {QStringLiteral("dragThresholdTouchpad"), m_dockDragTouchpad},
                      {QStringLiteral("iconSize"), m_dockIconSize},
                      {QStringLiteral("magnificationEnabled"), m_dockMagnification},
                  }},
                 {QStringLiteral("print"), QVariantMap{
                      {QStringLiteral("pdfFallbackPrinterId"), m_pdfFallbackPrinterId},
                  }},
                 {QStringLiteral("windowing"), QVariantMap{
                      {QStringLiteral("animationEnabled"), m_animationEnabled},
                      {QStringLiteral("controlsSide"), m_controlsSide},
                      {QStringLiteral("bindClose"), m_bindClose},
                      {QStringLiteral("bindWorkspace"), m_bindWorkspace},
                  }},
                 {QStringLiteral("shortcuts"), QVariantMap{
                      {QStringLiteral("workspaceOverview"), m_workspaceOverview},
                  }},
                 {QStringLiteral("fonts"), QVariantMap{
                      {QStringLiteral("defaultFont"), m_defaultFont},
                      {QStringLiteral("documentFont"), m_documentFont},
                      {QStringLiteral("monospaceFont"), m_monospaceFont},
                      {QStringLiteral("titlebarFont"), m_titlebarFont},
                  }},
                 {QStringLiteral("wallpaper"), QVariantMap{
                      {QStringLiteral("uri"), m_wallpaperUri},
                  }},
             }},
        };
    }

    QVariantMap SetSetting(const QString &path, const QDBusVariant &value)
    {
        const QVariant rawValue = value.variant();
        if (path == QLatin1String("contextTime.mode")) {
            const QString mode = rawValue.toString().trimmed().toLower();
            if (mode != QLatin1String("local") && mode != QLatin1String("sun")) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-mode")},
                };
            }
            m_mode = mode;
            emit SettingChanged(path, QDBusVariant(m_mode));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("contextTime.sunriseHour")) {
            m_sunrise = qBound(0, rawValue.toInt(), 23);
            emit SettingChanged(path, QDBusVariant(m_sunrise));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("contextTime.sunsetHour")) {
            m_sunset = qBound(0, rawValue.toInt(), 23);
            emit SettingChanged(path, QDBusVariant(m_sunset));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("dock.motionPreset")) {
            const QString v = rawValue.toString().trimmed().toLower();
            if (v != QLatin1String("subtle") && v != QLatin1String("macos-lively")) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-dock-motion")},
                };
            }
            m_dockMotion = v;
            emit SettingChanged(path, QDBusVariant(m_dockMotion));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("dock.autoHideEnabled")) {
            m_dockAutoHide = rawValue.toBool();
            emit SettingChanged(path, QDBusVariant(m_dockAutoHide));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("dock.dropPulseEnabled")) {
            m_dockDropPulse = rawValue.toBool();
            emit SettingChanged(path, QDBusVariant(m_dockDropPulse));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("dock.dragThresholdMouse")) {
            m_dockDragMouse = qBound(2, rawValue.toInt(), 24);
            emit SettingChanged(path, QDBusVariant(m_dockDragMouse));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("dock.dragThresholdTouchpad")) {
            m_dockDragTouchpad = qBound(2, rawValue.toInt(), 24);
            emit SettingChanged(path, QDBusVariant(m_dockDragTouchpad));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("dock.iconSize")) {
            const QString v = rawValue.toString().trimmed().toLower();
            if (v != QLatin1String("small")
                    && v != QLatin1String("medium")
                    && v != QLatin1String("large")) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-dock-icon-size")},
                };
            }
            m_dockIconSize = v;
            emit SettingChanged(path, QDBusVariant(m_dockIconSize));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("dock.magnificationEnabled")) {
            m_dockMagnification = rawValue.toBool();
            emit SettingChanged(path, QDBusVariant(m_dockMagnification));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("print.pdfFallbackPrinterId")) {
            m_pdfFallbackPrinterId = rawValue.toString().trimmed();
            emit SettingChanged(path, QDBusVariant(m_pdfFallbackPrinterId));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("windowing.bindClose")) {
            const QString v = rawValue.toString().trimmed();
            if (v.isEmpty()) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-shortcut")},
                };
            }
            m_bindClose = v;
            emit SettingChanged(path, QDBusVariant(m_bindClose));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("windowing.animationEnabled")) {
            m_animationEnabled = rawValue.toBool();
            emit SettingChanged(path, QDBusVariant(m_animationEnabled));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("windowing.controlsSide")) {
            const QString v = rawValue.toString().trimmed().toLower();
            if (v != QLatin1String("left") && v != QLatin1String("right")) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-controls-side")},
                };
            }
            m_controlsSide = v;
            emit SettingChanged(path, QDBusVariant(m_controlsSide));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("windowing.bindWorkspace")) {
            const QString v = rawValue.toString().trimmed();
            if (v.isEmpty()) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-shortcut")},
                };
            }
            m_bindWorkspace = v;
            emit SettingChanged(path, QDBusVariant(m_bindWorkspace));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("shortcuts.workspaceOverview")) {
            const QString v = rawValue.toString().trimmed();
            if (v.isEmpty()) {
                return {
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("invalid-shortcut")},
                };
            }
            m_workspaceOverview = v;
            emit SettingChanged(path, QDBusVariant(m_workspaceOverview));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("fonts.defaultFont")) {
            m_defaultFont = rawValue.toString().trimmed();
            emit SettingChanged(path, QDBusVariant(m_defaultFont));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("fonts.documentFont")) {
            m_documentFont = rawValue.toString().trimmed();
            emit SettingChanged(path, QDBusVariant(m_documentFont));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("fonts.monospaceFont")) {
            m_monospaceFont = rawValue.toString().trimmed();
            emit SettingChanged(path, QDBusVariant(m_monospaceFont));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("fonts.titlebarFont")) {
            m_titlebarFont = rawValue.toString().trimmed();
            emit SettingChanged(path, QDBusVariant(m_titlebarFont));
            return {{QStringLiteral("ok"), true}};
        }
        if (path == QLatin1String("wallpaper.uri")) {
            m_wallpaperUri = rawValue.toString().trimmed();
            emit SettingChanged(path, QDBusVariant(m_wallpaperUri));
            return {{QStringLiteral("ok"), true}};
        }
        // Accept unrelated settings to keep client stable in test.
        emit SettingChanged(path, value);
        return {{QStringLiteral("ok"), true}};
    }

signals:
    void SettingChanged(const QString &path, const QDBusVariant &value);
    void AppearanceModeChanged(const QString &mode);

private:
    QString m_mode = QStringLiteral("local");
    int m_sunrise = 6;
    int m_sunset = 18;
    QString m_dockMotion = QStringLiteral("subtle");
    bool m_dockAutoHide = false;
    bool m_dockDropPulse = true;
    int m_dockDragMouse = 6;
    int m_dockDragTouchpad = 3;
    QString m_dockIconSize = QStringLiteral("medium");
    bool m_dockMagnification = true;
    QString m_pdfFallbackPrinterId;
    bool m_animationEnabled = true;
    QString m_controlsSide = QStringLiteral("right");
    QString m_bindClose = QStringLiteral("Alt+F4");
    QString m_bindWorkspace = QStringLiteral("Alt+F3");
    QString m_workspaceOverview = QStringLiteral("Meta+S");
    QString m_defaultFont;
    QString m_documentFont;
    QString m_monospaceFont;
    QString m_titlebarFont;
    QString m_wallpaperUri;
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

        QTRY_COMPARE(client.dockMotionPreset(), QStringLiteral("subtle"));
        QTRY_COMPARE(client.dockDragThresholdMouse(), 6);
        QTRY_COMPARE(client.dockDragThresholdTouchpad(), 3);
        QVERIFY(client.setDockMotionPreset(QStringLiteral("macos-lively")));
        QTRY_COMPARE(client.dockMotionPreset(), QStringLiteral("macos-lively"));
        QVERIFY(client.setDockAutoHideEnabled(true));
        QTRY_VERIFY(client.dockAutoHideEnabled());
        QVERIFY(client.setDockDropPulseEnabled(false));
        QTRY_VERIFY(!client.dockDropPulseEnabled());
        QVERIFY(client.setDockDragThresholdMouse(12));
        QTRY_COMPARE(client.dockDragThresholdMouse(), 12);
        QVERIFY(client.setDockDragThresholdTouchpad(8));
        QTRY_COMPARE(client.dockDragThresholdTouchpad(), 8);
        QTRY_COMPARE(client.dockIconSize(), QStringLiteral("medium"));
        QVERIFY(client.setDockIconSize(QStringLiteral("large")));
        QTRY_COMPARE(client.dockIconSize(), QStringLiteral("large"));
        QVERIFY(client.setDockMagnificationEnabled(false));
        QTRY_VERIFY(!client.dockMagnificationEnabled());
        QTRY_COMPARE(client.printPdfFallbackPrinterId(), QStringLiteral(""));
        QVERIFY(client.setPrintPdfFallbackPrinterId(QStringLiteral("office-printer")));
        QTRY_COMPARE(client.printPdfFallbackPrinterId(), QStringLiteral("office-printer"));
        QTRY_VERIFY(client.windowingAnimationEnabled());
        QVERIFY(client.setWindowingAnimationEnabled(false));
        QTRY_VERIFY(!client.windowingAnimationEnabled());
        QTRY_COMPARE(client.windowControlsSide(), QStringLiteral("right"));
        QVERIFY(client.setWindowControlsSide(QStringLiteral("left")));
        QTRY_COMPARE(client.windowControlsSide(), QStringLiteral("left"));
        QTRY_COMPARE(client.keyboardShortcut(QStringLiteral("windowing.bindClose"), QStringLiteral("Alt+F4")),
                     QStringLiteral("Alt+F4"));
        QVERIFY(client.setKeyboardShortcut(QStringLiteral("windowing.bindClose"), QStringLiteral("Meta+Q")));
        QTRY_COMPARE(client.keyboardShortcut(QStringLiteral("windowing.bindClose"), QStringLiteral("Alt+F4")),
                     QStringLiteral("Meta+Q"));
        QVERIFY(client.setKeyboardShortcut(QStringLiteral("shortcuts.workspaceOverview"), QStringLiteral("Meta+D")));
        QTRY_COMPARE(client.keyboardShortcut(QStringLiteral("shortcuts.workspaceOverview"), QStringLiteral("Meta+S")),
                     QStringLiteral("Meta+D"));
        QTRY_COMPARE(client.defaultFont(), QStringLiteral(""));
        QVERIFY(client.setDefaultFont(QStringLiteral("Noto Sans,Regular,11")));
        QTRY_COMPARE(client.defaultFont(), QStringLiteral("Noto Sans,Regular,11"));
        QVERIFY(client.setDocumentFont(QStringLiteral("Noto Sans,Regular,12")));
        QTRY_COMPARE(client.documentFont(), QStringLiteral("Noto Sans,Regular,12"));
        QVERIFY(client.setMonospaceFont(QStringLiteral("Noto Sans Mono,Regular,11")));
        QTRY_COMPARE(client.monospaceFont(), QStringLiteral("Noto Sans Mono,Regular,11"));
        QVERIFY(client.setTitlebarFont(QStringLiteral("Noto Sans,Medium,11")));
        QTRY_COMPARE(client.titlebarFont(), QStringLiteral("Noto Sans,Medium,11"));
        QTRY_COMPARE(client.wallpaperUri(), QStringLiteral(""));
        QVERIFY(client.setWallpaperUri(QStringLiteral("file:///tmp/wall.jpg")));
        QTRY_COMPARE(client.wallpaperUri(), QStringLiteral("file:///tmp/wall.jpg"));

        // Remote live update path through DBus signal.
        emit fake.SettingChanged(QStringLiteral("contextTime.mode"),
                                 QDBusVariant(QStringLiteral("local")));
        QTRY_COMPARE(client.contextTimeMode(), QStringLiteral("local"));
        emit fake.SettingChanged(QStringLiteral("dock.motionPreset"),
                                 QDBusVariant(QStringLiteral("subtle")));
        QTRY_COMPARE(client.dockMotionPreset(), QStringLiteral("subtle"));
        emit fake.SettingChanged(QStringLiteral("dock.iconSize"),
                                 QDBusVariant(QStringLiteral("small")));
        QTRY_COMPARE(client.dockIconSize(), QStringLiteral("small"));
        emit fake.SettingChanged(QStringLiteral("print.pdfFallbackPrinterId"),
                                 QDBusVariant(QStringLiteral("lab-printer")));
        QTRY_COMPARE(client.printPdfFallbackPrinterId(), QStringLiteral("lab-printer"));
        emit fake.SettingChanged(QStringLiteral("windowing.bindWorkspace"),
                                 QDBusVariant(QStringLiteral("Meta+W")));
        QTRY_COMPARE(client.keyboardShortcut(QStringLiteral("windowing.bindWorkspace"),
                                             QStringLiteral("Alt+F3")),
                     QStringLiteral("Meta+W"));
        emit fake.SettingChanged(QStringLiteral("windowing.animationEnabled"),
                                 QDBusVariant(true));
        QTRY_VERIFY(client.windowingAnimationEnabled());
        emit fake.SettingChanged(QStringLiteral("windowing.controlsSide"),
                                 QDBusVariant(QStringLiteral("right")));
        QTRY_COMPARE(client.windowControlsSide(), QStringLiteral("right"));
        emit fake.SettingChanged(QStringLiteral("fonts.defaultFont"),
                                 QDBusVariant(QStringLiteral("Inter,Regular,12")));
        QTRY_COMPARE(client.defaultFont(), QStringLiteral("Inter,Regular,12"));
        emit fake.SettingChanged(QStringLiteral("wallpaper.uri"),
                                 QDBusVariant(QStringLiteral("file:///tmp/desk.jpg")));
        QTRY_COMPARE(client.wallpaperUri(), QStringLiteral("file:///tmp/desk.jpg"));

        QVERIFY(timeSpy.count() >= 1);
        fake.unregisterFromBus();
    }
};

QTEST_GUILESS_MAIN(DesktopSettingsClientTest)
#include "desktopsettingsclient_test.moc"
