#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "../src/apps/settings/moduleloader.h"
#include "../src/apps/settings/searchengine.h"

namespace {

void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(data);
    }
}

QVariantMap firstSettingResultForModule(const QVariantList &results, const QString &moduleId)
{
    for (const QVariant &v : results) {
        const QVariantMap row = v.toMap();
        if (row.value(QStringLiteral("type")).toString() != QStringLiteral("setting")) {
            continue;
        }
        if (row.value(QStringLiteral("moduleId")).toString() != moduleId) {
            continue;
        }
        return row;
    }
    return {};
}

QStringList settingOrderForModule(const QVariantList &results, const QString &moduleId)
{
    QStringList out;
    for (const QVariant &v : results) {
        const QVariantMap row = v.toMap();
        if (row.value(QStringLiteral("type")).toString() != QStringLiteral("setting")) {
            continue;
        }
        if (row.value(QStringLiteral("moduleId")).toString() != moduleId) {
            continue;
        }
        out.push_back(row.value(QStringLiteral("settingId")).toString());
    }
    return out;
}

} // namespace

class SettingsSearchEngineContextRankingTest : public QObject
{
    Q_OBJECT

private slots:
    void multiToken_rawJson_prioritizesContextRawEntry()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString moduleDir = tmp.path() + QStringLiteral("/developer");
        QDir().mkpath(moduleDir);
        writeFile(moduleDir + QStringLiteral("/DeveloperPage.qml"), "import QtQuick 2.15\nItem{}\n");
        writeFile(moduleDir + QStringLiteral("/metadata.json"), R"json(
{
  "id": "developer",
  "name": "Developer",
  "icon": "utilities-terminal",
  "group": "System",
  "description": "Developer tools.",
  "keywords": ["developer", "debug", "environment"],
  "main": "DeveloperPage.qml",
  "settings": [
    {
      "id": "context-debug-runtime",
      "label": "Context Runtime State",
      "description": "Inspect context service status and sensor source summary.",
      "keywords": ["context", "runtime", "sensor", "source", "actions"]
    },
    {
      "id": "context-debug-raw",
      "label": "Context Raw JSON",
      "description": "Inspect full normalized context payload for debugging.",
      "keywords": ["context", "raw", "json", "payload", "debug"]
    },
    {
      "id": "env-variables",
      "label": "Environment Variables",
      "description": "Manage environment variables.",
      "keywords": ["env", "path", "variables"]
    }
  ]
}
)json");

        ModuleLoader loader;
        loader.scanModules(QStringList{tmp.path()});
        SearchEngine engine(&loader);

        const QVariantList rows = engine.query(QStringLiteral("raw json context"), 10);
        QVERIFY(!rows.isEmpty());

        const QVariantMap top = firstSettingResultForModule(rows, QStringLiteral("developer"));
        QVERIFY(!top.isEmpty());
        QCOMPARE(top.value(QStringLiteral("settingId")).toString(), QStringLiteral("context-debug-raw"));
    }

    void multiToken_sensorSource_prioritizesRuntimeEntry()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString moduleDir = tmp.path() + QStringLiteral("/developer");
        QDir().mkpath(moduleDir);
        writeFile(moduleDir + QStringLiteral("/DeveloperPage.qml"), "import QtQuick 2.15\nItem{}\n");
        writeFile(moduleDir + QStringLiteral("/metadata.json"), R"json(
{
  "id": "developer",
  "name": "Developer",
  "icon": "utilities-terminal",
  "group": "System",
  "description": "Developer tools.",
  "keywords": ["developer", "debug", "environment"],
  "main": "DeveloperPage.qml",
  "settings": [
    {
      "id": "context-debug-runtime",
      "label": "Context Runtime State",
      "description": "Inspect context service status and sensor source summary.",
      "keywords": ["context", "runtime", "sensor", "source", "actions"]
    },
    {
      "id": "context-debug-raw",
      "label": "Context Raw JSON",
      "description": "Inspect full normalized context payload for debugging.",
      "keywords": ["context", "raw", "json", "payload", "debug"]
    },
    {
      "id": "build-info",
      "label": "Build & Runtime Info",
      "description": "Qt version, compiler and runtime details.",
      "keywords": ["build", "runtime", "system"]
    }
  ]
}
)json");

        ModuleLoader loader;
        loader.scanModules(QStringList{tmp.path()});
        SearchEngine engine(&loader);

        const QVariantList rows = engine.query(QStringLiteral("sensor source context"), 10);
        QVERIFY(!rows.isEmpty());

        const QVariantMap top = firstSettingResultForModule(rows, QStringLiteral("developer"));
        QVERIFY(!top.isEmpty());
        QCOMPARE(top.value(QStringLiteral("settingId")).toString(), QStringLiteral("context-debug-runtime"));
    }

    void multiToken_adaptiveActions_prioritizesRuntimeEntry()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString moduleDir = tmp.path() + QStringLiteral("/developer");
        QDir().mkpath(moduleDir);
        writeFile(moduleDir + QStringLiteral("/DeveloperPage.qml"), "import QtQuick 2.15\nItem{}\n");
        writeFile(moduleDir + QStringLiteral("/metadata.json"), R"json(
{
  "id": "developer",
  "name": "Developer",
  "icon": "utilities-terminal",
  "group": "System",
  "description": "Developer tools.",
  "keywords": ["developer", "debug", "environment"],
  "main": "DeveloperPage.qml",
  "settings": [
    {
      "id": "context-debug",
      "label": "Context Debug",
      "description": "Inspect live normalized context and adaptive behavior.",
      "keywords": ["context", "adaptive", "automation", "debug"]
    },
    {
      "id": "context-debug-runtime",
      "label": "Context Runtime State",
      "description": "Inspect context service status and resolved runtime actions.",
      "keywords": ["context", "runtime", "adaptive actions", "resolved actions", "sensor"]
    },
    {
      "id": "context-debug-raw",
      "label": "Context Raw JSON",
      "description": "Inspect full normalized context payload for debugging.",
      "keywords": ["context", "raw", "json", "payload"]
    }
  ]
}
)json");

        ModuleLoader loader;
        loader.scanModules(QStringList{tmp.path()});
        SearchEngine engine(&loader);

        const QVariantList rows = engine.query(QStringLiteral("adaptive actions"), 10);
        QVERIFY(!rows.isEmpty());

        const QVariantMap top = firstSettingResultForModule(rows, QStringLiteral("developer"));
        QVERIFY(!top.isEmpty());
        QCOMPARE(top.value(QStringLiteral("settingId")).toString(), QStringLiteral("context-debug-runtime"));
    }

    void mixedQuery_rankingLocksTop3Order()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString moduleDir = tmp.path() + QStringLiteral("/developer");
        QDir().mkpath(moduleDir);
        writeFile(moduleDir + QStringLiteral("/DeveloperPage.qml"), "import QtQuick 2.15\nItem{}\n");
        writeFile(moduleDir + QStringLiteral("/metadata.json"), R"json(
{
  "id": "developer",
  "name": "Developer",
  "icon": "utilities-terminal",
  "group": "System",
  "description": "Developer tools.",
  "keywords": ["developer", "debug", "environment"],
  "main": "DeveloperPage.qml",
  "settings": [
    {
      "id": "context-debug",
      "label": "Context Debug",
      "description": "Inspect live normalized context and adaptive behavior.",
      "keywords": ["context", "adaptive", "automation", "debug"]
    },
    {
      "id": "context-debug-runtime",
      "label": "Context Runtime State",
      "description": "Inspect context service status and resolved runtime actions.",
      "keywords": ["context", "runtime", "adaptive actions", "resolved actions", "sensor source"]
    },
    {
      "id": "context-debug-raw",
      "label": "Context Raw JSON",
      "description": "Inspect full normalized context payload for debugging.",
      "keywords": ["context", "raw", "json", "payload"]
    }
  ]
}
)json");

        ModuleLoader loader;
        loader.scanModules(QStringList{tmp.path()});
        SearchEngine engine(&loader);

        const QVariantList rows = engine.query(QStringLiteral("context runtime raw json sensor source adaptive actions"), 20);
        const QStringList order = settingOrderForModule(rows, QStringLiteral("developer"));
        QVERIFY(order.size() >= 3);

        QCOMPARE(order.at(0), QStringLiteral("context-debug-runtime"));
        QCOMPARE(order.at(1), QStringLiteral("context-debug-raw"));
        QCOMPARE(order.at(2), QStringLiteral("context-debug"));
    }
};

QTEST_GUILESS_MAIN(SettingsSearchEngineContextRankingTest)
#include "settings_searchengine_context_ranking_test.moc"
