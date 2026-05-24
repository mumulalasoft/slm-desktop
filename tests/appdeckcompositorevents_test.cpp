#include <QtTest/QtTest>
#include <QSignalSpy>

#include "../src/core/wayland/layershell/appdeckcompositorevents.h"
#include "../src/core/shell/shellpolicycontroller.h"
#include "../src/core/shell/shellstatecontroller.h"

namespace {
QVariantMap focusedFullscreenWindow(const QString &appId)
{
    return {
        { QStringLiteral("viewId"), QStringLiteral("view-1") },
        { QStringLiteral("title"), QStringLiteral("Focused Fullscreen") },
        { QStringLiteral("appId"), appId },
        { QStringLiteral("x"), 0 },
        { QStringLiteral("y"), 0 },
        { QStringLiteral("width"), 1920 },
        { QStringLiteral("height"), 1080 },
        { QStringLiteral("mapped"), true },
        { QStringLiteral("minimized"), false },
        { QStringLiteral("focused"), true },
        { QStringLiteral("fullscreen"), true },
    };
}
}

class AppDeckCompositorEventsTest : public QObject
{
    Q_OBJECT

private slots:
    void fullscreenSignal_firesOnceOnTransition()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);
        AppDeckCompositorEvents events(&policy, &state);

        QSignalSpy spy(&events, &AppDeckCompositorEvents::fullscreenStateChanged);
        QVERIFY(spy.isValid());
        QVERIFY(!events.fullscreenActive());

        policy.setWindowSnapshot({ focusedFullscreenWindow(QStringLiteral("firefox")) },
                                 QStringLiteral("test"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QVERIFY(events.fullscreenActive());

        // Same policy state again — must not re-emit.
        policy.setWindowSnapshot({ focusedFullscreenWindow(QStringLiteral("firefox")) },
                                 QStringLiteral("test"));
        QCOMPARE(spy.count(), 0);
    }

    void appActivated_firesOnFocusedAppIdChange()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);
        AppDeckCompositorEvents events(&policy, &state);

        QSignalSpy spy(&events, &AppDeckCompositorEvents::appActivated);
        QVERIFY(spy.isValid());

        policy.setWindowSnapshot({ focusedFullscreenWindow(QStringLiteral("firefox")) },
                                 QStringLiteral("test"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("firefox"));
    }

    void notifyOutsidePointer_reEmitsAsNamedSignal()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);
        AppDeckCompositorEvents events(&policy, &state);

        QSignalSpy spy(&events, &AppDeckCompositorEvents::outsidePointerPressed);
        QVERIFY(spy.isValid());

        events.notifyOutsidePointer(QPointF(120, 240));

        QCOMPARE(spy.count(), 1);
        const QPointF pos = spy.takeFirst().at(0).value<QPointF>();
        QCOMPARE(pos.x(), 120.0);
        QCOMPARE(pos.y(), 240.0);
    }

    void notifyWorkspaceFocused_idempotentOnSameIndex()
    {
        ShellStateController state;
        ShellPolicyController policy(&state, nullptr);
        AppDeckCompositorEvents events(&policy, &state);

        QSignalSpy spy(&events, &AppDeckCompositorEvents::workspaceFocused);
        events.notifyWorkspaceFocused(2);
        events.notifyWorkspaceFocused(2);
        events.notifyWorkspaceFocused(3);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 2);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 3);
    }
};

QTEST_MAIN(AppDeckCompositorEventsTest)
#include "appdeckcompositorevents_test.moc"
