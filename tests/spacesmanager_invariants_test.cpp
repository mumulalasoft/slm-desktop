#include <QtTest/QtTest>
#include <QCoreApplication>

#include "spacesmanager.h"

class SpacesManagerInvariantsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SLMTest"));
        QCoreApplication::setApplicationName(QStringLiteral("SpacesManagerInvariantsTest"));
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

    void defaults_haveSingleTrailingEmptyWorkspace()
    {
        SpacesManager manager;
        QCOMPARE(manager.spaceCount(), 1);
        QCOMPARE(manager.activeSpace(), 1);
        QVERIFY(manager.hasTrailingEmptyWorkspace());

        const QVariantList model = manager.workspaceModel();
        QCOMPARE(model.size(), 1);
        const QVariantMap first = model.first().toMap();
        QCOMPARE(first.value(QStringLiteral("id")).toInt(), 1);
        QVERIFY(first.value(QStringLiteral("isActive")).toBool());
        QVERIFY(first.value(QStringLiteral("isEmpty")).toBool());
        QVERIFY(first.value(QStringLiteral("isTrailingEmpty")).toBool());
    }

    void assigningIntoLastWorkspace_appendsTrailingWorkspace()
    {
        SpacesManager manager;
        QSignalSpy countSpy(&manager, &SpacesManager::spaceCountChanged);
        QSignalSpy workspacesSpy(&manager, &SpacesManager::workspacesChanged);

        manager.assignWindowToSpace(QStringLiteral("view-1"), 1);

        QCOMPARE(manager.windowSpace(QStringLiteral("view-1")), 1);
        QCOMPARE(manager.spaceCount(), 2);
        QVERIFY(manager.hasTrailingEmptyWorkspace());
        QVERIFY(!countSpy.isEmpty());
        QVERIFY(!workspacesSpy.isEmpty());
    }

    void sparseAssignments_areCompressedAndActiveRemainsValid()
    {
        SpacesManager manager;
        manager.assignWindowToSpace(QStringLiteral("view-1"), 1);
        QCOMPARE(manager.spaceCount(), 2);

        // Moving the only window to workspace 2 creates sparse state [1:empty, 2:occupied].
        // Invariant pass must remap occupancy to keep no empty non-last workspaces.
        manager.assignWindowToSpace(QStringLiteral("view-1"), 2);

        QCOMPARE(manager.spaceCount(), 2);
        QCOMPARE(manager.windowSpace(QStringLiteral("view-1")), 1);
        QVERIFY(manager.hasTrailingEmptyWorkspace());

        const QVariantMap state = manager.invariantState();
        QVERIFY(state.value(QStringLiteral("validActiveSpace")).toBool());
        QVERIFY(!state.value(QStringLiteral("hasEmptyNonLastWorkspace")).toBool());
    }

    void enforceInvariants_returnsFalseWhenAlreadyStable()
    {
        SpacesManager manager;
        manager.assignWindowToSpace(QStringLiteral("view-1"), 1);
        QVERIFY(!manager.enforceInvariants());
    }

    void clearMissingAssignments_keepsActiveWorkspaceValid()
    {
        SpacesManager manager;
        manager.assignWindowToSpace(QStringLiteral("view-1"), 1);
        manager.setActiveSpace(2);
        QCOMPARE(manager.activeSpace(), 2);

        manager.clearMissingAssignments(QVariantList{});

        QCOMPARE(manager.spaceCount(), 1);
        QCOMPARE(manager.activeSpace(), 1);
        QVERIFY(manager.hasTrailingEmptyWorkspace());
        const QVariantMap state = manager.invariantState();
        QVERIFY(state.value(QStringLiteral("validActiveSpace")).toBool());
    }

    void activeWorkspace_isAlwaysClampedToValidRange()
    {
        SpacesManager manager;
        QVERIFY(manager.isActiveSpaceValid());

        manager.setSpaceCount(0);
        QCOMPARE(manager.spaceCount(), 1);
        QCOMPARE(manager.activeSpace(), 1);
        QVERIFY(manager.isActiveSpaceValid());

        manager.setActiveSpace(999);
        QCOMPARE(manager.activeSpace(), 1);
        QVERIFY(manager.isActiveSpaceValid());

        manager.assignWindowToSpace(QStringLiteral("view-1"), 1);
        QCOMPARE(manager.spaceCount(), 2);
        manager.setActiveSpace(2);
        QVERIFY(manager.isActiveSpaceValid());

        manager.clearMissingAssignments(QVariantList{});
        QCOMPARE(manager.spaceCount(), 1);
        QCOMPARE(manager.activeSpace(), 1);
        QVERIFY(manager.isActiveSpaceValid());
    }
};

QTEST_MAIN(SpacesManagerInvariantsTest)

#include "spacesmanager_invariants_test.moc"
