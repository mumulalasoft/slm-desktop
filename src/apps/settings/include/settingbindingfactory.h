#pragma once

#include "settingbindingresolver.h"

#include <QObject>

class SettingBinding;

class SettingBindingFactory : public QObject
{
    Q_OBJECT

public:
    explicit SettingBindingFactory(QObject *parent = nullptr);

    SettingBinding *create(const QString &bindingSpec,
                           const QVariant &defaultValue,
                           QObject *owner) const;
    SettingBinding *create(const BindingDescriptor &descriptor,
                           QObject *owner) const;
};

