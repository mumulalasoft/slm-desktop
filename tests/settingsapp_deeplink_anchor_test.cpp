#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QStandardPaths>

#include "../src/apps/settings/settingsapp.h"

namespace {
QString writeTestModule()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString moduleDir = appData + QStringLiteral("/modules/anchor-test");
    QDir().mkpath(moduleDir);
    QFile meta(moduleDir + QStringLiteral("/metadata.json"));
    if (meta.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QByteArray json = R"json(
{
  "id": "anchor-test",
  "name": "Anchor Test",
  "icon": "preferences-system",
  "group": "Testing",
  "description": "Anchor resolver test module.",
  "keywords": ["anchor","test"],
  "main": "AnchorPage.qml",
  "settings": [
    {
      "id": "target-setting",
      "label": "Target Setting",
      "description": "Setting resolved by anchor.",
      "keywords": ["target"],
      "type": "action",
      "anchor": "target-anchor",
      "backend_binding": "mock:test"
    }
  ]
}
)json";
        meta.write(json);
    }
    QFile qml(moduleDir + QStringLiteral("/AnchorPage.qml"));
    if (qml.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qml.write("import QtQuick 2.15\nItem {}\n");
    }
    return moduleDir;
}
}

class SettingsAppDeepLinkAnchorTest : public QObject
{
    Q_OBJECT

private slots:
    void openDeepLink_fragmentAnchor_resolvesSettingId()
    {
        const QString moduleDir = writeTestModule();

        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QVERIFY(app.openDeepLink(QStringLiteral("settings://anchor-test#target-anchor")));
        QCOMPARE(app.currentModuleId(), QStringLiteral("anchor-test"));
        QCOMPARE(app.currentSettingId(), QStringLiteral("target-setting"));
        QVERIFY(app.lastDeepLinkLatencyMs() >= 0);
        QVERIFY(app.lastModuleOpenLatencyMs() >= 0);

        QDir(moduleDir).removeRecursively();
    }

    void openDeepLink_pathAnchor_resolvesSettingId()
    {
        const QString moduleDir = writeTestModule();

        QQmlApplicationEngine engine;
        SettingsApp app(&engine);
        QVERIFY(app.openDeepLink(QStringLiteral("settings://anchor-test/target-anchor")));
        QCOMPARE(app.currentModuleId(), QStringLiteral("anchor-test"));
        QCOMPARE(app.currentSettingId(), QStringLiteral("target-setting"));
        QVERIFY(app.lastDeepLinkLatencyMs() >= 0);
        QVERIFY(app.lastModuleOpenLatencyMs() >= 0);

        QDir(moduleDir).removeRecursively();
    }
};

QTEST_GUILESS_MAIN(SettingsAppDeepLinkAnchorTest)
#include "settingsapp_deeplink_anchor_test.moc"
