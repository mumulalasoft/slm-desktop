#pragma once

#include "settingbindingresolver.h"

#include <QObject>
#include "settingbindingresolver.h"

class DesktopSettingsClient;
class SettingBinding;

class SettingBindingFactory : public QObject
{
    Q_OBJECT

public:
    explicit SettingBindingFactory(QObject *parent = nullptr);

    void setDesktopSettingsClient(DesktopSettingsClient *client) { m_client = client; }

    SettingBinding *create(const QString &bindingSpec,
                           const QVariant &defaultValue,
                           QObject *owner = nullptr) const;

    SettingBinding *create(const BindingDescriptor &descriptor,
                           QObject *owner = nullptr) const;

private:
    DesktopSettingsClient *m_client = nullptr;
};

