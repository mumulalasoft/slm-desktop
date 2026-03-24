#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QFileSystemWatcher>

class UIPreferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString dockMotionPreset READ dockMotionPreset NOTIFY dockMotionPresetChanged)
    Q_PROPERTY(bool dockDropPulseEnabled READ dockDropPulseEnabled NOTIFY dockDropPulseEnabledChanged)
    Q_PROPERTY(bool dockAutoHideEnabled READ dockAutoHideEnabled NOTIFY dockAutoHideEnabledChanged)
    Q_PROPERTY(int dockDragThresholdMouse READ dockDragThresholdMouse NOTIFY dockDragThresholdMouseChanged)
    Q_PROPERTY(int dockDragThresholdTouchpad READ dockDragThresholdTouchpad NOTIFY dockDragThresholdTouchpadChanged)
    Q_PROPERTY(bool verboseLogging READ verboseLogging NOTIFY verboseLoggingChanged)
    Q_PROPERTY(QString iconThemeLight READ iconThemeLight NOTIFY iconThemeLightChanged)
    Q_PROPERTY(QString iconThemeDark READ iconThemeDark NOTIFY iconThemeDarkChanged)
    Q_PROPERTY(QString wallpaperUri READ wallpaperUri WRITE setWallpaperUri NOTIFY wallpaperUriChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(qreal fontScale READ fontScale WRITE setFontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(QString gtkThemeLight READ gtkThemeLight NOTIFY gtkThemeLightChanged)
    Q_PROPERTY(QString gtkThemeDark READ gtkThemeDark NOTIFY gtkThemeDarkChanged)
    Q_PROPERTY(QString kdeColorSchemeLight READ kdeColorSchemeLight NOTIFY kdeColorSchemeLightChanged)
    Q_PROPERTY(QString kdeColorSchemeDark READ kdeColorSchemeDark NOTIFY kdeColorSchemeDarkChanged)
    Q_PROPERTY(QString gtkIconThemeLight READ gtkIconThemeLight NOTIFY gtkIconThemeLightChanged)
    Q_PROPERTY(QString gtkIconThemeDark READ gtkIconThemeDark NOTIFY gtkIconThemeDarkChanged)
    Q_PROPERTY(QString kdeIconThemeLight READ kdeIconThemeLight NOTIFY kdeIconThemeLightChanged)
    Q_PROPERTY(QString kdeIconThemeDark READ kdeIconThemeDark NOTIFY kdeIconThemeDarkChanged)
    // Font role specs stored as "Family,Style,Size" — empty means system default.
    Q_PROPERTY(QString defaultFont READ defaultFont NOTIFY defaultFontChanged)
    Q_PROPERTY(QString documentFont READ documentFont NOTIFY documentFontChanged)
    Q_PROPERTY(QString monospaceFont READ monospaceFont NOTIFY monospaceFontChanged)
    Q_PROPERTY(QString titlebarFont READ titlebarFont NOTIFY titlebarFontChanged)

public:
    explicit UIPreferences(QObject *parent = nullptr);

    QString dockMotionPreset() const;
    bool dockDropPulseEnabled() const;
    bool dockAutoHideEnabled() const;
    int dockDragThresholdMouse() const;
    int dockDragThresholdTouchpad() const;
    bool verboseLogging() const;
    QString iconThemeLight() const;
    QString iconThemeDark() const;
    QString wallpaperUri() const;
    QString themeMode() const;
    QString accentColor() const;
    qreal fontScale() const;
    QString gtkThemeLight() const;
    QString gtkThemeDark() const;
    QString kdeColorSchemeLight() const;
    QString kdeColorSchemeDark() const;
    QString gtkIconThemeLight() const;
    QString gtkIconThemeDark() const;
    QString kdeIconThemeLight() const;
    QString kdeIconThemeDark() const;
    QString defaultFont() const;
    QString documentFont() const;
    QString monospaceFont() const;
    QString titlebarFont() const;

    Q_INVOKABLE void setDockMotionPreset(const QString &preset);
    Q_INVOKABLE void setDockDropPulseEnabled(bool enabled);
    Q_INVOKABLE void setDockAutoHideEnabled(bool enabled);
    Q_INVOKABLE void setDockDragThresholdMouse(int thresholdPx);
    Q_INVOKABLE void setDockDragThresholdTouchpad(int thresholdPx);
    Q_INVOKABLE void setVerboseLogging(bool enabled);
    Q_INVOKABLE void setIconThemeLight(const QString &themeName);
    Q_INVOKABLE void setIconThemeDark(const QString &themeName);
    Q_INVOKABLE void setIconThemeMapping(const QString &lightThemeName, const QString &darkThemeName);
    Q_INVOKABLE void setWallpaperUri(const QString &uri);
    Q_INVOKABLE void setThemeMode(const QString &mode);
    Q_INVOKABLE void setAccentColor(const QString &color);
    Q_INVOKABLE void setFontScale(qreal scale);
    Q_INVOKABLE void setGtkThemeLight(const QString &theme);
    Q_INVOKABLE void setGtkThemeDark(const QString &theme);
    Q_INVOKABLE void setKdeColorSchemeLight(const QString &scheme);
    Q_INVOKABLE void setKdeColorSchemeDark(const QString &scheme);
    Q_INVOKABLE void setGtkIconThemeLight(const QString &theme);
    Q_INVOKABLE void setGtkIconThemeDark(const QString &theme);
    Q_INVOKABLE void setKdeIconThemeLight(const QString &theme);
    Q_INVOKABLE void setKdeIconThemeDark(const QString &theme);
    Q_INVOKABLE void setDefaultFont(const QString &spec);
    Q_INVOKABLE void setDocumentFont(const QString &spec);
    Q_INVOKABLE void setMonospaceFont(const QString &spec);
    Q_INVOKABLE void setTitlebarFont(const QString &spec);
    Q_INVOKABLE QVariant getPreference(const QString &key, const QVariant &fallback = QVariant()) const;
    Q_INVOKABLE bool setPreference(const QString &key, const QVariant &value);
    Q_INVOKABLE bool resetPreference(const QString &key);
    Q_INVOKABLE QVariantMap preferenceSnapshot() const;
    Q_INVOKABLE QString settingsFilePath() const;

signals:
    void dockMotionPresetChanged();
    void dockDropPulseEnabledChanged();
    void dockAutoHideEnabledChanged();
    void dockDragThresholdMouseChanged();
    void dockDragThresholdTouchpadChanged();
    void verboseLoggingChanged();
    void iconThemeLightChanged();
    void iconThemeDarkChanged();
    void wallpaperUriChanged();
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
    void defaultFontChanged();
    void documentFontChanged();
    void monospaceFontChanged();
    void titlebarFontChanged();
    void preferenceChanged(QString key, QVariant value);

private:
    QString sanitizePreset(const QString &preset) const;
    int clampThreshold(int value) const;
    void syncSettings();
    void watchSettingsFile();
    void onSettingsFileChanged(const QString &path);

    QSettings m_settings;
    QFileSystemWatcher m_fileWatcher;
    QString m_dockMotionPreset;
    bool m_dockDropPulseEnabled = true;
    bool m_dockAutoHideEnabled = false;
    int m_dockDragThresholdMouse = 6;
    int m_dockDragThresholdTouchpad = 3;
    bool m_verboseLogging = false;
    QString m_iconThemeLight;
    QString m_iconThemeDark;
    QString m_wallpaperUri;
    QString m_themeMode;
    QString m_accentColor;
    qreal m_fontScale = 1.0;
    QString m_gtkThemeLight;
    QString m_gtkThemeDark;
    QString m_kdeColorSchemeLight;
    QString m_kdeColorSchemeDark;
    QString m_gtkIconThemeLight;
    QString m_gtkIconThemeDark;
    QString m_kdeIconThemeLight;
    QString m_kdeIconThemeDark;
    QString m_defaultFont;
    QString m_documentFont;
    QString m_monospaceFont;
    QString m_titlebarFont;
};
