#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantList>
#include <QVariantMap>

#include "../appmodel.h"
#include "../src/daemon/desktopd/daemonhealthmonitor.h"
#include "../src/daemon/desktopd/desktopdaemonservice.h"
#include "../src/core/workspace/spacesmanager.h"
#include "../src/core/prefs/uipreferences.h"
#include "../src/core/workspace/windowingbackendmanager.h"
#include "../src/core/workspace/workspacemanager.h"

namespace {
constexpr const char kService[] = "org.slm.WorkspaceManager";
constexpr const char kPath[] = "/org/slm/WorkspaceManager";
constexpr const char kIface[] = "org.slm.WorkspaceManager1";
}

class WorkspaceManagerDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void methodsAndSignals_contract()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        UIPreferences prefs;
        DesktopAppModel appModel;
        appModel.setUIPreferences(&prefs);
        appModel.refresh();
        DaemonHealthMonitor healthMonitor;
        DesktopDaemonService service(&workspace, &backend, &spaces, &appModel, &healthMonitor);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QSignalSpy overviewSpy(&service, SIGNAL(OverviewShown()));
        QSignalSpy workspaceChangedSpy(&service, SIGNAL(WorkspaceChanged()));
        QSignalSpy attentionSpy(&service, SIGNAL(WindowAttention(QVariantMap)));
        QVERIFY(overviewSpy.isValid());
        QVERIFY(workspaceChangedSpy.isValid());
        QVERIFY(attentionSpy.isValid());

        QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
        QVERIFY(pingReply.isValid());
        const QVariantMap ping = pingReply.value();
        QVERIFY(ping.value(QStringLiteral("ok")).toBool());
        QCOMPARE(ping.value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));
        QCOMPARE(ping.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));

        QDBusReply<QVariantMap> capsReply = iface.call(QStringLiteral("GetCapabilities"));
        QVERIFY(capsReply.isValid());
        const QVariantMap caps = capsReply.value();
        QVERIFY(caps.value(QStringLiteral("ok")).toBool());
        QCOMPARE(caps.value(QStringLiteral("api_version")).toString(), QStringLiteral("1.0"));
        const QStringList list = caps.value(QStringLiteral("capabilities")).toStringList();
        QVERIFY(list.contains(QStringLiteral("show_overview")));
        QVERIFY(list.contains(QStringLiteral("diagnostic_snapshot")));
        QVERIFY(list.contains(QStringLiteral("list_ranked_apps")));
        QVERIFY(list.contains(QStringLiteral("daemon_health_snapshot")));

        QDBusReply<QVariantList> rankedReply = iface.call(QStringLiteral("ListRankedApps"), 8);
        QVERIFY(rankedReply.isValid());
        const QVariantList ranked = rankedReply.value();
        QVERIFY(ranked.size() <= 8);
        for (int i = 0; i < ranked.size(); ++i) {
            QVERIFY(ranked.at(i).canConvert<QVariantMap>());
        }
        for (int i = 1; i < ranked.size(); ++i) {
            const QVariantMap prev = ranked.at(i - 1).toMap();
            const QVariantMap curr = ranked.at(i).toMap();
            const int prevScore = prev.value(QStringLiteral("score")).toInt();
            const int currScore = curr.value(QStringLiteral("score")).toInt();
            QVERIFY2(prevScore >= currScore, "ranked apps are not sorted by score DESC");
        }

        QDBusReply<void> overviewReply = iface.call(QStringLiteral("ShowOverview"));
        QVERIFY(overviewReply.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(overviewSpy.count() >= 1, 1000);

        QDBusReply<void> switchReply = iface.call(QStringLiteral("SwitchWorkspace"), 2);
        QVERIFY(switchReply.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(workspaceChangedSpy.count() >= 1, 1000);

        QDBusReply<QVariantMap> diagReply = iface.call(QStringLiteral("DiagnosticSnapshot"));
        QVERIFY(diagReply.isValid());
        const QVariantMap diag = diagReply.value();
        QCOMPARE(diag.value(QStringLiteral("service")).toString(), QString::fromLatin1(kService));
        QVERIFY(diag.value(QStringLiteral("serviceRegistered")).toBool());
        QCOMPARE(diag.value(QStringLiteral("activeSpace")).toInt(), 2);
        QVERIFY(diag.value(QStringLiteral("spaceCount")).toInt() >= 1);
        QVERIFY(diag.value(QStringLiteral("daemonHealth")).canConvert<QVariantMap>());
        const QVariantMap diagHealth = diag.value(QStringLiteral("daemonHealth")).toMap();
        QVERIFY(diagHealth.value(QStringLiteral("fileOperations")).canConvert<QVariantMap>());
        QVERIFY(diagHealth.value(QStringLiteral("devices")).canConvert<QVariantMap>());
        QCOMPARE(diagHealth.value(QStringLiteral("fileOperations")).toMap().value(QStringLiteral("service")).toString(),
                 QStringLiteral("org.slm.Desktop.FileOperations"));
        QCOMPARE(diagHealth.value(QStringLiteral("devices")).toMap().value(QStringLiteral("service")).toString(),
                 QStringLiteral("org.slm.Desktop.Devices"));

        QDBusReply<QVariantMap> healthReply = iface.call(QStringLiteral("DaemonHealthSnapshot"));
        QVERIFY(healthReply.isValid());
        const QVariantMap health = healthReply.value();
        QVERIFY(health.value(QStringLiteral("fileOperations")).canConvert<QVariantMap>());
        QVERIFY(health.value(QStringLiteral("devices")).canConvert<QVariantMap>());
        QCOMPARE(health.value(QStringLiteral("fileOperations")).toMap().value(QStringLiteral("service")).toString(),
                 QStringLiteral("org.slm.Desktop.FileOperations"));
        QCOMPARE(health.value(QStringLiteral("devices")).toMap().value(QStringLiteral("service")).toString(),
                 QStringLiteral("org.slm.Desktop.Devices"));
        QVERIFY(health.contains(QStringLiteral("baseDelayMs")));
        QVERIFY(health.contains(QStringLiteral("maxDelayMs")));
        QVERIFY(health.value(QStringLiteral("baseDelayMs")).toInt() > 0);
        QVERIFY(health.value(QStringLiteral("maxDelayMs")).toInt() >= health.value(QStringLiteral("baseDelayMs")).toInt());

        QDBusReply<bool> presentReply = iface.call(QStringLiteral("PresentWindow"),
                                                   QStringLiteral("non-existent-app"));
        QVERIFY(presentReply.isValid());
        QVERIFY(!presentReply.value());

        QDBusReply<bool> moveReply = iface.call(QStringLiteral("MoveWindowToWorkspace"),
                                                QVariant(QStringLiteral("kwin:test-view")),
                                                3);
        QVERIFY(moveReply.isValid());
        QVERIFY(moveReply.value());
        QCOMPARE(spaces.windowSpace(QStringLiteral("kwin:test-view")), 3);

        QDBusReply<QVariantMap> beforeDeltaReply = iface.call(QStringLiteral("DiagnosticSnapshot"));
        QVERIFY(beforeDeltaReply.isValid());
        const QVariantMap beforeDelta = beforeDeltaReply.value();
        const int beforeActive = beforeDelta.value(QStringLiteral("activeSpace")).toInt();
        const int beforeCount = qMax(1, beforeDelta.value(QStringLiteral("spaceCount")).toInt());

        QDBusReply<void> switchDeltaReply = iface.call(QStringLiteral("SwitchWorkspaceByDelta"), 1);
        QVERIFY(switchDeltaReply.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(workspaceChangedSpy.count() >= 2, 1000);

        QDBusReply<QVariantMap> afterDeltaReply = iface.call(QStringLiteral("DiagnosticSnapshot"));
        QVERIFY(afterDeltaReply.isValid());
        const QVariantMap afterDelta = afterDeltaReply.value();
        const int expectedAfterActive = qMin(beforeCount, beforeActive + 1);
        QCOMPARE(afterDelta.value(QStringLiteral("activeSpace")).toInt(), expectedAfterActive);

        QDBusReply<bool> moveFocusedDeltaReply = iface.call(QStringLiteral("MoveFocusedWindowByDelta"), 1);
        QVERIFY(moveFocusedDeltaReply.isValid());
        // In headless DBus test environment there is usually no focused user window.
        QVERIFY(!moveFocusedDeltaReply.value());

        QVariantMap payload;
        payload.insert(QStringLiteral("event"), QStringLiteral("window-attention"));
        payload.insert(QStringLiteral("viewId"), QStringLiteral("kwin:test-view"));
        payload.insert(QStringLiteral("appId"), QStringLiteral("org.example.App"));
        QVERIFY(QMetaObject::invokeMethod(&backend, "eventReceived",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("window-attention")),
                                          Q_ARG(QVariantMap, payload)));
        QTRY_VERIFY_WITH_TIMEOUT(attentionSpy.count() >= 1, 1000);
    }

    void daemonHealthSnapshot_withoutMonitor_returnsEmpty()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("session bus is not available in this test environment");
        }

        WindowingBackendManager backend;
        SpacesManager spaces;
        WorkspaceManager workspace(&backend,
                                   &spaces,
                                   backend.compositorStateObject());
        UIPreferences prefs;
        DesktopAppModel appModel;
        appModel.setUIPreferences(&prefs);
        appModel.refresh();
        DesktopDaemonService service(&workspace, &backend, &spaces, &appModel, nullptr);
        QVERIFY(service.serviceRegistered());

        QDBusInterface iface(QString::fromLatin1(kService),
                             QString::fromLatin1(kPath),
                             QString::fromLatin1(kIface),
                             bus);
        QVERIFY(iface.isValid());

        QDBusReply<QVariantMap> healthReply = iface.call(QStringLiteral("DaemonHealthSnapshot"));
        QVERIFY(healthReply.isValid());
        const QVariantMap health = healthReply.value();
        QVERIFY(health.isEmpty());

        QDBusReply<QVariantMap> diagReply = iface.call(QStringLiteral("DiagnosticSnapshot"));
        QVERIFY(diagReply.isValid());
        const QVariantMap diag = diagReply.value();
        QVERIFY(diag.value(QStringLiteral("daemonHealth")).canConvert<QVariantMap>());
        QCOMPARE(diag.value(QStringLiteral("daemonHealth")).toMap().size(), 0);
    }
};

QTEST_MAIN(WorkspaceManagerDbusTest)
#include "workspacemanager_dbus_test.moc"
