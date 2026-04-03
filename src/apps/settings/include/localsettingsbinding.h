#pragma once

#include "settingbinding.h"

#include <QVariant>

class LocalSettingsBinding : public SettingBinding
{
    Q_OBJECT

public:
    explicit LocalSettingsBinding(const QString &key,
                                  const QVariant &defaultValue = {},
                                  QObject *parent = nullptr);

    QVariant value() const override;
    void setValue(const QVariant &newValue) override;

private:
    QString m_key;
    QVariant m_defaultValue;
};

