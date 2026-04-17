#pragma once

#include "settingsstore.h"
#include "src/services/theme-policy/appthemeclassifier.h"

#include <QObject>
#include <QDBusContext>
#include <QDBusVariant>
#include <QVariantMap>

class SettingsService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Settings")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit SettingsService(QObject *parent = nullptr);
    ~SettingsService() override;

    bool start(QString *error = nullptr);
    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetSettings() const;
    QVariantMap SetSetting(const QString &path, const QDBusVariant &value);
    QVariantMap SetSettingsPatch(const QVariantMap &patch);
    QVariantMap SubscribeChanges() const;
    QVariantMap ClassifyApp(const QVariantMap &appDescriptor) const;
    QVariantMap ResolveThemeForApp(const QVariantMap &appDescriptor,
                                   const QVariantMap &options = QVariantMap{});
    QString ServiceVersion() const;

signals:
    void serviceRegisteredChanged();
    void SettingChanged(const QString &path, const QDBusVariant &value);
    void AppearanceModeChanged(const QString &mode);
    void ThemePolicyChanged(const QVariantMap &policy);
    void IconPolicyChanged(const QVariantMap &policy);

private:
    static Slm::ThemePolicy::AppDescriptor appDescriptorFromVariant(const QVariantMap &map);
    static Slm::ThemePolicy::ClassificationRules rulesFromSettings(const QVariantMap &settings);
    void registerDbusService();
    void emitSemanticSignals(const QStringList &changedPaths);

    SettingsStore m_store;
    bool m_serviceRegistered = false;
};
