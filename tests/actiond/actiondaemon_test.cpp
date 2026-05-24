#include "src/daemon/actiond/actiondaemon.h"

#include <QtTest>

class ActionDaemonTest : public QObject
{
    Q_OBJECT

private slots:
    void activeContextPrefersFocusedWindow();
    void renameEnabledOnlyWhenSingleSelection();
    void destructiveRequiresGesture();
};

void ActionDaemonTest::activeContextPrefersFocusedWindow()
{
    Slm::Actiond::ActionDaemon daemon;

    QVERIFY(daemon.registerProvider(QStringLiteral("bus.a"),
                                    QStringLiteral("org.slm.FileManager.desktopview"),
                                    QStringLiteral("org.slm.FileManager"),
                                    {}));
    QVERIFY(daemon.registerContext(QStringLiteral("bus.a"),
                                   QStringLiteral("org.slm.FileManager.desktopview"),
                                   QVariantMap{{QStringLiteral("context_id"), QStringLiteral("desktopview")},
                                               {QStringLiteral("type"), QStringLiteral("desktopview")},
                                               {QStringLiteral("priority"), 40},
                                               {QStringLiteral("active"), true}}));

    QVERIFY(daemon.registerProvider(QStringLiteral("bus.b"),
                                    QStringLiteral("org.slm.Editor.window"),
                                    QStringLiteral("org.slm.Editor"),
                                    {}));
    QVERIFY(daemon.registerContext(QStringLiteral("bus.b"),
                                   QStringLiteral("org.slm.Editor.window"),
                                   QVariantMap{{QStringLiteral("context_id"), QStringLiteral("editor.main")},
                                               {QStringLiteral("type"), QStringLiteral("focused-window")},
                                               {QStringLiteral("priority"), 80},
                                               {QStringLiteral("active"), true}}));

    QCOMPARE(daemon.getActiveContext().value(QStringLiteral("context_id")).toString(), QStringLiteral("editor.main"));

    QVERIFY(daemon.deactivateContext(QStringLiteral("editor.main")));
    QCOMPARE(daemon.getActiveContext().value(QStringLiteral("context_id")).toString(), QStringLiteral("desktopview"));
}

void ActionDaemonTest::renameEnabledOnlyWhenSingleSelection()
{
    Slm::Actiond::ActionDaemon daemon;

    QVERIFY(daemon.registerProvider(QStringLiteral("bus.a"),
                                    QStringLiteral("org.slm.FileManager.desktopview"),
                                    QStringLiteral("org.slm.FileManager"),
                                    {}));
    QVERIFY(daemon.registerContext(QStringLiteral("bus.a"),
                                   QStringLiteral("org.slm.FileManager.desktopview"),
                                   QVariantMap{{QStringLiteral("context_id"), QStringLiteral("desktopview")},
                                               {QStringLiteral("type"), QStringLiteral("desktop-selection")},
                                               {QStringLiteral("priority"), 40},
                                               {QStringLiteral("active"), true}}));
    QVERIFY(daemon.setActions(QStringLiteral("bus.a"),
                              QStringLiteral("org.slm.FileManager.desktopview"),
                              QStringLiteral("desktopview"),
                              QVariantList{QVariantMap{{QStringLiteral("id"), QStringLiteral("file.rename")},
                                                       {QStringLiteral("label"), QStringLiteral("Rename")},
                                                       {QStringLiteral("enabled"), false},
                                                       {QStringLiteral("section"), QStringLiteral("File")}}}));

    QVERIFY(daemon.updateAction(QStringLiteral("bus.a"),
                                QStringLiteral("org.slm.FileManager.desktopview"),
                                QStringLiteral("desktopview"),
                                QStringLiteral("file.rename"),
                                QVariantMap{{QStringLiteral("enabled"), true},
                                            {QStringLiteral("selection_count"), 1}}));
    const QVariantList actions = daemon.getActions(QStringLiteral("desktopview"));
    QCOMPARE(actions.first().toMap().value(QStringLiteral("enabled")).toBool(), true);
}

void ActionDaemonTest::destructiveRequiresGesture()
{
    Slm::Actiond::ActionDaemon daemon;
    const QVariantMap denied = daemon.invokeAction(QStringLiteral("app.quit"),
                                                   QString(),
                                                   QVariantMap{},
                                                   true);
    QCOMPARE(denied.value(QStringLiteral("ok")).toBool(), false);

    const QVariantMap allowed = daemon.invokeAction(QStringLiteral("app.quit"),
                                                    QString(),
                                                    QVariantMap{{QStringLiteral("user_gesture_token"), QStringLiteral("click")}},
                                                    true);
    QCOMPARE(allowed.value(QStringLiteral("ok")).toBool(), true);
}

QTEST_MAIN(ActionDaemonTest)
#include "actiondaemon_test.moc"
