#pragma once

#include "settingbinding.h"
#include "../desktopsettingsclient.h"

class DesktopSettingsBinding : public SettingBinding
{
    Q_OBJECT

public:
    DesktopSettingsBinding(DesktopSettingsClient *client,
                           const QString &path,
                           const QVariant &defaultValue,
                           QObject *parent = nullptr);

    QVariant value() const override;
    void setValue(const QVariant &newValue) override;

private:
    DesktopSettingsClient *m_client;
    QString m_path;
    QVariant m_defaultValue;
};
