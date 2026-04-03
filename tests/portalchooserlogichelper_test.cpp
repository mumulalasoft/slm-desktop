#include <QtTest>

#include "../portalchooserlogichelper.h"

class PortalChooserLogicHelperTest : public QObject
{
    Q_OBJECT

private slots:
    void shouldGoUpOnBackspace_respectsFocus()
    {
        PortalChooserLogicHelper helper;
        QVERIFY(helper.shouldGoUpOnBackspace(false, false));
        QVERIFY(!helper.shouldGoUpOnBackspace(true, false));
        QVERIFY(!helper.shouldGoUpOnBackspace(false, true));
        QVERIFY(!helper.shouldGoUpOnBackspace(true, true));
    }

    void canPrimaryAction_rules()
    {
        PortalChooserLogicHelper helper;

        // save mode depends on filename
        QVERIFY(helper.canPrimaryAction(QStringLiteral("save"),
                                        false,
                                        QStringLiteral("shot"),
                                        QStringLiteral("/tmp"),
                                        0));
        QVERIFY(!helper.canPrimaryAction(QStringLiteral("save"),
                                         false,
                                         QStringLiteral("   "),
                                         QStringLiteral("/tmp"),
                                         2));

        // folder mode/selectFolders depends on current directory
        QVERIFY(helper.canPrimaryAction(QStringLiteral("open"),
                                        true,
                                        QString(),
                                        QStringLiteral("/home/garis"),
                                        0));
        QVERIFY(!helper.canPrimaryAction(QStringLiteral("open"),
                                         true,
                                         QString(),
                                         QStringLiteral("   "),
                                         10));

        // file open depends on selection count
        QVERIFY(helper.canPrimaryAction(QStringLiteral("open"),
                                        false,
                                        QString(),
                                        QStringLiteral("/home/garis"),
                                        1));
        QVERIFY(!helper.canPrimaryAction(QStringLiteral("open"),
                                         false,
                                         QString(),
                                         QStringLiteral("/home/garis"),
                                         0));
    }

    void enterShortcut_routesToLoadOrPrimary()
    {
        PortalChooserLogicHelper helper;
        QVERIFY(helper.shouldLoadDirectoryOnEnter(true));
        QVERIFY(!helper.shouldLoadDirectoryOnEnter(false));
        QVERIFY(!helper.shouldTriggerPrimaryOnEnter(true));
        QVERIFY(helper.shouldTriggerPrimaryOnEnter(false));
    }

    void escapeShortcut_alwaysCancels()
    {
        PortalChooserLogicHelper helper;
        QVERIFY(helper.shouldCancelOnEscape());
    }

    void altUpShortcut_alwaysGoesUp()
    {
        PortalChooserLogicHelper helper;
        QVERIFY(helper.shouldGoUpOnAltUp(false, false));
        QVERIFY(helper.shouldGoUpOnAltUp(true, false));
        QVERIFY(helper.shouldGoUpOnAltUp(false, true));
        QVERIFY(helper.shouldGoUpOnAltUp(true, true));
    }
};

QTEST_MAIN(PortalChooserLogicHelperTest)
#include "portalchooserlogichelper_test.moc"
