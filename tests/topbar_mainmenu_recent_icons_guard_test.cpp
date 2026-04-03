#include <QtTest/QtTest>

#include <QFile>

namespace {
QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}
}

class TopBarMainMenuRecentIconsGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void recentSubmenus_presentAndIconBound()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/topbar/TopBarMainMenuControl.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("title: qsTr(\"Recent Applications\")")));
        QVERIFY(text.contains(QStringLiteral("title: qsTr(\"Recent Files\")")));
        QVERIFY(text.contains(QStringLiteral("enabled: recentAppsInstantiator.count > 0")));
        QVERIFY(text.contains(QStringLiteral("enabled: recentFilesInstantiator.count > 0")));
        QVERIFY(text.contains(QStringLiteral("delegate: DSStyle.MenuItem")));
        QVERIFY(text.contains(QStringLiteral("iconSource: String(entry.iconSource || \"\")")));
        QVERIFY(text.contains(QStringLiteral("fallbackIconSource: \"qrc:/icons/launchpad.svg\"")));
        QVERIFY(text.contains(QStringLiteral("fallbackIconSource: root._themeIconSource(\"text-x-generic\")")));
    }

    void recentFiles_fallbackChain_present()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/Qml/components/topbar/TopBarMainMenuControl.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("var iconName = String(f.iconName || \"\").trim()")));
        QVERIFY(text.contains(QStringLiteral("var mimeType = String(f.mimeType || \"\").trim()")));
        QVERIFY(text.contains(QStringLiteral("if (iconName.length > 0)")));
        QVERIFY(text.contains(QStringLiteral("if (iconSource.length === 0 && mimeType.length > 0)")));
        QVERIFY(text.contains(QStringLiteral("root._mimeIconForMimeType(mimeType)")));
        QVERIFY(text.contains(QStringLiteral("if (iconSource.length === 0)")));
        QVERIFY(text.contains(QStringLiteral("root._mimeIconForPath(uri)")));
    }

    void filemanagerRecent_backendProvidesMimeAndIconName()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR)
                + QStringLiteral("/src/apps/filemanager/filemanagerapi_recent.cpp");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("#include <QMimeDatabase>")));
        QVERIFY(text.contains(QStringLiteral("row.insert(QStringLiteral(\"mimeType\")")));
        QVERIFY(text.contains(QStringLiteral("row.insert(QStringLiteral(\"iconName\")")));
        QVERIFY(text.contains(QStringLiteral("QMimeDatabase().mimeTypeForFile")));
    }
};

QTEST_MAIN(TopBarMainMenuRecentIconsGuardTest)
#include "topbar_mainmenu_recent_icons_guard_test.moc"
