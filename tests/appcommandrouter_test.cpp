#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>

#include "../src/core/execution/appcommandrouter.h"
#include "../src/core/execution/appexecutiongate.h"
#include "../src/core/workspace/workspacemanager.h"
#include "../src/core/workspace/windowingbackendmanager.h"

// Stub methods required by appcommandrouter.cpp link-time references.
bool AppExecutionGate::launchDesktopEntry(const QString &, const QString &, const QString &,
                                          const QString &, const QString &, const QString &)
{
    return false;
}

bool AppExecutionGate::launchDesktopId(const QString &, const QString &)
{
    return false;
}

bool AppExecutionGate::launchEntryMap(const QVariantMap &, const QString &)
{
    return false;
}

bool AppExecutionGate::launchFromFileManager(const QString &)
{
    return false;
}

bool AppExecutionGate::launchFromTerminal(const QString &, const QString &)
{
    return false;
}

bool WorkspaceManager::PresentView(const QString &)
{
    return false;
}

void WorkspaceManager::ToggleWorkspace()
{
}

bool WindowingBackendManager::sendCommand(const QString &)
{
    return false;
}

class AppCommandRouterTest : public QObject {
    Q_OBJECT

private slots:
    void supportedActions_contract()
    {
        AppCommandRouter router(nullptr, nullptr);
        const QStringList actions = router.supportedActions();
        QVERIFY(actions.contains(QStringLiteral("filemanager.open")));
        QVERIFY(actions.contains(QStringLiteral("terminal.exec")));
        QVERIFY(actions.contains(QStringLiteral("app.desktopid")));
        QVERIFY(actions.contains(QStringLiteral("app.entry")));
        QVERIFY(actions.contains(QStringLiteral("app.desktopentry")));
        QVERIFY(actions.contains(QStringLiteral("workspace.presentview")));
        QVERIFY(actions.contains(QStringLiteral("workspace.toggle")));
        QVERIFY(actions.contains(QStringLiteral("workspace.split_left")));
        QVERIFY(actions.contains(QStringLiteral("workspace.split_right")));
        QVERIFY(actions.contains(QStringLiteral("workspace.pin_current")));
        QVERIFY(actions.contains(QStringLiteral("storage.mount")));
        QVERIFY(actions.contains(QStringLiteral("storage.unmount")));
        QVERIFY(router.isSupportedAction(QStringLiteral("APP.ENTRY")));
        QVERIFY(!router.isSupportedAction(QStringLiteral("app.unknown")));
    }

    void routeWithNullGate_recordsFailure()
    {
        AppCommandRouter router(nullptr, nullptr);
        QVERIFY(!router.route(QStringLiteral("app.entry"), QVariantMap{}, QStringLiteral("test")));
        QCOMPARE(router.eventCount(), 1);
        QCOMPARE(router.failureCount(), 1);
        QCOMPARE(router.lastError(), QStringLiteral("gate-unavailable"));
        const QVariantMap last = router.lastEvent();
        QCOMPARE(last.value(QStringLiteral("action")).toString(), QStringLiteral("app.entry"));
        QCOMPARE(last.value(QStringLiteral("source")).toString(), QStringLiteral("test"));
        QCOMPARE(last.value(QStringLiteral("success")).toBool(), false);
        QCOMPARE(last.value(QStringLiteral("detail")).toString(), QStringLiteral("gate-unavailable"));
    }

    void statsAndRecentFailures_work()
    {
        AppCommandRouter router(nullptr, nullptr);
        QVERIFY(!router.route(QStringLiteral("app.entry"), QVariantMap{}, QStringLiteral("s1")));
        QVERIFY(!router.route(QStringLiteral("terminal.exec"), QVariantMap{}, QStringLiteral("s2")));
        QVERIFY(!router.route(QStringLiteral("app.entry"), QVariantMap{}, QStringLiteral("s3")));

        const QVariantMap stats = router.actionStats();
        const QVariantMap appEntry = stats.value(QStringLiteral("app.entry")).toMap();
        QCOMPARE(appEntry.value(QStringLiteral("total")).toInt(), 2);
        QCOMPARE(appEntry.value(QStringLiteral("failure")).toInt(), 2);
        QCOMPARE(appEntry.value(QStringLiteral("success")).toInt(), 0);

        const QVariantList failures = router.recentFailures(2);
        QCOMPARE(failures.size(), 2);
        QCOMPARE(failures.at(0).toMap().value(QStringLiteral("source")).toString(), QStringLiteral("s3"));
        QCOMPARE(failures.at(1).toMap().value(QStringLiteral("source")).toString(), QStringLiteral("s2"));
    }

    void unknownAction_andPayloadValidation_haveDetails()
    {
        AppCommandRouter router(nullptr, nullptr);
        QSignalSpy detailedSpy(&router, SIGNAL(routedDetailed(QVariantMap)));
        const QVariantMap unknown = router.routeWithResult(QStringLiteral("app.unknown"),
                                                           QVariantMap{},
                                                           QStringLiteral("x"));
        QCOMPARE(unknown.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(unknown.value(QStringLiteral("error")).toString(), QStringLiteral("unknown-action"));
        QCOMPARE(unknown.value(QStringLiteral("action")).toString(), QStringLiteral("app.unknown"));
        QVERIFY(unknown.contains(QStringLiteral("durationMs")));
        QCOMPARE(detailedSpy.count(), 1);
        const QVariantMap sigUnknown = detailedSpy.takeFirst().at(0).toMap();
        QCOMPARE(sigUnknown.value(QStringLiteral("error")).toString(), QStringLiteral("unknown-action"));
        QVERIFY(sigUnknown.contains(QStringLiteral("durationMs")));
        QCOMPARE(router.lastError(), QStringLiteral("unknown-action"));

        const QVariantMap missing = router.routeWithResult(QStringLiteral("filemanager.open"),
                                                           QVariantMap{},
                                                           QStringLiteral("x"));
        QCOMPARE(missing.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(missing.value(QStringLiteral("error")).toString(), QStringLiteral("missing-target"));
        QCOMPARE(detailedSpy.count(), 1);
        const QVariantMap sigMissing = detailedSpy.takeFirst().at(0).toMap();
        QCOMPARE(sigMissing.value(QStringLiteral("error")).toString(), QStringLiteral("missing-target"));
        QCOMPARE(router.lastError(), QStringLiteral("missing-target"));

        const QVariantMap missingView = router.routeWithResult(QStringLiteral("workspace.presentview"),
                                                               QVariantMap{},
                                                               QStringLiteral("x"));
        QCOMPARE(missingView.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(missingView.value(QStringLiteral("error")).toString(), QStringLiteral("missing-viewid"));
        QCOMPARE(detailedSpy.count(), 1);
        const QVariantMap sigMissingView = detailedSpy.takeFirst().at(0).toMap();
        QCOMPARE(sigMissingView.value(QStringLiteral("error")).toString(), QStringLiteral("missing-viewid"));
        QCOMPARE(router.lastError(), QStringLiteral("missing-viewid"));

        const QVariantMap missingDevice = router.routeWithResult(QStringLiteral("storage.mount"),
                                                                 QVariantMap{},
                                                                 QStringLiteral("x"));
        QCOMPARE(missingDevice.value(QStringLiteral("ok")).toBool(), false);
        QCOMPARE(missingDevice.value(QStringLiteral("error")).toString(), QStringLiteral("missing-device-path"));
        QCOMPARE(detailedSpy.count(), 1);
        const QVariantMap sigMissingDevice = detailedSpy.takeFirst().at(0).toMap();
        QCOMPARE(sigMissingDevice.value(QStringLiteral("error")).toString(), QStringLiteral("missing-device-path"));
        QCOMPARE(router.lastError(), QStringLiteral("missing-device-path"));
    }

    void diagnosticSnapshot_hasCoreFields()
    {
        AppCommandRouter router(nullptr, nullptr);
        QVERIFY(!router.route(QStringLiteral("app.entry"), QVariantMap{}, QStringLiteral("s1")));
        const QVariantMap snap = router.diagnosticSnapshot();
        QVERIFY(snap.contains(QStringLiteral("eventCount")));
        QVERIFY(snap.contains(QStringLiteral("failureCount")));
        QVERIFY(snap.contains(QStringLiteral("lastError")));
        QVERIFY(snap.contains(QStringLiteral("lastEvent")));
        QVERIFY(snap.contains(QStringLiteral("actionStats")));
        QVERIFY(snap.contains(QStringLiteral("recentFailures")));
    }

    void diagnosticsJson_isValid()
    {
        AppCommandRouter router(nullptr, nullptr);
        QVERIFY(!router.route(QStringLiteral("app.entry"), QVariantMap{}, QStringLiteral("s1")));
        const QString jsonText = router.diagnosticsJson(false);
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
        QCOMPARE(err.error, QJsonParseError::NoError);
        QVERIFY(doc.isObject());
        const QJsonObject obj = doc.object();
        QVERIFY(obj.contains(QStringLiteral("eventCount")));
        QVERIFY(obj.contains(QStringLiteral("supportedActions")));
    }

    void clearRecentEvents_resetsCounters()
    {
        AppCommandRouter router(nullptr, nullptr);
        QVERIFY(!router.route(QStringLiteral("app.entry"), QVariantMap{}, QStringLiteral("test")));
        router.clearRecentEvents();
        QCOMPARE(router.eventCount(), 0);
        QCOMPARE(router.failureCount(), 0);
        QVERIFY(router.lastEvent().isEmpty());
        QVERIFY(router.recentEvents().isEmpty());
    }
};

QTEST_MAIN(AppCommandRouterTest)
#include "appcommandrouter_test.moc"
