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

class AppModelXbelFallbackTest : public QObject
{
    Q_OBJECT

private slots:
    void malformedXbel_usesCachedUsageForScoring()
    {
        DesktopAppModel model;
        model.refresh();

        const QVariantList firstPage = model.page(0, 1, QString());
        QVERIFY2(!firstPage.isEmpty(), "No desktop apps discovered; cannot validate scoring fallback.");
        const QVariantMap app = firstPage.first().toMap();
        const QString desktopId = app.value(QStringLiteral("desktopId")).toString();
        const QString desktopFile = app.value(QStringLiteral("desktopFile")).toString();
        const QString executable = app.value(QStringLiteral("executable")).toString();
        QVERIFY(!desktopId.trimmed().isEmpty() ||
                !desktopFile.trimmed().isEmpty() ||
                !executable.trimmed().isEmpty());

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
        QVERIFY2(cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Failed to write XBEL usage cache");

        QString key = desktopId.trimmed().toLower();
        if (key.isEmpty()) {
            const QString desktopBase = QFileInfo(desktopFile).fileName().toLower();
            key = desktopBase;
        }
        if (key.isEmpty()) {
            QString exec = executable.trimmed();
            const int spacePos = exec.indexOf(QLatin1Char(' '));
            if (spacePos > 0) {
                exec = exec.left(spacePos);
            }
            key = QFileInfo(exec).fileName().toLower();
        }
        QVERIFY2(!key.isEmpty(), "Unable to derive cache key for chosen app");

        QJsonObject usageRow;
        usageRow.insert(QStringLiteral("launchCount7d"), 9);
        usageRow.insert(QStringLiteral("fileOpenCount7d"), 11);
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

        model.refresh();

        const QVariantMap stats = model.appUsage(desktopId, desktopFile, executable);
        QVERIFY(stats.value(QStringLiteral("score")).toInt() > 0);
        QVERIFY(stats.value(QStringLiteral("fileOpenCount7d")).toInt() >= 11);
    }
};

int main(int argc, char **argv)
{
    const QString root = QDir::temp().filePath(
        QStringLiteral("slm-appmodel-xbel-fallback-%1").arg(QCoreApplication::applicationPid()));
    QDir(root).removeRecursively();
    QDir().mkpath(root);

    qputenv("XDG_DATA_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("data"))));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("config"))));
    qputenv("XDG_STATE_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("state"))));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(QDir(root).filePath(QStringLiteral("cache"))));

    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Desktop"));

    AppModelXbelFallbackTest tc;
    const int rc = QTest::qExec(&tc, argc, argv);
    QDir(root).removeRecursively();
    return rc;
}

#include "appmodel_xbel_fallback_test.moc"
