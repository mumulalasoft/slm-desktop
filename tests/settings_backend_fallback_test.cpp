#include <QtTest/QtTest>
#include <memory>

#include "../src/apps/settings/include/dbusbinding.h"
#include "../src/apps/settings/include/gsettingsbinding.h"
#include "../src/apps/settings/include/mockbinding.h"
#include "../src/apps/settings/include/settingbindingfactory.h"

class SettingsBackendFallbackTest : public QObject
{
    Q_OBJECT

private slots:
    void gsettingsMissingSchema_reportsUnavailable()
    {
        GSettingsBinding binding(QStringLiteral("org.slm.settings.missing"), QStringLiteral("enabled"));
        QSignalSpy finishedSpy(&binding, SIGNAL(operationFinished(QString,bool,QString)));
        QSignalSpy errorSpy(&binding, SIGNAL(errorOccurred(QString)));

        binding.setValue(true);

        QCOMPARE(finishedSpy.count(), 1);
        const QList<QVariant> args = finishedSpy.takeFirst();
        QCOMPARE(args.value(0).toString(), QStringLiteral("write"));
        QCOMPARE(args.value(1).toBool(), false);
        QCOMPARE(args.value(2).toString(), QStringLiteral("gsettings-unavailable"));
        QCOMPARE(binding.lastError(), QStringLiteral("gsettings-unavailable"));
        QCOMPARE(errorSpy.count(), 1);
    }

    void dbusInvalidBinding_reportsContractError()
    {
        DBusBinding binding(QString(),
                            QString(),
                            QString(),
                            QString(),
                            DBusBinding::Mode::Property,
                            QDBusConnection::sessionBus());
        QSignalSpy finishedSpy(&binding, SIGNAL(operationFinished(QString,bool,QString)));
        QSignalSpy errorSpy(&binding, SIGNAL(errorOccurred(QString)));

        binding.setValue(true);

        QCOMPARE(finishedSpy.count(), 1);
        const QList<QVariant> args = finishedSpy.takeFirst();
        QCOMPARE(args.value(0).toString(), QStringLiteral("write"));
        QCOMPARE(args.value(1).toBool(), false);
        QCOMPARE(args.value(2).toString(), QStringLiteral("invalid-dbus-binding"));
        QCOMPARE(binding.lastError(), QStringLiteral("invalid-dbus-binding"));
        QCOMPARE(errorSpy.count(), 1);
    }

    void factoryUnsupportedScheme_fallsBackToMockBinding()
    {
        SettingBindingFactory factory;
        std::unique_ptr<SettingBinding> binding(
            factory.create(QStringLiteral("unsupported:foo/bar"), QVariant(10), nullptr));

        auto *mock = qobject_cast<MockBinding *>(binding.get());
        QVERIFY(mock != nullptr);
        QCOMPARE(binding->value().toInt(), 10);

        QSignalSpy finishedSpy(binding.get(), SIGNAL(operationFinished(QString,bool,QString)));
        binding->setValue(22);
        QCOMPARE(binding->value().toInt(), 22);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.takeFirst().value(1).toBool(), true);
    }
};

QTEST_GUILESS_MAIN(SettingsBackendFallbackTest)
#include "settings_backend_fallback_test.moc"
