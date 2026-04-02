#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "../src/apps/settings/moduleloader.h"

namespace {

void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(data);
    }
}

}

class SettingsModuleLoaderContractTest : public QObject
{
    Q_OBJECT

private slots:
    void scanModules_skipsInvalidModuleAndSetting()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString invalidModuleDir = tmp.path() + QStringLiteral("/invalid-module");
        const QString validModuleDir = tmp.path() + QStringLiteral("/valid-module");
        QDir().mkpath(invalidModuleDir);
        QDir().mkpath(validModuleDir);

        writeFile(invalidModuleDir + QStringLiteral("/metadata.json"), R"json(
{
  "name": "Missing Id",
  "main": "Invalid.qml",
  "settings": []
}
)json");
        writeFile(invalidModuleDir + QStringLiteral("/Invalid.qml"), "import QtQuick 2.15\nItem{}\n");

        writeFile(validModuleDir + QStringLiteral("/metadata.json"), R"json(
{
  "id": "contract-module",
  "name": "Contract Module",
  "group": "Testing",
  "main": "Valid.qml",
  "settings": [
    {
      "label": "No id, should be skipped",
      "backend_binding": "mock:a"
    },
    {
      "id": "wifi",
      "label": "Wi-Fi",
      "backend_binding": "dbus:org.freedesktop.NetworkManager/WirelessEnabled",
      "privileged_action": "org.slm.settings.network.modify"
    }
  ]
}
)json");
        writeFile(validModuleDir + QStringLiteral("/Valid.qml"), "import QtQuick 2.15\nItem{}\n");

        ModuleLoader loader;
        loader.scanModules(QStringList{tmp.path()});
        const QVariantMap mod = loader.moduleById(QStringLiteral("contract-module"));
        QVERIFY(!mod.isEmpty());

        const QVariantList settings = mod.value(QStringLiteral("settings")).toList();
        QCOMPARE(settings.size(), 1);
        const QVariantMap setting = settings.first().toMap();
        QCOMPARE(setting.value(QStringLiteral("id")).toString(), QStringLiteral("wifi"));
        QCOMPARE(setting.value(QStringLiteral("backendBinding")).toString(),
                 QStringLiteral("dbus:org.freedesktop.NetworkManager/WirelessEnabled"));
        QCOMPARE(setting.value(QStringLiteral("privilegedAction")).toString(),
                 QStringLiteral("org.slm.settings.network.modify"));
        QCOMPARE(setting.value(QStringLiteral("anchor")).toString(), QStringLiteral("wifi"));
        QCOMPARE(setting.value(QStringLiteral("deepLink")).toString(),
                 QStringLiteral("settings://contract-module/wifi"));
    }
};

QTEST_GUILESS_MAIN(SettingsModuleLoaderContractTest)
#include "settings_moduleloader_contract_test.moc"
