#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>

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

    Q_INVOKABLE void setDockMotionPreset(const QString &preset);
    Q_INVOKABLE void setDockDropPulseEnabled(bool enabled);
    Q_INVOKABLE void setDockAutoHideEnabled(bool enabled);
    Q_INVOKABLE void setDockDragThresholdMouse(int thresholdPx);
    Q_INVOKABLE void setDockDragThresholdTouchpad(int thresholdPx);
    Q_INVOKABLE void setVerboseLogging(bool enabled);
    Q_INVOKABLE void setIconThemeLight(const QString &themeName);
    Q_INVOKABLE void setIconThemeDark(const QString &themeName);
    Q_INVOKABLE void setIconThemeMapping(const QString &lightThemeName, const QString &darkThemeName);
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
    void preferenceChanged(QString key, QVariant value);

private:
    QString sanitizePreset(const QString &preset) const;
    int clampThreshold(int value) const;
    void syncSettings();

    QSettings m_settings;
    QString m_dockMotionPreset;
    bool m_dockDropPulseEnabled = true;
    bool m_dockAutoHideEnabled = false;
    int m_dockDragThresholdMouse = 6;
    int m_dockDragThresholdTouchpad = 3;
    bool m_verboseLogging = false;
    QString m_iconThemeLight;
    QString m_iconThemeDark;
};
