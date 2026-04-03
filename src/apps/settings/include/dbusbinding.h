#pragma once

#include "settingbinding.h"

#include <QDBusConnection>
#include <QString>

class DBusBinding : public SettingBinding
{
    Q_OBJECT

public:
    enum class Mode {
        Property,
        Method,
    };
    Q_ENUM(Mode)

    explicit DBusBinding(const QString &service,
                         const QString &path,
                         const QString &interfaceName,
                         const QString &member,
                         Mode mode = Mode::Property,
                         const QDBusConnection &bus = QDBusConnection::systemBus(),
                         QObject *parent = nullptr);

    QVariant value() const override;
    void setValue(const QVariant &newValue) override;

private:
    QString m_service;
    QString m_path;
    QString m_interfaceName;
    QString m_member;
    Mode m_mode = Mode::Property;
    QDBusConnection m_bus;
};

