#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSignalSpy>

#include "../src/services/portal/screencastprivacymodel.h"

namespace {
constexpr const char kService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kIface[] = "org.freedesktop.impl.portal.desktop.slm";
}

class FakeImplPortalService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.desktop.slm")

public:
    int closeSessionCalls = 0;
    int revokeSessionCalls = 0;
    int closeAllCalls = 0;
    QString lastClosedSession;
    QString lastRevokedSession;
    QString lastRevokeReason;
    int closeAllClosedCount = 0;

    QVariantMap screencastState{
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"),
         QVariantMap{
             {QStringLiteral("active_count"), 0},
             {QStringLiteral("active_apps"), QVariantList{}},
             {QStringLiteral("active_session_items"), QVariantList{}},
         }},
    };

public slots:
    QVariantMap GetScreencastState() const
    {
        return screencastState;
    }

    QVariantMap CloseScreencastSession(const QString &sessionHandle)
    {
        ++closeSessionCalls;
        lastClosedSession = sessionHandle.trimmed();
        return {
            {QStringLiteral("ok"), !lastClosedSession.isEmpty()},
            {QStringLiteral("response"), lastClosedSession.isEmpty() ? 3u : 0u},
            {QStringLiteral("results"),
             QVariantMap{{QStringLiteral("session_handle"), lastClosedSession}}},
        };
    }

    QVariantMap RevokeScreencastSession(const QString &sessionHandle, const QString &reason)
    {
        ++revokeSessionCalls;
        lastRevokedSession = sessionHandle.trimmed();
        lastRevokeReason = reason.trimmed();
        return {
            {QStringLiteral("ok"), !lastRevokedSession.isEmpty()},
            {QStringLiteral("response"), lastRevokedSession.isEmpty() ? 3u : 0u},
            {QStringLiteral("results"),
             QVariantMap{
                 {QStringLiteral("session_handle"), lastRevokedSession},
                 {QStringLiteral("reason"), lastRevokeReason},
             }},
        };
    }

    QVariantMap CloseAllScreencastSessions()
    {
        ++closeAllCalls;
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("response"), 0u},
            {QStringLiteral("results"),
             QVariantMap{
                 {QStringLiteral("closed_count"), closeAllClosedCount},
             }},
        };
    }

signals:
    void ScreencastSessionStateChanged(const QString &sessionHandle,
                                       const QString &appId,
                                       bool active,
                                       int activeCount);
    void ScreencastSessionRevoked(const QString &sessionHandle,
                                  const QString &appId,
                                  const QString &reason,
                                  int activeCount);
};

class ScreencastPrivacyModelDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void refresh_and_signal_update_state();
    void stop_and_revoke_and_stop_all_call_backend();

private:
    bool registerFake(FakeImplPortalService &fake);
    void unregisterFake();

    QDBusConnection m_bus = QDBusConnection::sessionBus();
};

void ScreencastPrivacyModelDbusTest::init()
{
    if (!m_bus.isConnected()) {
        QSKIP("session bus unavailable");
    }
}

void ScreencastPrivacyModelDbusTest::cleanup()
{
    unregisterFake();
}

bool ScreencastPrivacyModelDbusTest::registerFake(FakeImplPortalService &fake)
{
    QDBusConnectionInterface *iface = m_bus.interface();
    if (!iface) {
        return false;
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_bus.unregisterObject(QString::fromLatin1(kPath));
        m_bus.unregisterService(QString::fromLatin1(kService));
    }
    if (!m_bus.registerService(QString::fromLatin1(kService))) {
        return false;
    }
    if (!m_bus.registerObject(QString::fromLatin1(kPath),
                              &fake,
                              QDBusConnection::ExportAllSlots
                                  | QDBusConnection::ExportAllSignals)) {
        m_bus.unregisterService(QString::fromLatin1(kService));
        return false;
    }
    return true;
}

void ScreencastPrivacyModelDbusTest::unregisterFake()
{
    if (!m_bus.isConnected()) {
        return;
    }
    m_bus.unregisterObject(QString::fromLatin1(kPath));
    m_bus.unregisterService(QString::fromLatin1(kService));
}

void ScreencastPrivacyModelDbusTest::refresh_and_signal_update_state()
{
    FakeImplPortalService fake;
    fake.screencastState = {
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"),
         QVariantMap{
             {QStringLiteral("active_count"), 1},
             {QStringLiteral("active_apps"),
              QVariantList{QStringLiteral("org.example.ScreenShare.desktop")}},
             {QStringLiteral("active_session_items"),
              QVariantList{
                  QVariantMap{
                      {QStringLiteral("session"), QStringLiteral("/session/one")},
                      {QStringLiteral("app_id"), QStringLiteral("org.example.ScreenShare.desktop")},
                      {QStringLiteral("app_label"), QStringLiteral("ScreenShare")},
                      {QStringLiteral("icon_name"), QStringLiteral("video-display")},
                  },
              }},
         }},
    };
    if (!registerFake(fake)) {
        QSKIP("cannot register fake service/object on session bus");
    }

    ScreencastPrivacyModel model;
    QCOMPARE(model.activeCount(), 1);
    QCOMPARE(model.active(), true);
    QCOMPARE(model.activeApps(), QStringList{QStringLiteral("org.example.ScreenShare")});
    QCOMPARE(model.activeSessionItems().size(), 1);
    QCOMPARE(model.statusTitle(), QStringLiteral("Screen sharing is active"));
    QCOMPARE(model.statusSubtitle(), QStringLiteral("Shared by org.example.ScreenShare"));
    QCOMPARE(model.tooltipText(), QStringLiteral("Screen sharing active"));
    QCOMPARE(model.lastActionText(), QString());

    QSignalSpy countSpy(&model, &ScreencastPrivacyModel::activeCountChanged);
    fake.screencastState = {
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"),
         QVariantMap{
             {QStringLiteral("active_count"), 2},
             {QStringLiteral("active_apps"),
              QVariantList{
                  QStringLiteral("org.example.ScreenShare.desktop"),
                  QStringLiteral("org.example.Meet.desktop"),
              }},
             {QStringLiteral("active_session_items"),
              QVariantList{
                  QVariantMap{
                      {QStringLiteral("session"), QStringLiteral("/session/one")},
                      {QStringLiteral("app_id"), QStringLiteral("org.example.ScreenShare.desktop")},
                  },
                  QVariantMap{
                      {QStringLiteral("session"), QStringLiteral("/session/two")},
                      {QStringLiteral("app_id"), QStringLiteral("org.example.Meet.desktop")},
                  },
              }},
         }},
    };

    emit fake.ScreencastSessionStateChanged(QStringLiteral("/session/two"),
                                            QStringLiteral("org.example.Meet.desktop"),
                                            true,
                                            2);
    QTRY_COMPARE(model.activeCount(), 2);
    QVERIFY(countSpy.count() >= 1);
    QCOMPARE(model.statusTitle(), QStringLiteral("Screen sharing is active (2 sessions)"));
    QCOMPARE(model.statusSubtitle(), QStringLiteral("Shared by org.example.ScreenShare and 1 more"));
    QCOMPARE(model.tooltipText(), QStringLiteral("Screen sharing active (2)"));

    emit fake.ScreencastSessionRevoked(QStringLiteral("/session/two"),
                                       QStringLiteral("org.example.Meet.desktop"),
                                       QStringLiteral("policy-revoke"),
                                       1);
    QTRY_COMPARE(model.lastActionText(), QStringLiteral("Session revoked: policy-revoke"));
}

void ScreencastPrivacyModelDbusTest::stop_and_revoke_and_stop_all_call_backend()
{
    FakeImplPortalService fake;
    fake.closeAllClosedCount = 3;
    if (!registerFake(fake)) {
        QSKIP("cannot register fake service/object on session bus");
    }

    ScreencastPrivacyModel model;

    QVERIFY(model.stopSession(QStringLiteral("/session/a")));
    QCOMPARE(fake.closeSessionCalls, 1);
    QCOMPARE(fake.lastClosedSession, QStringLiteral("/session/a"));
    QCOMPARE(model.lastActionText(), QStringLiteral("Stopped sharing for one session"));

    QVERIFY(model.revokeSession(QStringLiteral("/session/b"), QStringLiteral("policy-user")));
    QCOMPARE(fake.revokeSessionCalls, 1);
    QCOMPARE(fake.lastRevokedSession, QStringLiteral("/session/b"));
    QCOMPARE(fake.lastRevokeReason, QStringLiteral("policy-user"));
    QCOMPARE(model.lastActionText(), QStringLiteral("Revoked sharing for one session"));

    const int closedCount = model.stopAllSessions();
    QCOMPARE(closedCount, 3);
    QCOMPARE(fake.closeAllCalls, 1);
    QCOMPARE(model.lastActionText(), QStringLiteral("Stopped sharing for 3 sessions"));
}

QTEST_MAIN(ScreencastPrivacyModelDbusTest)
#include "screencastprivacymodel_dbus_test.moc"
