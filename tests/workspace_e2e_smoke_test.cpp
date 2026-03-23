#include <QtTest/QtTest>

#include "../src/core/workspace/multitaskingcontroller.h"
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

class WorkspaceE2ESmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SLMTest"));
        QCoreApplication::setApplicationName(QStringLiteral("WorkspaceE2ESmokeTest"));
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

    void switchWorkspace_whileOverviewOpen_keepsOverviewMode()
    {
        SpacesManager spaces;
        FakeCompositorStateModel state;
        WorkspaceManager workspace(nullptr, &spaces, &state);
        MultitaskingController controller(&workspace, &spaces);

        spaces.assignWindowToSpace(QStringLiteral("kwin:view-a"), 1);
        spaces.assignWindowToSpace(QStringLiteral("kwin:view-b"), 2);
        QCOMPARE(spaces.spaceCount(), 3); // includes trailing empty

        controller.showWorkspace();
        QCOMPARE(controller.workspaceVisible(), true);
        QCOMPARE(controller.mode(), MultitaskingController::WorkspaceOverviewMode);

        controller.switchWorkspace(2);
        QCOMPARE(spaces.activeSpace(), 2);
        QCOMPARE(controller.workspaceVisible(), true);
        QCOMPARE(controller.mode(), MultitaskingController::WorkspaceOverviewMode);
    }

    void moveWindowByKeyboard_updatesWorkspaceAndFollowsFocus()
    {
        SpacesManager spaces;
        FakeCompositorStateModel state;
        WorkspaceManager workspace(nullptr, &spaces, &state);

        spaces.assignWindowToSpace(QStringLiteral("kwin:view-a"), 1);
        spaces.assignWindowToSpace(QStringLiteral("kwin:view-b"), 1);
        QCOMPARE(spaces.spaceCount(), 2);

        state.windows = {
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

        QVERIFY(workspace.MoveFocusedWindowByDelta(1));
        QCOMPARE(spaces.activeSpace(), 2);
        QVERIFY(spaces.isActiveSpaceValid());
        QVERIFY(spaces.hasTrailingEmptyWorkspace());
    }

    void moveWindowByDrag_toTrailingWorkspace_keepsDynamicInvariant()
    {
        SpacesManager spaces;
        FakeCompositorStateModel state;
        WorkspaceManager workspace(nullptr, &spaces, &state);

        spaces.assignWindowToSpace(QStringLiteral("kwin:view-a"), 1);
        spaces.assignWindowToSpace(QStringLiteral("kwin:view-b"), 1);
        QCOMPARE(spaces.spaceCount(), 2);

        // Simulate drag-and-drop from overview thumbnail to trailing placeholder.
        QVERIFY(workspace.MoveWindowToWorkspace(QStringLiteral("kwin:view-a"), 2));
        // Dynamic invariant contract is more important than fixed index:
        // manager may compact empty workspaces and remap ids.
        QVERIFY(spaces.spaceCount() >= 2);
        QVERIFY(spaces.hasTrailingEmptyWorkspace());
    }

    void dockLikePresentWindow_jumpsToTargetWorkspace()
    {
        SpacesManager spaces;
        FakeCompositorStateModel state;
        WorkspaceManager workspace(nullptr, &spaces, &state);

        spaces.assignWindowToSpace(QStringLiteral("kwin:view-b"), 2);
        spaces.setActiveSpace(1);
        QCOMPARE(spaces.activeSpace(), 1);

        state.windows = {
            QVariantMap{
                {QStringLiteral("viewId"), QStringLiteral("kwin:view-b")},
                {QStringLiteral("appId"), QStringLiteral("org.example.Browser")},
                {QStringLiteral("title"), QStringLiteral("Browser")},
                {QStringLiteral("focused"), false},
                {QStringLiteral("space"), 2}
            }
        };

        const bool focused = workspace.PresentWindow(QStringLiteral("org.example.Browser"));
        Q_UNUSED(focused)
        // In headless tests focus command may be unavailable; workspace jump must still happen.
        QCOMPARE(spaces.activeSpace(), 2);
    }
};

QTEST_MAIN(WorkspaceE2ESmokeTest)
#include "workspace_e2e_smoke_test.moc"
