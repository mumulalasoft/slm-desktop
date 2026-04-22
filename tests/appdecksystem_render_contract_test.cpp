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

class AppDeckSystemRenderContractTest : public QObject
{
    Q_OBJECT

private slots:
    void appDeckWindow_hasSingleStateOwnerContract()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("readonly property string appdeckState")));
        QVERIFY(text.contains(QStringLiteral("function enterCollapsedMode()")));
        QVERIFY(text.contains(QStringLiteral("function enterExpandedMode()")));
        QVERIFY(text.contains(QStringLiteral("function enterContextMode()")));
        QVERIFY(text.contains(QStringLiteral("function enterHiddenMode()")));
        QVERIFY(text.contains(QStringLiteral("AppDeckComp.AppDeckCollapsedView")));
        QVERIFY(text.contains(QStringLiteral("AppDeckComp.AppDeckExpandedView")));
        QVERIFY(text.contains(QStringLiteral("AppDeckComp.AppDeckContextView")));
    }

    void dockRenderer_usesLocalResolvers()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/appdeck/AppDeck.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("property var iconRectsCache")));
        QVERIFY(text.contains(QStringLiteral("function nearestPinnedIndex(globalX)")));
        QVERIFY(text.contains(QStringLiteral("function insertionPinnedPosFromGlobalX(globalX)")));
        QVERIFY(text.contains(QStringLiteral("root.iconRectsCache = rects")));
        QVERIFY(text.contains(QStringLiteral("signal iconRectsChanged")));
    }

    void appDeckWindow_noLongerDependsOnLegacyDockSystem()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString dockWindowPath = base + QStringLiteral("/Qml/components/overlay/AppDeckWindow.qml");
        const QString dockWindow = readTextFile(dockWindowPath);
        QVERIFY2(!dockWindow.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(dockWindowPath)));

        QVERIFY(!dockWindow.contains(QStringLiteral("AppDeckSystem.")));
        QVERIFY(dockWindow.contains(QStringLiteral("required property var pulseResultsModel")));
        QVERIFY(dockWindow.contains(QStringLiteral("readonly property var dockItem: collapsedView.dockItem")));
    }
};

QTEST_MAIN(AppDeckSystemRenderContractTest)
#include "appdecksystem_render_contract_test.moc"
