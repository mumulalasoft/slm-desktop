#include <QtTest>

#include "../tothespotcontextmenuhelper.h"

class TothespotContextMenuHelperTest : public QObject
{
    Q_OBJECT

private slots:
    void actionLayoutByEntryType();
    void actionForSlotMapping();
    void selectionNormalizeAndWrap();
};

void TothespotContextMenuHelperTest::actionLayoutByEntryType()
{
    TothespotContextMenuHelper helper;

    // app => launch + properties
    QCOMPARE(helper.hasContainingAction(true, false, true), false);
    QCOMPARE(helper.hasCopyPathAction(true, false, true), false);
    QCOMPARE(helper.visibleActionCount(true, false, true), 2);

    // folder => open + copyPath + copyName + copyUri + properties
    QCOMPARE(helper.hasContainingAction(false, true, true), false);
    QCOMPARE(helper.hasCopyPathAction(false, true, true), true);
    QCOMPARE(helper.visibleActionCount(false, true, true), 5);

    // file => open + openContainingFolder + copyPath + copyName + copyUri + properties
    QCOMPARE(helper.hasContainingAction(false, false, true), true);
    QCOMPARE(helper.hasCopyPathAction(false, false, true), true);
    QCOMPARE(helper.visibleActionCount(false, false, true), 6);
}

void TothespotContextMenuHelperTest::actionForSlotMapping()
{
    TothespotContextMenuHelper helper;

    QCOMPARE(helper.actionForSlot(0, true, false, true), QStringLiteral("launch"));
    QCOMPARE(helper.actionForSlot(1, true, false, true), QStringLiteral("properties"));
    QCOMPARE(helper.actionForSlot(9, true, false, true), QStringLiteral("properties"));

    QCOMPARE(helper.actionForSlot(0, false, true, true), QStringLiteral("open"));
    QCOMPARE(helper.actionForSlot(1, false, true, true), QStringLiteral("copyPath"));
    QCOMPARE(helper.actionForSlot(2, false, true, true), QStringLiteral("copyName"));
    QCOMPARE(helper.actionForSlot(3, false, true, true), QStringLiteral("copyUri"));
    QCOMPARE(helper.actionForSlot(4, false, true, true), QStringLiteral("properties"));

    QCOMPARE(helper.actionForSlot(0, false, false, true), QStringLiteral("open"));
    QCOMPARE(helper.actionForSlot(1, false, false, true), QStringLiteral("openContainingFolder"));
    QCOMPARE(helper.actionForSlot(2, false, false, true), QStringLiteral("copyPath"));
    QCOMPARE(helper.actionForSlot(3, false, false, true), QStringLiteral("copyName"));
    QCOMPARE(helper.actionForSlot(4, false, false, true), QStringLiteral("copyUri"));
    QCOMPARE(helper.actionForSlot(5, false, false, true), QStringLiteral("properties"));
    QCOMPARE(helper.actionForSlot(8, false, false, true), QStringLiteral("properties"));
}

void TothespotContextMenuHelperTest::selectionNormalizeAndWrap()
{
    TothespotContextMenuHelper helper;

    // file menu has 6 actions
    QCOMPARE(helper.normalizeSelectionIndex(-1, false, false, true), 0);
    QCOMPARE(helper.normalizeSelectionIndex(99, false, false, true), 0);
    QCOMPARE(helper.normalizeSelectionIndex(1, false, false, true), 1);
    QCOMPARE(helper.moveSelection(0, 1, false, false, true), 1);
    QCOMPARE(helper.moveSelection(5, 1, false, false, true), 0);
    QCOMPARE(helper.moveSelection(0, -1, false, false, true), 5);

    // folder menu has 5 actions
    QCOMPARE(helper.moveSelection(0, 1, false, true, true), 1);
    QCOMPARE(helper.moveSelection(4, 1, false, true, true), 0);
    QCOMPARE(helper.moveSelection(0, -1, false, true, true), 4);

    // app menu has 2 actions
    QCOMPARE(helper.moveSelection(0, 1, true, false, true), 1);
    QCOMPARE(helper.moveSelection(1, 1, true, false, true), 0);
    QCOMPARE(helper.moveSelection(0, -1, true, false, true), 1);
}

QTEST_MAIN(TothespotContextMenuHelperTest)
#include "tothespot_contextmenuhelper_test.moc"
