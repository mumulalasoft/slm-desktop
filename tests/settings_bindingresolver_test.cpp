#include <QtTest/QtTest>

#include "../src/apps/settings/include/settingbindingresolver.h"

class SettingsBindingResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void parseGsettings_contract()
    {
        const BindingDescriptor d =
            SettingBindingResolver::parse(QStringLiteral("gsettings:org.gnome.desktop.interface/color-scheme"));
        QCOMPARE(d.kind, BindingDescriptor::Kind::GSettings);
        QCOMPARE(d.schema, QStringLiteral("org.gnome.desktop.interface"));
        QCOMPARE(d.key, QStringLiteral("color-scheme"));
    }

    void parseDbusNetwork_contract()
    {
        const BindingDescriptor d =
            SettingBindingResolver::parse(QStringLiteral("dbus:org.freedesktop.NetworkManager/WirelessEnabled"));
        QCOMPARE(d.kind, BindingDescriptor::Kind::NetworkManagerProperty);
        QCOMPARE(d.service, QStringLiteral("org.freedesktop.NetworkManager"));
        QCOMPARE(d.path, QStringLiteral("/org/freedesktop/NetworkManager"));
        QCOMPARE(d.interfaceName, QStringLiteral("org.freedesktop.NetworkManager"));
        QCOMPARE(d.member, QStringLiteral("WirelessEnabled"));
    }

    void parseLocalSettings_contract()
    {
        const BindingDescriptor d =
            SettingBindingResolver::parse(QStringLiteral("settings:appearance/accent"));
        QCOMPARE(d.kind, BindingDescriptor::Kind::LocalSettings);
        QCOMPARE(d.localKey, QStringLiteral("settings/appearance/accent"));
    }

    void parseInvalid_contract()
    {
        const BindingDescriptor d = SettingBindingResolver::parse(QStringLiteral("broken"));
        QCOMPARE(d.kind, BindingDescriptor::Kind::Invalid);
        QVERIFY(!d.error.isEmpty());
    }

    void parseDbusCanonicalMethod_contract()
    {
        const BindingDescriptor d =
            SettingBindingResolver::parse(
                QStringLiteral("dbus:org.bluez:/org/bluez/hci0:org.bluez.Adapter1:StartDiscovery:method"));
        QCOMPARE(d.kind, BindingDescriptor::Kind::DBusMethod);
        QCOMPARE(d.service, QStringLiteral("org.bluez"));
        QCOMPARE(d.path, QStringLiteral("/org/bluez/hci0"));
        QCOMPARE(d.interfaceName, QStringLiteral("org.bluez.Adapter1"));
        QCOMPARE(d.member, QStringLiteral("StartDiscovery"));
    }

    void parseUnsupportedScheme_contract()
    {
        const BindingDescriptor d =
            SettingBindingResolver::parse(QStringLiteral("foobar:alpha/beta"));
        QCOMPARE(d.kind, BindingDescriptor::Kind::Unsupported);
        QCOMPARE(d.scheme, QStringLiteral("foobar"));
        QVERIFY(!d.error.isEmpty());
    }

    void parseInvalidGsettings_contract()
    {
        const BindingDescriptor d =
            SettingBindingResolver::parse(QStringLiteral("gsettings:org.gnome.desktop.interface"));
        QCOMPARE(d.kind, BindingDescriptor::Kind::Invalid);
        QCOMPARE(d.error, QStringLiteral("invalid-gsettings-spec"));
    }
};

QTEST_MAIN(SettingsBindingResolverTest)
#include "settings_bindingresolver_test.moc"
