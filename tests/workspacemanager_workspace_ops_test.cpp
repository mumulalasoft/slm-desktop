#include <QtTest/QtTest>

#include "../src/core/workspace/spacesmanager.h"
#include "../src/core/workspace/workspacemanager.h"

class FakeCompositorStateModel : public QObject
{
    Q_OBJECT

public:
    QVector<QVariantMap> windows;

    Q_INVOKABLE int windowCount() const
    {
        return windows.size();
    }

    Q_INVOKABLE QVariantMap windowAt(int index) const
    {
        if (index < 0 || index >= windows.size()) {
            return {};
        }
        return windows.at(index);
    }
};

class WorkspaceManagerWorkspaceOpsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SLMTest"));
        QCoreApplication::setApplicationName(QStringLiteral("WorkspaceManagerWorkspaceOpsTest"));
    }

    void init()
    {
        QSettings settings;
        settings.remove(QStringLiteral("spaces"));
        settings.sync();
    }

    void cleanup()
    {
        QSettings settings;
        settings.remove(QStringLiteral("spaces"));
        settings.sync();
    }

    void switchWorkspaceByDelta_clampsToValidRange()
    {
        SpacesManager spaces;
        FakeCompositorStateModel state;
        WorkspaceManager manager(nullptr, &spaces, &state);

        spaces.assignWindowToSpace(QStringLiteral("kwin:view-a"), 1);
        QCOMPARE(spaces.spaceCount(), 2);
        QCOMPARE(spaces.activeSpace(), 1);

        manager.SwitchWorkspaceByDelta(1);
        QCOMPARE(spaces.activeSpace(), 2);

        manager.SwitchWorkspaceByDelta(1);
        QCOMPARE(spaces.activeSpace(), 2);

        manager.SwitchWorkspaceByDelta(-99);
        QCOMPARE(spaces.activeSpace(), 1);
    }

    void moveFocusedWindowByDelta_movesUserWindowAndSwitchesWorkspace()
    {
        SpacesManager spaces;
        FakeCompositorStateModel state;
        WorkspaceManager manager(nullptr, &spaces, &state);

        spaces.assignWindowToSpace(QStringLiteral("kwin:view-a"), 1);
        spaces.assignWindowToSpace(QStringLiteral("kwin:view-b"), 1);
        QCOMPARE(spaces.spaceCount(), 2);

        state.windows = {
            QVariantMap{
                {QStringLiteral("viewId"), QStringLiteral("kwin:shell")},
                {QStringLiteral("appId"), QStringLiteral("Slm_Desktop")},
                {QStringLiteral("title"), QStringLiteral("Slm Desktop")},
                {QStringLiteral("focused"), true}
            },
            QVariantMap{
                {QStringLiteral("viewId"), QStringLiteral("kwin:view-a")},
                {QStringLiteral("appId"), QStringLiteral("org.example.Editor")},
                {QStringLiteral("title"), QStringLiteral("Editor")},
                {QStringLiteral("focused"), true}
            },
            QVariantMap{
                {QStringLiteral("viewId"), QStringLiteral("kwin:view-b")},
                {QStringLiteral("appId"), QStringLiteral("org.example.Terminal")},
                {QStringLiteral("title"), QStringLiteral("Terminal")},
                {QStringLiteral("focused"), false}
            }
        };

        QVERIFY(manager.MoveFocusedWindowByDelta(1));
        QCOMPARE(spaces.windowSpace(QStringLiteral("kwin:view-a")), 2);
        QCOMPARE(spaces.windowSpace(QStringLiteral("kwin:view-b")), 1);
        QCOMPARE(spaces.activeSpace(), 2);
    }

    void moveFocusedWindowByDelta_returnsFalseWhenNoFocusedUserWindow()
    {
        SpacesManager spaces;
        FakeCompositorStateModel state;
        WorkspaceManager manager(nullptr, &spaces, &state);

        spaces.assignWindowToSpace(QStringLiteral("kwin:view-a"), 1);

        state.windows = {
            QVariantMap{
                {QStringLiteral("viewId"), QStringLiteral("kwin:shell")},
                {QStringLiteral("appId"), QStringLiteral("Slm_Desktop")},
                {QStringLiteral("title"), QStringLiteral("Slm Desktop")},
                {QStringLiteral("focused"), true}
            }
        };

        QVERIFY(!manager.MoveFocusedWindowByDelta(1));
        QCOMPARE(spaces.windowSpace(QStringLiteral("kwin:view-a")), 1);
        QCOMPARE(spaces.activeSpace(), 1);
    }
};

QTEST_MAIN(WorkspaceManagerWorkspaceOpsTest)
#include "workspacemanager_workspace_ops_test.moc"

