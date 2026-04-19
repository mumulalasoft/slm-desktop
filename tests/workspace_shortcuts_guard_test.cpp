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

class WorkspaceShortcutsGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void desktopScene_routesToBackendFirst()
    {
        const QString scenePath =
            QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/DesktopScene.qml");
        const QString scene = readTextFile(scenePath);
        QVERIFY2(!scene.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(scenePath)));

        QVERIFY(scene.contains(QStringLiteral("function switchWorkspaceByDelta(delta)")));
        QVERIFY(scene.contains(QStringLiteral("WorkspaceManager.SwitchWorkspaceByDelta")));
        QVERIFY(scene.contains(QStringLiteral("function moveFocusedWindowByDelta(delta)")));
        QVERIFY(scene.contains(QStringLiteral("WorkspaceManager.MoveFocusedWindowByDelta")));
    }

    void mainQml_shortcuts_areRegistered()
    {
        const QString mainPath =
            QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Main.qml");
        const QString mainQml = readTextFile(mainPath);
        QVERIFY2(!mainQml.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(mainPath)));

        QVERIFY(mainQml.contains(QStringLiteral("sequence: \"Escape\"")));
        QVERIFY(mainQml.contains(QStringLiteral("GlobalShortcutManager")));
    }

    void globalShortcutManager_shortcuts_areRegistered()
    {
        const QString managerPath =
            QStringLiteral(DESKTOP_SOURCE_DIR)
            + QStringLiteral("/Qml/components/shell/GlobalShortcutManager.qml");
        const QString managerQml = readTextFile(managerPath);
        QVERIFY2(!managerQml.isEmpty(),
                 qPrintable(QStringLiteral("failed to read %1").arg(managerPath)));

        QVERIFY(managerQml.contains(QStringLiteral("workspaceOverviewBind")));
        QVERIFY(managerQml.contains(QStringLiteral("workspacePrevBind")));
        QVERIFY(managerQml.contains(QStringLiteral("workspaceNextBind")));
        QVERIFY(managerQml.contains(QStringLiteral("\"Ctrl+Meta+Left\"")));
        QVERIFY(managerQml.contains(QStringLiteral("\"Ctrl+Meta+Right\"")));
        QVERIFY(managerQml.contains(QStringLiteral("\"Meta+S\"")));
        QVERIFY(managerQml.contains(QStringLiteral("\"Meta+Left\"")));
        QVERIFY(managerQml.contains(QStringLiteral("\"Meta+Right\"")));
    }
};

QTEST_MAIN(WorkspaceShortcutsGuardTest)
#include "workspace_shortcuts_guard_test.moc"

