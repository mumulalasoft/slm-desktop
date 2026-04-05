#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class QDBusInterface;
class UIPreferences;

class DesktopSettingsClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString themeMode READ themeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(QString gtkThemeLight READ gtkThemeLight NOTIFY gtkThemeLightChanged)
    Q_PROPERTY(QString gtkThemeDark READ gtkThemeDark NOTIFY gtkThemeDarkChanged)
    Q_PROPERTY(QString kdeColorSchemeLight READ kdeColorSchemeLight NOTIFY kdeColorSchemeLightChanged)
    Q_PROPERTY(QString kdeColorSchemeDark READ kdeColorSchemeDark NOTIFY kdeColorSchemeDarkChanged)
    Q_PROPERTY(QString gtkIconThemeLight READ gtkIconThemeLight NOTIFY gtkIconThemeLightChanged)
    Q_PROPERTY(QString gtkIconThemeDark READ gtkIconThemeDark NOTIFY gtkIconThemeDarkChanged)
    Q_PROPERTY(QString kdeIconThemeLight READ kdeIconThemeLight NOTIFY kdeIconThemeLightChanged)
    Q_PROPERTY(QString kdeIconThemeDark READ kdeIconThemeDark NOTIFY kdeIconThemeDarkChanged)
    Q_PROPERTY(bool qtGenericAllowKdeCompat READ qtGenericAllowKdeCompat NOTIFY qtGenericAllowKdeCompatChanged)
    Q_PROPERTY(bool qtIncompatibleUseDesktopFallback READ qtIncompatibleUseDesktopFallback NOTIFY qtIncompatibleUseDesktopFallbackChanged)
    Q_PROPERTY(bool unknownUsesSafeFallback READ unknownUsesSafeFallback NOTIFY unknownUsesSafeFallbackChanged)
    Q_PROPERTY(bool contextAutoReduceAnimation READ contextAutoReduceAnimation NOTIFY contextAutomationChanged)
    Q_PROPERTY(bool contextAutoDisableBlur READ contextAutoDisableBlur NOTIFY contextAutomationChanged)
    Q_PROPERTY(bool contextAutoDisableHeavyEffects READ contextAutoDisableHeavyEffects NOTIFY contextAutomationChanged)
    Q_PROPERTY(QString contextTimeMode READ contextTimeMode NOTIFY contextTimeChanged)
    Q_PROPERTY(int contextTimeSunriseHour READ contextTimeSunriseHour NOTIFY contextTimeChanged)
    Q_PROPERTY(int contextTimeSunsetHour READ contextTimeSunsetHour NOTIFY contextTimeChanged)

public:
    explicit DesktopSettingsClient(UIPreferences *fallbackPrefs, QObject *parent = nullptr);

    bool available() const;
    QString themeMode() const;
    QString accentColor() const;
    double fontScale() const;
    QString gtkThemeLight() const;
    QString gtkThemeDark() const;
    QString kdeColorSchemeLight() const;
    QString kdeColorSchemeDark() const;
    QString gtkIconThemeLight() const;
    QString gtkIconThemeDark() const;
    QString kdeIconThemeLight() const;
    QString kdeIconThemeDark() const;
    bool qtGenericAllowKdeCompat() const;
    bool qtIncompatibleUseDesktopFallback() const;
    bool unknownUsesSafeFallback() const;
    bool contextAutoReduceAnimation() const;
    bool contextAutoDisableBlur() const;
    bool contextAutoDisableHeavyEffects() const;
    QString contextTimeMode() const;
    int contextTimeSunriseHour() const;
    int contextTimeSunsetHour() const;

    Q_INVOKABLE bool setThemeMode(const QString &mode);
    Q_INVOKABLE bool setAccentColor(const QString &color);
    Q_INVOKABLE bool setFontScale(double scale);
    Q_INVOKABLE bool setGtkThemeLight(const QString &value);
    Q_INVOKABLE bool setGtkThemeDark(const QString &value);
    Q_INVOKABLE bool setKdeColorSchemeLight(const QString &value);
    Q_INVOKABLE bool setKdeColorSchemeDark(const QString &value);
    Q_INVOKABLE bool setGtkIconThemeLight(const QString &value);
    Q_INVOKABLE bool setGtkIconThemeDark(const QString &value);
    Q_INVOKABLE bool setKdeIconThemeLight(const QString &value);
    Q_INVOKABLE bool setKdeIconThemeDark(const QString &value);
    Q_INVOKABLE bool setQtGenericAllowKdeCompat(bool enabled);
    Q_INVOKABLE bool setQtIncompatibleUseDesktopFallback(bool enabled);
    Q_INVOKABLE bool setUnknownUsesSafeFallback(bool enabled);
    Q_INVOKABLE bool setContextAutoReduceAnimation(bool enabled);
    Q_INVOKABLE bool setContextAutoDisableBlur(bool enabled);
    Q_INVOKABLE bool setContextAutoDisableHeavyEffects(bool enabled);
    Q_INVOKABLE bool setContextTimeMode(const QString &mode);
    Q_INVOKABLE bool setContextTimeSunriseHour(int hour);
    Q_INVOKABLE bool setContextTimeSunsetHour(int hour);
    Q_INVOKABLE bool setPerAppOverride(const QString &appIdOrDesktopId, const QString &category);
    Q_INVOKABLE bool clearPerAppOverride(const QString &appIdOrDesktopId);
    Q_INVOKABLE QVariantMap classifyApp(const QVariantMap &appDescriptor);
    Q_INVOKABLE QVariantMap resolveThemeForApp(const QVariantMap &appDescriptor,
                                               const QVariantMap &options = QVariantMap{});
    Q_INVOKABLE void refresh();

signals:
    void availableChanged();
    void themeModeChanged();
    void accentColorChanged();
    void fontScaleChanged();
    void gtkThemeLightChanged();
    void gtkThemeDarkChanged();
    void kdeColorSchemeLightChanged();
    void kdeColorSchemeDarkChanged();
    void gtkIconThemeLightChanged();
    void gtkIconThemeDarkChanged();
    void kdeIconThemeLightChanged();
    void kdeIconThemeDarkChanged();
    void qtGenericAllowKdeCompatChanged();
    void qtIncompatibleUseDesktopFallbackChanged();
    void unknownUsesSafeFallbackChanged();
    void contextAutomationChanged();
    void contextTimeChanged();

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onSettingChanged(const QString &path, const QVariant &value);
    void onAppearanceModeChanged(const QString &mode);

private:
    bool ensureIface();
    bool setSetting(const QString &path, const QVariant &value);
    void loadFromService();
    void loadFromFallback();

    void setThemeModeLocal(const QString &mode);
    void setAccentColorLocal(const QString &color);
    void setFontScaleLocal(double scale);
    void setThemeStringLocal(QString &slot, const QString &value, void (DesktopSettingsClient::*signal)());

    UIPreferences *m_fallbackPrefs = nullptr;
    QDBusInterface *m_iface = nullptr;
    bool m_available = false;
    QString m_themeMode;
    QString m_accentColor;
    double m_fontScale = 1.0;
    QString m_gtkThemeLight;
    QString m_gtkThemeDark;
    QString m_kdeColorSchemeLight;
    QString m_kdeColorSchemeDark;
    QString m_gtkIconThemeLight;
    QString m_gtkIconThemeDark;
    QString m_kdeIconThemeLight;
    QString m_kdeIconThemeDark;
    bool m_qtGenericAllowKdeCompat = true;
    bool m_qtIncompatibleUseDesktopFallback = true;
    bool m_unknownUsesSafeFallback = true;
    bool m_contextAutoReduceAnimation = true;
    bool m_contextAutoDisableBlur = true;
    bool m_contextAutoDisableHeavyEffects = true;
    QString m_contextTimeMode = QStringLiteral("local");
    int m_contextTimeSunriseHour = 6;
    int m_contextTimeSunsetHour = 18;
};
