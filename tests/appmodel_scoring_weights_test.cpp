#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QVariantList>
#include <QVariantMap>

#include "../appmodel.h"
#include "../src/core/prefs/uipreferences.h"

namespace {
QString deriveKey(const QString &desktopId, const QString &desktopFile, const QString &executable)
{
    QString key = desktopId.trimmed().toLower();
    if (!key.isEmpty()) {
        return key;
    }
    key = QFileInfo(desktopFile.trimmed()).fileName().toLower();
    if (!key.isEmpty()) {
        return key;
    }
    QString exec = executable.trimmed();
    const int spacePos = exec.indexOf(QLatin1Char(' '));
    if (spacePos > 0) {
        exec = exec.left(spacePos);
    }
    return QFileInfo(exec).fileName().toLower();
}
}

class AppModelScoringWeightsTest : public QObject
{
    Q_OBJECT

private slots:
    void preferenceWeights_affectScoreDeterministically()
    {
        UIPreferences prefs;
        DesktopAppModel model;
        model.setUIPreferences(&prefs);
        model.refresh();

        const QVariantList firstPage = model.page(0, 1, QString());
        QVERIFY2(!firstPage.isEmpty(), "No desktop apps discovered; cannot validate scoring weights.");
        const QVariantMap app = firstPage.first().toMap();
        const QString desktopId = app.value(QStringLiteral("desktopId")).toString();
        const QString desktopFile = app.value(QStringLiteral("desktopFile")).toString();
        const QString executable = app.value(QStringLiteral("executable")).toString();

        const QString key = deriveKey(desktopId, desktopFile, executable);
        QVERIFY2(!key.isEmpty(), "Unable to derive scoring key");

        const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QVERIFY2(!dataHome.isEmpty(), "GenericDataLocation is empty");
        QDir().mkpath(dataHome);
        QFile badXbel(QDir(dataHome).filePath(QStringLiteral("recently-used.xbel")));
        QVERIFY2(badXbel.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to create malformed XBEL");
        badXbel.write("<xbel><bookmark href=\"file:///broken\"");
        badXbel.close();

        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QVERIFY2(!appData.isEmpty(), "AppDataLocation is empty");
        QDir().mkpath(appData);

        QFile cacheFile(QDir(appData).filePath(QStringLiteral("app-xbel-usage-cache.json")));
        QVERIFY2(cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write usage cache");
        QJsonObject usageRow;
        usageRow.insert(QStringLiteral("launchCount7d"), 7);
        usageRow.insert(QStringLiteral("fileOpenCount7d"), 5);
        usageRow.insert(QStringLiteral("lastLaunchMs"),
                        static_cast<double>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()));
        QJsonObject usage;
        usage.insert(key, usageRow);
        QJsonObject root;
        root.insert(QStringLiteral("usage"), usage);
        root.insert(QStringLiteral("updatedAt"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        cacheFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        cacheFile.close();

        prefs.setPreference(QStringLiteral("app.score.launchWeight"), 2);
        prefs.setPreference(QStringLiteral("app.score.fileOpenWeight"), 4);
        prefs.setPreference(QStringLiteral("app.score.recencyWeight"), 0);

        model.refresh();
        QVariantMap stats = model.appUsage(desktopId, desktopFile, executable);
        const int launchCountA = stats.value(QStringLiteral("launchCount7d")).toInt();
        const int fileOpenCountA = stats.value(QStringLiteral("fileOpenCount7d")).toInt();
        QVERIFY(launchCountA >= 7);
        QVERIFY(fileOpenCountA >= 5);
        QCOMPARE(stats.value(QStringLiteral("score")).toInt(), launchCountA * 2 + fileOpenCountA * 4);

        prefs.setPreference(QStringLiteral("app.score.launchWeight"), 10);
        prefs.setPreference(QStringLiteral("app.score.fileOpenWeight"), 0);
        prefs.setPreference(QStringLiteral("app.score.recencyWeight"), 0);

        stats = model.appUsage(desktopId, desktopFile, executable);
        const int launchCountB = stats.value(QStringLiteral("launchCount7d")).toInt();
        const int fileOpenCountB = stats.value(QStringLiteral("fileOpenCount7d")).toInt();
        QCOMPARE(launchCountB, launchCountA);
        QCOMPARE(fileOpenCountB, fileOpenCountA);
        QCOMPARE(stats.value(QStringLiteral("score")).toInt(), launchCountB * 10);
    }
};

int main(int argc, char **argv)
{
    const QString root = QDir::temp().filePath(
        QStringLiteral("slm-appmodel-scoring-%1").arg(QCoreApplication::applicationPid()));
    QDir(root).removeRecursively();
    QDir().mkpath(root);

    qputenv("XDG_DATA_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("data"))));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("config"))));
    qputenv("XDG_STATE_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("state"))));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("cache"))));

    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Desktop"));

    AppModelScoringWeightsTest tc;
    const int rc = QTest::qExec(&tc, argc, argv);
    QDir(root).removeRecursively();
    return rc;
}

#include "appmodel_scoring_weights_test.moc"
