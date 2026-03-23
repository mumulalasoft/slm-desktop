#include "settingbindingfactory.h"

#include "dbusbinding.h"
#include "gsettingsbinding.h"
#include "localsettingsbinding.h"
#include "mockbinding.h"
#include "networkmanagerbinding.h"
#include "settingbinding.h"

#include <QDebug>

SettingBindingFactory::SettingBindingFactory(QObject *parent)
    : QObject(parent)
{
}

SettingBinding *SettingBindingFactory::create(const QString &bindingSpec,
                                              const QVariant &defaultValue,
                                              QObject *owner) const
{
    return create(SettingBindingResolver::parse(bindingSpec, defaultValue), owner);
}

SettingBinding *SettingBindingFactory::create(const BindingDescriptor &descriptor,
                                              QObject *owner) const
{
    switch (descriptor.kind) {
    case BindingDescriptor::Kind::GSettings:
        return new GSettingsBinding(descriptor.schema, descriptor.key, owner);
    case BindingDescriptor::Kind::NetworkManagerState:
        return new NetworkManagerBinding(owner);
    case BindingDescriptor::Kind::NetworkManagerProperty:
        return new DBusBinding(descriptor.service,
                               descriptor.path,
                               descriptor.interfaceName,
                               descriptor.member,
                               DBusBinding::Mode::Property,
                               QDBusConnection::systemBus(),
                               owner);
    case BindingDescriptor::Kind::DBusProperty:
        return new DBusBinding(descriptor.service,
                               descriptor.path,
                               descriptor.interfaceName,
                               descriptor.member,
                               DBusBinding::Mode::Property,
                               QDBusConnection::systemBus(),
                               owner);
    case BindingDescriptor::Kind::DBusMethod:
        return new DBusBinding(descriptor.service,
                               descriptor.path,
                               descriptor.interfaceName,
                               descriptor.member,
                               DBusBinding::Mode::Method,
                               QDBusConnection::systemBus(),
                               owner);
    case BindingDescriptor::Kind::LocalSettings:
        return new LocalSettingsBinding(descriptor.localKey, descriptor.defaultValue, owner);
    case BindingDescriptor::Kind::Unsupported:
        qWarning() << "[slm-settings] unsupported backend_binding scheme:"
                   << descriptor.scheme << "spec=" << descriptor.original;
        return new MockBinding(descriptor.defaultValue, owner);
    case BindingDescriptor::Kind::Invalid:
    default:
        qWarning() << "[slm-settings] invalid backend_binding spec:"
                   << descriptor.original << "error=" << descriptor.error;
        return new MockBinding(descriptor.defaultValue, owner);
    }
}

