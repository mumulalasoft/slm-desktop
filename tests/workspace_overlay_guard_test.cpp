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

class WorkspaceOverlayGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void workspaceOverlay_prefersWorkspaceStripModel()
    {
        const QString overlayPath =
            QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/workspace/WorkspaceOverlay.qml");
        const QString overlay = readTextFile(overlayPath);
        QVERIFY2(!overlay.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(overlayPath)));

        QVERIFY(overlay.contains(QStringLiteral("WorkspaceStripModel.rows")));
        QVERIFY(overlay.contains(QStringLiteral("WorkspaceStripModel.switchToWorkspace")));
    }

    void workspaceOverlay_prefersBackendDeltaSwitch()
    {
        const QString overlayPath =
            QStringLiteral(DESKTOP_SOURCE_DIR) + QStringLiteral("/Qml/components/workspace/WorkspaceOverlay.qml");
        const QString overlay = readTextFile(overlayPath);
        QVERIFY2(!overlay.isEmpty(), qPrintable(QStringLiteral("failed to read %1").arg(overlayPath)));

        QVERIFY(overlay.contains(QStringLiteral("WorkspaceManager.SwitchWorkspaceByDelta")));
    }
};

QTEST_MAIN(WorkspaceOverlayGuardTest)
#include "workspace_overlay_guard_test.moc"
