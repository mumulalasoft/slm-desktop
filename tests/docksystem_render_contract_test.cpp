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

class DockSystemRenderContractTest : public QObject
{
    Q_OBJECT

private slots:
    void dockSystem_hasSinglePipelinePhaseContract()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/DockSystem.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("phase: \"idle\"")));
        QVERIFY(text.contains(QStringLiteral("enteringLaunchpad")));
        QVERIFY(text.contains(QStringLiteral("launchpadActive")));
        QVERIFY(text.contains(QStringLiteral("leavingLaunchpad")));
    }

    void dockRenderer_usesGlobalResolvers()
    {
        const QString path = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/dock/Dock.qml");
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(path)));

        QVERIFY(text.contains(QStringLiteral("DockSystem.resolveNearestPinnedModelIndex")));
        QVERIFY(text.contains(QStringLiteral("DockSystem.resolveInsertionPinnedPos")));
        QVERIFY(text.contains(QStringLiteral("DockSystem.resolveMagnificationInfluence")));
        QVERIFY(text.contains(QStringLiteral("signal iconRectsChanged")));
    }

    void dockHosts_publishRectsAndUseSharedLayout()
    {
        const QString base = QStringLiteral(DESKTOP_SOURCE_DIR);
        const QString shellHostPath = base + QStringLiteral("/Qml/components/overlay/ShellDockHost.qml");
        const QString launchpadHostPath = base + QStringLiteral("/Qml/components/overlay/LaunchpadDockHost.qml");
        const QString shellHost = readTextFile(shellHostPath);
        const QString launchpadHost = readTextFile(launchpadHostPath);
        QVERIFY2(!shellHost.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(shellHostPath)));
        QVERIFY2(!launchpadHost.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(launchpadHostPath)));

        QVERIFY(shellHost.contains(QStringLiteral("onIconRectsChanged: DockSystem.updateHostIconRects")));
        QVERIFY(launchpadHost.contains(QStringLiteral("onIconRectsChanged: DockSystem.updateHostIconRects")));
        QVERIFY(shellHost.contains(QStringLiteral("DockSystem.dockLayoutState")));
        QVERIFY(launchpadHost.contains(QStringLiteral("DockSystem.dockLayoutState")));
    }
};

QTEST_MAIN(DockSystemRenderContractTest)
#include "docksystem_render_contract_test.moc"

