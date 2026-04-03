#pragma once

#include <QString>
#include <QVariant>

struct BindingDescriptor
{
    enum class Kind {
        Invalid,
        GSettings,
        NetworkManagerState,
        NetworkManagerProperty,
        DBusProperty,
        DBusMethod,
        LocalSettings,
        Unsupported,
    };

    Kind kind = Kind::Invalid;
    QString original;
    QString scheme;
    QString error;

    // GSettings
    QString schema;
    QString key;

    // D-Bus
    QString service;
    QString path;
    QString interfaceName;
    QString member;

    // Local settings fallback
    QString localKey;
    QVariant defaultValue;

    bool isValid() const { return kind != Kind::Invalid; }
};

class SettingBindingResolver
{
public:
    static BindingDescriptor parse(const QString &bindingSpec,
                                   const QVariant &defaultValue = {});
};

