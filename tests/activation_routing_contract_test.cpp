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

class ActivationRoutingContractTest : public QObject
{
    Q_OBJECT

private slots:
    void dockAndWorkspaceMustRouteViaAppCommandRouter()
    {
        const QString dockPath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/dock/Dock.qml");
        const QString dockDelegatePath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/dock/DockAppDelegate.qml");
        const QString workspacePath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/workspace/WorkspaceOverlay.qml");

        const QString dockText = readTextFile(dockPath);
        const QString dockDelegateText = readTextFile(dockDelegatePath);
        const QString workspaceText = readTextFile(workspacePath);

        QVERIFY2(!dockText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(dockPath)));
        QVERIFY2(!dockDelegateText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(dockDelegatePath)));
        QVERIFY2(!workspaceText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(workspacePath)));

        QVERIFY(dockText.contains(QStringLiteral("routeWithResult(\"workspace.presentview\"")));
        QVERIFY(dockDelegateText.contains(QStringLiteral("routeWithResult(\"workspace.presentview\"")));
        QVERIFY(workspaceText.contains(QStringLiteral("routeWithResult(\"workspace.presentview\"")));
    }

    void routerMustExposeWorkspacePresentViewAction()
    {
        const QString routerHeaderPath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/src/core/execution/appcommandrouter.h");
        const QString routerCppPath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/src/core/execution/appcommandrouter.cpp");
        const QString mainPath = QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/main.cpp");

        const QString routerHeaderText = readTextFile(routerHeaderPath);
        const QString routerCppText = readTextFile(routerCppPath);
        const QString mainText = readTextFile(mainPath);

        QVERIFY2(!routerHeaderText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(routerHeaderPath)));
        QVERIFY2(!routerCppText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(routerCppPath)));
        QVERIFY2(!mainText.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(mainPath)));

        QVERIFY(routerHeaderText.contains(QStringLiteral("WorkspaceManager *workspaceManager = nullptr")));
        QVERIFY(routerHeaderText.contains(QStringLiteral("WorkspaceManager *m_workspaceManager = nullptr")));
        QVERIFY(routerCppText.contains(QStringLiteral("QStringLiteral(\"workspace.presentview\")")));
        QVERIFY(routerCppText.contains(QStringLiteral("missing-viewid")));
        QVERIFY(routerCppText.contains(QStringLiteral("workspace-manager-unavailable")));
        QVERIFY(mainText.contains(QStringLiteral("&workspaceManager")));
    }
};

QTEST_MAIN(ActivationRoutingContractTest)
#include "activation_routing_contract_test.moc"
