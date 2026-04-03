#include <QtTest/QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QStringList>

#include "../src/daemon/desktopd/inputcaptureservice.h"
#include "../src/core/workspace/compositorinputcapturebackendservice.h"
#include "../src/core/workspace/compositorinputcaptureprimitiveservice.h"

namespace {
constexpr const char kService[] = "org.slm.Desktop.InputCapture";
constexpr const char kPath[] = "/org/slm/Desktop/InputCapture";
constexpr const char kIface[] = "org.slm.Desktop.InputCapture";
constexpr const char kDefaultBackendService[] = "org.slm.Desktop.InputCaptureBackend";
constexpr const char kDefaultBackendPath[] = "/org/slm/Desktop/InputCaptureBackend";
constexpr const char kDefaultBackendIface[] = "org.slm.Desktop.InputCaptureBackend";
constexpr const char kFakeBackendService[] = "org.example.InputCaptureBackend";
constexpr const char kFakeBackendPath[] = "/org/example/InputCaptureBackend";
constexpr const char kFakeBackendIface[] = "org.example.InputCaptureBackend";
constexpr const char kNativeBackendService[] = "org.slm.Compositor.InputCaptureBackend";
constexpr const char kNativeBackendPath[] = "/org/slm/Compositor/InputCaptureBackend";
constexpr const char kNativeBackendIface[] = "org.slm.Compositor.InputCaptureBackend";
constexpr const char kPrimitiveService[] = "org.example.Compositor.InputCapture";
constexpr const char kPrimitivePath[] = "/org/example/Compositor/InputCapture";
constexpr const char kPrimitiveIface[] = "org.example.Compositor.InputCapture";
}

class FakeInputCaptureBackendService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.InputCaptureBackend")

public slots:
    QVariantMap CreateSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options)
    {
        return ok(QStringLiteral("create"), sessionPath,
                  {{QStringLiteral("app_id"), appId},
                   {QStringLiteral("options"), options}});
    }

    QVariantMap SetPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options)
    {
        return ok(QStringLiteral("set_barriers"), sessionPath,
                  {{QStringLiteral("barrier_count"), barriers.size()},
                   {QStringLiteral("options"), options}});
    }

    QVariantMap EnableSession(const QString &sessionPath,
                              const QVariantMap &options)
    {
        return ok(QStringLiteral("enable"), sessionPath,
                  {{QStringLiteral("options"), options}});
    }

    QVariantMap DisableSession(const QString &sessionPath,
                               const QVariantMap &options)
    {
        return ok(QStringLiteral("disable"), sessionPath,
                  {{QStringLiteral("options"), options}});
    }

    QVariantMap ReleaseSession(const QString &sessionPath,
                               const QString &reason)
    {
        return ok(QStringLiteral("release"), sessionPath,
                  {{QStringLiteral("reason"), reason}});
    }

private:
    static QVariantMap ok(const QString &op,
                          const QString &session,
                          const QVariantMap &extra)
    {
        QVariantMap results{
            {QStringLiteral("operation"), op},
            {QStringLiteral("session_handle"), session},
        };
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
            results.insert(it.key(), it.value());
        }
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("results"), results},
        };
    }
};

class FakeDefaultInputCaptureBackendService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.InputCaptureBackend")

public slots:
    QVariantMap CreateSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options)
    {
        return ok(QStringLiteral("create"), sessionPath,
                  {{QStringLiteral("app_id"), appId},
                   {QStringLiteral("options"), options}});
    }

    QVariantMap SetPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options)
    {
        return ok(QStringLiteral("set_barriers"), sessionPath,
                  {{QStringLiteral("barrier_count"), barriers.size()},
                   {QStringLiteral("options"), options}});
    }

    QVariantMap EnableSession(const QString &sessionPath,
                              const QVariantMap &options)
    {
        return ok(QStringLiteral("enable"), sessionPath,
                  {{QStringLiteral("options"), options}});
    }

    QVariantMap DisableSession(const QString &sessionPath,
                               const QVariantMap &options)
    {
        return ok(QStringLiteral("disable"), sessionPath,
                  {{QStringLiteral("options"), options}});
    }

    QVariantMap ReleaseSession(const QString &sessionPath,
                               const QString &reason)
    {
        return ok(QStringLiteral("release"), sessionPath,
                  {{QStringLiteral("reason"), reason}});
    }

private:
    static QVariantMap ok(const QString &op,
                          const QString &session,
                          const QVariantMap &extra)
    {
        QVariantMap results{
            {QStringLiteral("operation"), op},
            {QStringLiteral("session_handle"), session},
        };
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
            results.insert(it.key(), it.value());
        }
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("results"), results},
        };
    }
};

class FakeNativeInputCaptureBackendService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Compositor.InputCaptureBackend")

public slots:
    QVariantMap CreateSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options)
    {
        return ok(QStringLiteral("create"), sessionPath,
                  {{QStringLiteral("app_id"), appId},
                   {QStringLiteral("options"), options}});
    }

    QVariantMap SetPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options)
    {
        return ok(QStringLiteral("set_barriers"), sessionPath,
                  {{QStringLiteral("barrier_count"), barriers.size()},
                   {QStringLiteral("options"), options}});
    }

    QVariantMap EnableSession(const QString &sessionPath,
                              const QVariantMap &options)
    {
        return ok(QStringLiteral("enable"), sessionPath,
                  {{QStringLiteral("options"), options}});
    }

    QVariantMap DisableSession(const QString &sessionPath,
                               const QVariantMap &options)
    {
        return ok(QStringLiteral("disable"), sessionPath,
                  {{QStringLiteral("options"), options}});
    }

    QVariantMap ReleaseSession(const QString &sessionPath,
                               const QString &reason)
    {
        return ok(QStringLiteral("release"), sessionPath,
                  {{QStringLiteral("reason"), reason}});
    }

private:
    static QVariantMap ok(const QString &op,
                          const QString &session,
                          const QVariantMap &extra)
    {
        QVariantMap results{
            {QStringLiteral("operation"), op},
            {QStringLiteral("session_handle"), session},
        };
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
            results.insert(it.key(), it.value());
        }
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("results"), results},
        };
    }
};

class FakeInputCaptureCommandBridge : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE bool sendCommand(const QString &command)
    {
        m_commands.push_back(command.trimmed());
        return true;
    }
    Q_INVOKABLE bool hasCapability(const QString &) const
    {
        return true;
    }

    QStringList commands() const
    {
        return m_commands;
    }

private:
    QStringList m_commands;
};

class FakeInputCaptureRejectingCommandBridge : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE bool sendCommand(const QString &)
    {
        return false;
    }

    Q_INVOKABLE bool hasCapability(const QString &) const
    {
        return false;
    }
};

class FakeInputCapturePrimitiveService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.Compositor.InputCapture")

public slots:
    QVariantMap SetPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_barrierCount = barriers.size();
        return {{QStringLiteral("ok"), true}};
    }

    QVariantMap EnableSession(const QString &sessionPath, const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_enabled = true;
        return {{QStringLiteral("ok"), true}};
    }

    QVariantMap DisableSession(const QString &sessionPath, const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_enabled = false;
        return {{QStringLiteral("ok"), true}};
    }

    QVariantMap ReleaseSession(const QString &sessionPath, const QString &reason)
    {
        Q_UNUSED(reason)
        m_lastSession = sessionPath.trimmed();
        m_released = true;
        return {{QStringLiteral("ok"), true}};
    }

public:
    QString lastSession() const { return m_lastSession; }
    int barrierCount() const { return m_barrierCount; }
    bool enabled() const { return m_enabled; }
    bool released() const { return m_released; }

private:
    QString m_lastSession;
    int m_barrierCount = 0;
    bool m_enabled = false;
    bool m_released = false;
};

class FakeStructuredProviderBridge : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QVariantMap setInputCapturePointerBarriers(const QString &sessionPath,
                                                           const QVariantList &barriers,
                                                           const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_barrierCount = barriers.size();
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }
    Q_INVOKABLE QVariantMap enableInputCaptureSession(const QString &sessionPath,
                                                      const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_enabled = true;
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }
    Q_INVOKABLE QVariantMap disableInputCaptureSession(const QString &sessionPath,
                                                       const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_enabled = false;
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }
    Q_INVOKABLE QVariantMap releaseInputCaptureSession(const QString &sessionPath,
                                                       const QString &reason)
    {
        Q_UNUSED(reason)
        m_lastSession = sessionPath.trimmed();
        m_released = true;
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }
    Q_INVOKABLE bool sendCommand(const QString &command)
    {
        m_commands.push_back(command.trimmed());
        return false;
    }
    Q_INVOKABLE bool hasCapability(const QString &) const
    {
        return false;
    }

    QString lastSession() const { return m_lastSession; }
    int barrierCount() const { return m_barrierCount; }
    bool enabled() const { return m_enabled; }
    bool released() const { return m_released; }
    int structuredCalls() const { return m_structuredCalls; }
    QStringList commands() const { return m_commands; }

private:
    QString m_lastSession;
    int m_barrierCount = 0;
    bool m_enabled = false;
    bool m_released = false;
    int m_structuredCalls = 0;
    QStringList m_commands;
};

class FakeStructuredInputCaptureBridge : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QVariantMap setInputCapturePointerBarriers(const QString &sessionPath,
                                                           const QVariantList &barriers,
                                                           const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_barrierCount = barriers.size();
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }

    Q_INVOKABLE QVariantMap enableInputCaptureSession(const QString &sessionPath,
                                                      const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_enabled = true;
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }

    Q_INVOKABLE QVariantMap disableInputCaptureSession(const QString &sessionPath,
                                                       const QVariantMap &options)
    {
        Q_UNUSED(options)
        m_lastSession = sessionPath.trimmed();
        m_enabled = false;
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }

    Q_INVOKABLE QVariantMap releaseInputCaptureSession(const QString &sessionPath,
                                                       const QString &reason)
    {
        Q_UNUSED(reason)
        m_lastSession = sessionPath.trimmed();
        m_released = true;
        ++m_structuredCalls;
        return {{QStringLiteral("ok"), true}};
    }

    Q_INVOKABLE bool sendCommand(const QString &command)
    {
        m_commands.push_back(command.trimmed());
        return true;
    }

    QString lastSession() const { return m_lastSession; }
    int barrierCount() const { return m_barrierCount; }
    bool enabled() const { return m_enabled; }
    bool released() const { return m_released; }
    int structuredCalls() const { return m_structuredCalls; }
    QStringList commands() const { return m_commands; }

private:
    QString m_lastSession;
    int m_barrierCount = 0;
    bool m_enabled = false;
    bool m_released = false;
    int m_structuredCalls = 0;
    QStringList m_commands;
};

class InputCaptureServiceDbusTest : public QObject
{
    Q_OBJECT

private slots:
    void contract_basicFlow();
    void contract_invalidBarrierRejected();
    void contract_enableConflictRequiresMediation();
    void contract_directEnablePreemptPath();
    void contract_helperBackendPath();
    void contract_dbusDirectBackendPath();
    void contract_dbusDirectAutoDetectPath();
    void contract_nativeCompositorAutoDetectPath();
    void contract_realCompositorProviderEndToEndPath();
    void contract_realCompositorProviderCommandForwarding();
    void contract_realCompositorProviderRequireCompositorCommandRollback();
    void contract_realCompositorProviderPrimitiveBridgePath();
    void contract_realCompositorProviderStructuredPrimitivePreferred();
    void contract_primitiveServiceStructuredBridgePreferred();
};

void InputCaptureServiceDbusTest::contract_basicFlow()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    InputCaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register InputCapture service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
    QVERIFY(pingReply.isValid());
    QVERIFY(pingReply.value().value(QStringLiteral("ok")).toBool());

    const QString sessionPath = QStringLiteral("/org/desktop/portal/session/org_example_Test/ic1");

    QDBusReply<QVariantMap> createReply =
        iface.call(QStringLiteral("CreateSession"),
                   sessionPath,
                   QStringLiteral("org.example.Test"),
                   QVariantMap{});
    QVERIFY(createReply.isValid());
    QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

    const QVariantList barriers{};

    QDBusReply<QVariantMap> barriersReply =
        iface.call(QStringLiteral("SetPointerBarriers"),
                   sessionPath,
                   barriers,
                   QVariantMap{});
    QVERIFY(barriersReply.isValid());
    QVERIFY(barriersReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enableReply =
        iface.call(QStringLiteral("EnableSession"), sessionPath, QVariantMap{});
    QVERIFY(enableReply.isValid());
    QVERIFY(enableReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> stateReply =
        iface.call(QStringLiteral("GetSessionState"), sessionPath);
    QVERIFY(stateReply.isValid());
    QVERIFY(stateReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> releaseReply =
        iface.call(QStringLiteral("ReleaseSession"), sessionPath, QStringLiteral("test-release"));
    QVERIFY(releaseReply.isValid());
    QVERIFY(releaseReply.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> stateAfterReply =
        iface.call(QStringLiteral("GetSessionState"), sessionPath);
    QVERIFY(stateAfterReply.isValid());
    QVERIFY(stateAfterReply.value().value(QStringLiteral("ok")).toBool());
}

void InputCaptureServiceDbusTest::contract_invalidBarrierRejected()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    InputCaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register InputCapture service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    const QString sessionPath = QStringLiteral("/org/desktop/portal/session/org_example_Test/ic2");
    QDBusReply<QVariantMap> createReply =
        iface.call(QStringLiteral("CreateSession"),
                   sessionPath,
                   QStringLiteral("org.example.Test"),
                   QVariantMap{});
    QVERIFY(createReply.isValid());
    QVERIFY(createReply.value().value(QStringLiteral("ok")).toBool());

    const QVariantList invalidBarriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("bad-shape")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 10},
            {QStringLiteral("y2"), 10},
        }
    };
    QDBusReply<QVariantMap> barriersReply =
        iface.call(QStringLiteral("SetPointerBarriers"),
                   sessionPath,
                   invalidBarriers,
                   QVariantMap{});
    QVERIFY(barriersReply.isValid());
    QVERIFY(!barriersReply.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(barriersReply.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("invalid-argument"));
}

void InputCaptureServiceDbusTest::contract_enableConflictRequiresMediation()
{
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }

    InputCaptureService service;
    if (!service.serviceRegistered()) {
        QSKIP("cannot register InputCapture service name on session bus (likely already owned)");
    }

    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         bus);
    QVERIFY(iface.isValid());

    const QString s1 = QStringLiteral("/org/desktop/portal/session/org_example_Test/ic_conflict_1");
    const QString s2 = QStringLiteral("/org/desktop/portal/session/org_example_Test/ic_conflict_2");
    QDBusReply<QVariantMap> create1 =
        iface.call(QStringLiteral("CreateSession"),
                   s1,
                   QStringLiteral("org.example.Test"),
                   QVariantMap{});
    QVERIFY(create1.isValid());
    QVERIFY(create1.value().value(QStringLiteral("ok")).toBool());
    QDBusReply<QVariantMap> create2 =
        iface.call(QStringLiteral("CreateSession"),
                   s2,
                   QStringLiteral("org.example.Test"),
                   QVariantMap{});
    QVERIFY(create2.isValid());
    QVERIFY(create2.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable1 = iface.call(QStringLiteral("EnableSession"), s1, QVariantMap{});
    QVERIFY(enable1.isValid());
    QVERIFY(enable1.value().value(QStringLiteral("ok")).toBool());

    QDBusReply<QVariantMap> enable2 = iface.call(QStringLiteral("EnableSession"), s2, QVariantMap{});
    QVERIFY(enable2.isValid());
    QVERIFY(!enable2.value().value(QStringLiteral("ok")).toBool());
    QCOMPARE(enable2.value().value(QStringLiteral("error")).toString(),
             QStringLiteral("permission-denied"));
    const QVariantMap results = enable2.value().value(QStringLiteral("results")).toMap();
    const QString reason =
        enable2.value().value(QStringLiteral("reason")).toString().trimmed().isEmpty()
            ? results.value(QStringLiteral("reason")).toString().trimmed()
            : enable2.value().value(QStringLiteral("reason")).toString().trimmed();
    QCOMPARE(reason, QStringLiteral("conflict-active-session"));
}

void InputCaptureServiceDbusTest::contract_directEnablePreemptPath()
{
    InputCaptureService service;

    const QString s1 = QStringLiteral("/org/desktop/portal/session/direct/ic_preempt_1");
    const QString s2 = QStringLiteral("/org/desktop/portal/session/direct/ic_preempt_2");

    const QVariantMap create1 = service.CreateSession(s1, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(create1.value(QStringLiteral("ok")).toBool());
    const QVariantMap create2 = service.CreateSession(s2, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(create2.value(QStringLiteral("ok")).toBool());

    const QVariantMap enable1 = service.EnableSession(s1, QVariantMap{});
    QVERIFY(enable1.value(QStringLiteral("ok")).toBool());

    const QVariantMap preemptOptions{
        {QStringLiteral("allow_preempt"), true},
        {QStringLiteral("preempt_reason"), QStringLiteral("direct-test-preempt")},
    };
    const QVariantMap enable2 = service.EnableSession(s2, preemptOptions);
    QVERIFY(enable2.value(QStringLiteral("ok")).toBool());
    const QVariantMap enable2Results = enable2.value(QStringLiteral("results")).toMap();
    QCOMPARE(enable2Results.value(QStringLiteral("preempted")).toBool(), true);
    QCOMPARE(enable2Results.value(QStringLiteral("preempted_session")).toString(), s1);

    const QVariantMap state1 = service.GetSessionState(s1);
    QVERIFY(state1.value(QStringLiteral("ok")).toBool());
    QCOMPARE(state1.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("enabled")).toBool(), false);

    const QVariantMap state2 = service.GetSessionState(s2);
    QVERIFY(state2.value(QStringLiteral("ok")).toBool());
    QCOMPARE(state2.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("enabled")).toBool(), true);
}

void InputCaptureServiceDbusTest::contract_helperBackendPath()
{
    const QByteArray prevHelper = qgetenv("SLM_INPUTCAPTURE_HELPER");
    const QByteArray prevSecurity = qgetenv("SLM_SECURITY_DISABLED");
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString helperPath = dir.path() + QStringLiteral("/inputcapture-helper-mock.sh");

    QFile helper(helperPath);
    QVERIFY(helper.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    QTextStream out(&helper);
    out << "#!/usr/bin/env bash\n";
    out << "read -r _payload\n";
    out << "echo '{\"ok\":true,\"reason\":\"mock-ok\"}'\n";
    helper.close();
    QVERIFY(helper.setPermissions(QFileDevice::ReadOwner
                                  | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));

    qputenv("SLM_INPUTCAPTURE_HELPER", helperPath.toUtf8());

    InputCaptureService service;
    const QString s = QStringLiteral("/org/desktop/portal/session/direct/ic_helper_1");
    const QVariantMap create = service.CreateSession(s, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(create.value(QStringLiteral("ok")).toBool());
    const QVariantMap createResults = create.value(QStringLiteral("results")).toMap();
    const QVariantMap backend = createResults.value(QStringLiteral("backend")).toMap();
    QCOMPARE(backend.value(QStringLiteral("backend")).toString(), QStringLiteral("helper-process"));
    QCOMPARE(backend.value(QStringLiteral("applied")).toBool(), true);
    QVERIFY(backend.value(QStringLiteral("helper")).toMap().value(QStringLiteral("ok")).toBool());

    const QVariantMap state = service.GetState();
    QVERIFY(state.value(QStringLiteral("ok")).toBool());
    const QVariantMap stateBackend = state.value(QStringLiteral("results")).toMap()
                                        .value(QStringLiteral("backend")).toMap();
    QCOMPARE(stateBackend.value(QStringLiteral("name")).toString(), QStringLiteral("helper-process"));
    QCOMPARE(stateBackend.value(QStringLiteral("mode")).toString(), QStringLiteral("external-helper"));

    if (prevHelper.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_HELPER");
    } else {
        qputenv("SLM_INPUTCAPTURE_HELPER", prevHelper);
    }
    if (prevSecurity.isEmpty()) {
        qunsetenv("SLM_SECURITY_DISABLED");
    } else {
        qputenv("SLM_SECURITY_DISABLED", prevSecurity);
    }
}

void InputCaptureServiceDbusTest::contract_dbusDirectBackendPath()
{
    const QByteArray prevHelper = qgetenv("SLM_INPUTCAPTURE_HELPER");
    const QByteArray prevDbusService = qgetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    const QByteArray prevDbusPath = qgetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    const QByteArray prevDbusIface = qgetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    const QByteArray prevSecurity = qgetenv("SLM_SECURITY_DISABLED");
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));
    qunsetenv("SLM_INPUTCAPTURE_HELPER");

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }
    QDBusConnectionInterface *iface = bus.interface();
    QVERIFY(iface);

    if (iface->isServiceRegistered(QString::fromLatin1(kFakeBackendService)).value()) {
        bus.unregisterObject(QString::fromLatin1(kFakeBackendPath));
        bus.unregisterService(QString::fromLatin1(kFakeBackendService));
    }

    FakeInputCaptureBackendService fakeBackend;
    QVERIFY(bus.registerService(QString::fromLatin1(kFakeBackendService)));
    QVERIFY(bus.registerObject(QString::fromLatin1(kFakeBackendPath),
                               &fakeBackend,
                               QDBusConnection::ExportAllSlots));

    qputenv("SLM_INPUTCAPTURE_DBUS_SERVICE", QByteArray(kFakeBackendService));
    qputenv("SLM_INPUTCAPTURE_DBUS_PATH", QByteArray(kFakeBackendPath));
    qputenv("SLM_INPUTCAPTURE_DBUS_IFACE", QByteArray(kFakeBackendIface));

    InputCaptureService service;
    const QString s = QStringLiteral("/org/desktop/portal/session/direct/ic_dbus_1");
    const QVariantMap create = service.CreateSession(s, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(create.value(QStringLiteral("ok")).toBool());
    const QVariantMap backend = create.value(QStringLiteral("results")).toMap()
                                   .value(QStringLiteral("backend")).toMap();
    QCOMPARE(backend.value(QStringLiteral("backend")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(backend.value(QStringLiteral("applied")).toBool(), true);
    QVERIFY(backend.contains(QStringLiteral("reply")));

    const QVariantMap state = service.GetState();
    QVERIFY(state.value(QStringLiteral("ok")).toBool());
    const QVariantMap stateBackend = state.value(QStringLiteral("results")).toMap()
                                        .value(QStringLiteral("backend")).toMap();
    QCOMPARE(stateBackend.value(QStringLiteral("name")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(stateBackend.value(QStringLiteral("service")).toString(),
             QString::fromLatin1(kFakeBackendService));

    bus.unregisterObject(QString::fromLatin1(kFakeBackendPath));
    bus.unregisterService(QString::fromLatin1(kFakeBackendService));

    if (prevHelper.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_HELPER");
    } else {
        qputenv("SLM_INPUTCAPTURE_HELPER", prevHelper);
    }
    if (prevDbusService.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_SERVICE", prevDbusService);
    }
    if (prevDbusPath.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_PATH", prevDbusPath);
    }
    if (prevDbusIface.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_IFACE", prevDbusIface);
    }
    if (prevSecurity.isEmpty()) {
        qunsetenv("SLM_SECURITY_DISABLED");
    } else {
        qputenv("SLM_SECURITY_DISABLED", prevSecurity);
    }
}

void InputCaptureServiceDbusTest::contract_dbusDirectAutoDetectPath()
{
    const QByteArray prevHelper = qgetenv("SLM_INPUTCAPTURE_HELPER");
    const QByteArray prevDbusService = qgetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    const QByteArray prevDbusPath = qgetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    const QByteArray prevDbusIface = qgetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    const QByteArray prevSecurity = qgetenv("SLM_SECURITY_DISABLED");
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));
    qunsetenv("SLM_INPUTCAPTURE_HELPER");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_IFACE");

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }
    QDBusConnectionInterface *iface = bus.interface();
    QVERIFY(iface);

    if (iface->isServiceRegistered(QString::fromLatin1(kDefaultBackendService)).value()) {
        bus.unregisterObject(QString::fromLatin1(kDefaultBackendPath));
        bus.unregisterService(QString::fromLatin1(kDefaultBackendService));
    }

    FakeDefaultInputCaptureBackendService fakeBackend;
    QVERIFY(bus.registerService(QString::fromLatin1(kDefaultBackendService)));
    QVERIFY(bus.registerObject(QString::fromLatin1(kDefaultBackendPath),
                               &fakeBackend,
                               QDBusConnection::ExportAllSlots));

    InputCaptureService service;
    const QString s = QStringLiteral("/org/desktop/portal/session/direct/ic_dbus_autodetect_1");
    const QVariantMap create = service.CreateSession(s, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(create.value(QStringLiteral("ok")).toBool());
    const QVariantMap backend = create.value(QStringLiteral("results")).toMap()
                                   .value(QStringLiteral("backend")).toMap();
    QCOMPARE(backend.value(QStringLiteral("backend")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(backend.value(QStringLiteral("applied")).toBool(), true);
    QVERIFY(backend.contains(QStringLiteral("reply")));

    const QVariantMap state = service.GetState();
    QVERIFY(state.value(QStringLiteral("ok")).toBool());
    const QVariantMap stateBackend = state.value(QStringLiteral("results")).toMap()
                                        .value(QStringLiteral("backend")).toMap();
    QCOMPARE(stateBackend.value(QStringLiteral("name")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(stateBackend.value(QStringLiteral("service")).toString(),
             QString::fromLatin1(kDefaultBackendService));

    bus.unregisterObject(QString::fromLatin1(kDefaultBackendPath));
    bus.unregisterService(QString::fromLatin1(kDefaultBackendService));

    if (prevHelper.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_HELPER");
    } else {
        qputenv("SLM_INPUTCAPTURE_HELPER", prevHelper);
    }
    if (prevDbusService.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_SERVICE", prevDbusService);
    }
    if (prevDbusPath.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_PATH", prevDbusPath);
    }
    if (prevDbusIface.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_IFACE", prevDbusIface);
    }
    if (prevSecurity.isEmpty()) {
        qunsetenv("SLM_SECURITY_DISABLED");
    } else {
        qputenv("SLM_SECURITY_DISABLED", prevSecurity);
    }
}

void InputCaptureServiceDbusTest::contract_nativeCompositorAutoDetectPath()
{
    const QByteArray prevHelper = qgetenv("SLM_INPUTCAPTURE_HELPER");
    const QByteArray prevDbusService = qgetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    const QByteArray prevDbusPath = qgetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    const QByteArray prevDbusIface = qgetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    const QByteArray prevSecurity = qgetenv("SLM_SECURITY_DISABLED");
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));
    qunsetenv("SLM_INPUTCAPTURE_HELPER");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_IFACE");

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }
    QDBusConnectionInterface *iface = bus.interface();
    QVERIFY(iface);

    if (iface->isServiceRegistered(QString::fromLatin1(kNativeBackendService)).value()) {
        bus.unregisterObject(QString::fromLatin1(kNativeBackendPath));
        bus.unregisterService(QString::fromLatin1(kNativeBackendService));
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kDefaultBackendService)).value()) {
        bus.unregisterObject(QString::fromLatin1(kDefaultBackendPath));
        bus.unregisterService(QString::fromLatin1(kDefaultBackendService));
    }

    FakeNativeInputCaptureBackendService nativeBackend;
    QVERIFY(bus.registerService(QString::fromLatin1(kNativeBackendService)));
    QVERIFY(bus.registerObject(QString::fromLatin1(kNativeBackendPath),
                               &nativeBackend,
                               QDBusConnection::ExportAllSlots));

    InputCaptureService service;
    const QString s = QStringLiteral("/org/desktop/portal/session/direct/ic_native_autodetect_1");
    const QVariantMap create = service.CreateSession(s, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(create.value(QStringLiteral("ok")).toBool());
    const QVariantMap backend = create.value(QStringLiteral("results")).toMap()
                                   .value(QStringLiteral("backend")).toMap();
    QCOMPARE(backend.value(QStringLiteral("backend")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(backend.value(QStringLiteral("applied")).toBool(), true);
    QVERIFY(backend.contains(QStringLiteral("reply")));

    const QVariantMap state = service.GetState();
    QVERIFY(state.value(QStringLiteral("ok")).toBool());
    const QVariantMap stateBackend = state.value(QStringLiteral("results")).toMap()
                                        .value(QStringLiteral("backend")).toMap();
    QCOMPARE(stateBackend.value(QStringLiteral("name")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(stateBackend.value(QStringLiteral("mode")).toString(),
             QStringLiteral("native-compositor-dbus"));
    QCOMPARE(stateBackend.value(QStringLiteral("service")).toString(),
             QString::fromLatin1(kNativeBackendService));

    bus.unregisterObject(QString::fromLatin1(kNativeBackendPath));
    bus.unregisterService(QString::fromLatin1(kNativeBackendService));

    if (prevHelper.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_HELPER");
    } else {
        qputenv("SLM_INPUTCAPTURE_HELPER", prevHelper);
    }
    if (prevDbusService.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_SERVICE", prevDbusService);
    }
    if (prevDbusPath.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_PATH", prevDbusPath);
    }
    if (prevDbusIface.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_IFACE", prevDbusIface);
    }
    if (prevSecurity.isEmpty()) {
        qunsetenv("SLM_SECURITY_DISABLED");
    } else {
        qputenv("SLM_SECURITY_DISABLED", prevSecurity);
    }
}

void InputCaptureServiceDbusTest::contract_realCompositorProviderEndToEndPath()
{
    const QByteArray prevHelper = qgetenv("SLM_INPUTCAPTURE_HELPER");
    const QByteArray prevDbusService = qgetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    const QByteArray prevDbusPath = qgetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    const QByteArray prevDbusIface = qgetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    const QByteArray prevSecurity = qgetenv("SLM_SECURITY_DISABLED");
    qputenv("SLM_SECURITY_DISABLED", QByteArrayLiteral("1"));
    qunsetenv("SLM_INPUTCAPTURE_HELPER");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    qunsetenv("SLM_INPUTCAPTURE_DBUS_IFACE");

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("session bus is not available in this test environment");
    }
    QDBusConnectionInterface *iface = bus.interface();
    QVERIFY(iface);

    if (iface->isServiceRegistered(QString::fromLatin1(kNativeBackendService)).value()) {
        QSKIP("native compositor backend service is already owned by another process");
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kDefaultBackendService)).value()) {
        bus.unregisterObject(QString::fromLatin1(kDefaultBackendPath));
        bus.unregisterService(QString::fromLatin1(kDefaultBackendService));
    }

    FakeInputCaptureCommandBridge commandBridge;
    CompositorInputCaptureBackendService compositorProvider(&commandBridge);
    if (!compositorProvider.serviceRegistered()) {
        QSKIP("cannot register real compositor input capture backend service");
    }

    InputCaptureService service;
    const QString s = QStringLiteral("/org/desktop/portal/session/direct/ic_compositor_e2e_1");
    const QVariantMap create = service.CreateSession(s, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(create.value(QStringLiteral("ok")).toBool());
    const QVariantMap backend = create.value(QStringLiteral("results")).toMap()
                                   .value(QStringLiteral("backend")).toMap();
    QCOMPARE(backend.value(QStringLiteral("backend")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(backend.value(QStringLiteral("applied")).toBool(), true);
    QVERIFY(compositorProvider.GetState().value(QStringLiteral("ok")).toBool());
    QCOMPARE(compositorProvider.GetState().value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("session_count")).toInt(),
             1);

    const QVariantList barriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("b1")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 0},
            {QStringLiteral("y2"), 250},
        }
    };
    const QVariantMap setBarriers = service.SetPointerBarriers(s, barriers, QVariantMap{});
    QVERIFY(setBarriers.value(QStringLiteral("ok")).toBool());
    QCOMPARE(setBarriers.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("barrier_count")).toInt(),
             1);
    const QVariantMap enable = service.EnableSession(s, QVariantMap{});
    QVERIFY(enable.value(QStringLiteral("ok")).toBool());
    const QVariantMap providerStateEnabled = compositorProvider.GetState();
    QVERIFY(providerStateEnabled.value(QStringLiteral("ok")).toBool());
    const QVariantMap providerEnabledResults = providerStateEnabled.value(QStringLiteral("results")).toMap();
    QCOMPARE(providerEnabledResults.value(QStringLiteral("enabled_count")).toInt(), 1);
    QCOMPARE(providerEnabledResults.value(QStringLiteral("active")).toBool(), true);
    QCOMPARE(providerEnabledResults.value(QStringLiteral("active_session")).toString(), s);
    QCOMPARE(compositorProvider.enabledSessionCount(), 1);
    QCOMPARE(compositorProvider.activeSession(), s);

    const QVariantMap disable = service.DisableSession(s, QVariantMap{});
    QVERIFY(disable.value(QStringLiteral("ok")).toBool());
    const QVariantMap providerStateDisabled = compositorProvider.GetState();
    QVERIFY(providerStateDisabled.value(QStringLiteral("ok")).toBool());
    const QVariantMap providerDisabledResults = providerStateDisabled.value(QStringLiteral("results")).toMap();
    QCOMPARE(providerDisabledResults.value(QStringLiteral("enabled_count")).toInt(), 0);
    QCOMPARE(providerDisabledResults.value(QStringLiteral("active")).toBool(), false);
    QCOMPARE(compositorProvider.enabledSessionCount(), 0);
    QVERIFY(compositorProvider.activeSession().isEmpty());

    const QVariantMap release = service.ReleaseSession(s, QStringLiteral("test-release"));
    QVERIFY(release.value(QStringLiteral("ok")).toBool());
    const QVariantMap providerStateReleased = compositorProvider.GetState();
    QVERIFY(providerStateReleased.value(QStringLiteral("ok")).toBool());
    QCOMPARE(providerStateReleased.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("session_count")).toInt(),
             0);

    const QVariantMap state = service.GetState();
    QVERIFY(state.value(QStringLiteral("ok")).toBool());
    const QVariantMap stateBackend = state.value(QStringLiteral("results")).toMap()
                                        .value(QStringLiteral("backend")).toMap();
    QCOMPARE(stateBackend.value(QStringLiteral("name")).toString(), QStringLiteral("dbus-direct"));
    QCOMPARE(stateBackend.value(QStringLiteral("mode")).toString(),
             QStringLiteral("native-compositor-dbus"));
    QCOMPARE(stateBackend.value(QStringLiteral("service")).toString(),
             QString::fromLatin1(kNativeBackendService));

    if (prevHelper.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_HELPER");
    } else {
        qputenv("SLM_INPUTCAPTURE_HELPER", prevHelper);
    }
    if (prevDbusService.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_SERVICE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_SERVICE", prevDbusService);
    }
    if (prevDbusPath.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_PATH");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_PATH", prevDbusPath);
    }
    if (prevDbusIface.isEmpty()) {
        qunsetenv("SLM_INPUTCAPTURE_DBUS_IFACE");
    } else {
        qputenv("SLM_INPUTCAPTURE_DBUS_IFACE", prevDbusIface);
    }
    if (prevSecurity.isEmpty()) {
        qunsetenv("SLM_SECURITY_DISABLED");
    } else {
        qputenv("SLM_SECURITY_DISABLED", prevSecurity);
    }
}

void InputCaptureServiceDbusTest::contract_realCompositorProviderCommandForwarding()
{
    FakeInputCaptureCommandBridge commandBridge;
    CompositorInputCaptureBackendService provider(&commandBridge);
    if (!provider.serviceRegistered()) {
        QSKIP("cannot register real compositor input capture backend service");
    }

    const QString session = QStringLiteral("/org/desktop/portal/session/direct/ic_forward_1");
    const QVariantMap created = provider.CreateSession(session, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(created.value(QStringLiteral("ok")).toBool());

    const QVariantList barriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("b1")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 0},
            {QStringLiteral("y2"), 250},
        }
    };
    const QVariantMap setBarriers = provider.SetPointerBarriers(session, barriers, QVariantMap{});
    QVERIFY(setBarriers.value(QStringLiteral("ok")).toBool());
    QCOMPARE(setBarriers.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_command_forwarded")).toBool(),
             true);
    const QVariantMap enabled = provider.EnableSession(session, QVariantMap{});
    QVERIFY(enabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(enabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_command_forwarded")).toBool(),
             true);
    const QVariantMap disabled = provider.DisableSession(session, QVariantMap{});
    QVERIFY(disabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(disabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_command_forwarded")).toBool(),
             true);
    const QVariantMap released = provider.ReleaseSession(session, QStringLiteral("test-release"));
    QVERIFY(released.value(QStringLiteral("ok")).toBool());
    QCOMPARE(released.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_command_forwarded")).toBool(),
             true);

    bool sawSetBarriers = false;
    for (const QString &cmd : commandBridge.commands()) {
        if (cmd.startsWith(QStringLiteral("inputcapture set-barriers "))) {
            sawSetBarriers = true;
            break;
        }
    }
    QVERIFY2(sawSetBarriers, "set-barriers command must be forwarded to compositor bridge");
    QVERIFY(commandBridge.commands().contains(QStringLiteral("inputcapture enable %1").arg(session)));
    QVERIFY(commandBridge.commands().contains(QStringLiteral("inputcapture disable %1").arg(session)));
    QVERIFY(commandBridge.commands().contains(QStringLiteral("inputcapture release %1").arg(session)));
}

void InputCaptureServiceDbusTest::contract_realCompositorProviderRequireCompositorCommandRollback()
{
    FakeInputCaptureRejectingCommandBridge commandBridge;
    CompositorInputCaptureBackendService provider(&commandBridge);
    if (!provider.serviceRegistered()) {
        QSKIP("cannot register real compositor input capture backend service");
    }

    const QString session = QStringLiteral("/org/desktop/portal/session/direct/ic_forward_rollback_1");
    const QVariantMap created = provider.CreateSession(session, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(created.value(QStringLiteral("ok")).toBool());

    const QVariantList barriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("b1")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 0},
            {QStringLiteral("y2"), 250},
        }
    };

    const QVariantMap setBarriers = provider.SetPointerBarriers(
        session,
        barriers,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(!setBarriers.value(QStringLiteral("ok")).toBool());
    QCOMPARE(setBarriers.value(QStringLiteral("error")).toString(), QStringLiteral("permission-denied"));
    QCOMPARE(setBarriers.value(QStringLiteral("reason")).toString(),
             QStringLiteral("compositor-bridge-unavailable"));

    const QVariantMap enabled = provider.EnableSession(
        session,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(!enabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(enabled.value(QStringLiteral("error")).toString(), QStringLiteral("permission-denied"));
    QCOMPARE(enabled.value(QStringLiteral("reason")).toString(),
             QStringLiteral("compositor-bridge-unavailable"));

    const QVariantMap state = provider.GetState();
    QVERIFY(state.value(QStringLiteral("ok")).toBool());
    const QVariantMap results = state.value(QStringLiteral("results")).toMap();
    const QVariantList sessions = results.value(QStringLiteral("sessions")).toList();
    QVERIFY(!sessions.isEmpty());
    const QVariantMap row = sessions.constFirst().toMap();
    QCOMPARE(row.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(row.value(QStringLiteral("barrier_count")).toInt(), 0);
}

void InputCaptureServiceDbusTest::contract_realCompositorProviderPrimitiveBridgePath()
{
    const QByteArray prevPrimitiveService = qgetenv("SLM_INPUTCAPTURE_PRIMITIVE_SERVICE");
    const QByteArray prevPrimitivePath = qgetenv("SLM_INPUTCAPTURE_PRIMITIVE_PATH");
    const QByteArray prevPrimitiveIface = qgetenv("SLM_INPUTCAPTURE_PRIMITIVE_IFACE");
    qputenv("SLM_INPUTCAPTURE_PRIMITIVE_SERVICE", QByteArray(kPrimitiveService));
    qputenv("SLM_INPUTCAPTURE_PRIMITIVE_PATH", QByteArray(kPrimitivePath));
    qputenv("SLM_INPUTCAPTURE_PRIMITIVE_IFACE", QByteArray(kPrimitiveIface));
    const auto restorePrimitiveEnv = [&]() {
        if (prevPrimitiveService.isEmpty()) {
            qunsetenv("SLM_INPUTCAPTURE_PRIMITIVE_SERVICE");
        } else {
            qputenv("SLM_INPUTCAPTURE_PRIMITIVE_SERVICE", prevPrimitiveService);
        }
        if (prevPrimitivePath.isEmpty()) {
            qunsetenv("SLM_INPUTCAPTURE_PRIMITIVE_PATH");
        } else {
            qputenv("SLM_INPUTCAPTURE_PRIMITIVE_PATH", prevPrimitivePath);
        }
        if (prevPrimitiveIface.isEmpty()) {
            qunsetenv("SLM_INPUTCAPTURE_PRIMITIVE_IFACE");
        } else {
            qputenv("SLM_INPUTCAPTURE_PRIMITIVE_IFACE", prevPrimitiveIface);
        }
    };

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        restorePrimitiveEnv();
        QSKIP("session bus is not available in this test environment");
    }
    QDBusConnectionInterface *iface = bus.interface();
    QVERIFY(iface);
    if (iface->isServiceRegistered(QString::fromLatin1(kPrimitiveService)).value()) {
        restorePrimitiveEnv();
        QSKIP("primitive test service name already owned by another process");
    }

    FakeInputCapturePrimitiveService primitive;
    QVERIFY(bus.registerService(QString::fromLatin1(kPrimitiveService)));
    QVERIFY(bus.registerObject(QString::fromLatin1(kPrimitivePath),
                               &primitive,
                               QDBusConnection::ExportAllSlots));

    FakeInputCaptureRejectingCommandBridge commandBridge;
    CompositorInputCaptureBackendService provider(&commandBridge);
    if (!provider.serviceRegistered()) {
        bus.unregisterObject(QString::fromLatin1(kPrimitivePath));
        bus.unregisterService(QString::fromLatin1(kPrimitiveService));
        restorePrimitiveEnv();
        QSKIP("cannot register real compositor input capture backend service");
    }

    const QString session = QStringLiteral("/org/desktop/portal/session/direct/ic_primitive_1");
    QVERIFY(provider.CreateSession(session, QStringLiteral("org.example.Test"), QVariantMap{})
                .value(QStringLiteral("ok")).toBool());

    const QVariantList barriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("b1")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 0},
            {QStringLiteral("y2"), 250},
        }
    };
    const QVariantMap setBarriers = provider.SetPointerBarriers(
        session,
        barriers,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(setBarriers.value(QStringLiteral("ok")).toBool());
    QCOMPARE(setBarriers.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(setBarriers.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("dbus"));
    QCOMPARE(primitive.lastSession(), session);
    QCOMPARE(primitive.barrierCount(), 1);

    const QVariantMap enabled = provider.EnableSession(
        session,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(enabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(enabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(enabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("dbus"));
    QCOMPARE(primitive.enabled(), true);

    const QVariantMap disabled = provider.DisableSession(
        session,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(disabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(disabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(disabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("dbus"));
    QCOMPARE(primitive.enabled(), false);

    const QVariantMap released = provider.ReleaseSession(session, QStringLiteral("test-release"));
    QVERIFY(released.value(QStringLiteral("ok")).toBool());
    QCOMPARE(released.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(released.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("dbus"));
    QCOMPARE(primitive.released(), true);

    bus.unregisterObject(QString::fromLatin1(kPrimitivePath));
    bus.unregisterService(QString::fromLatin1(kPrimitiveService));
    restorePrimitiveEnv();
}

void InputCaptureServiceDbusTest::contract_realCompositorProviderStructuredPrimitivePreferred()
{
    FakeStructuredProviderBridge commandBridge;
    CompositorInputCaptureBackendService provider(&commandBridge);
    if (!provider.serviceRegistered()) {
        QSKIP("cannot register real compositor input capture backend service");
    }

    const QString session = QStringLiteral("/org/desktop/portal/session/direct/ic_structured_native_1");
    const QVariantMap created = provider.CreateSession(session, QStringLiteral("org.example.Test"), QVariantMap{});
    QVERIFY(created.value(QStringLiteral("ok")).toBool());

    const QVariantList barriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("b1")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 0},
            {QStringLiteral("y2"), 250},
        }
    };
    const QVariantMap setBarriers = provider.SetPointerBarriers(
        session,
        barriers,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(setBarriers.value(QStringLiteral("ok")).toBool());
    QCOMPARE(setBarriers.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(setBarriers.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("structured"));
    QCOMPARE(commandBridge.lastSession(), session);
    QCOMPARE(commandBridge.barrierCount(), 1);

    const QVariantMap enabled = provider.EnableSession(
        session,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(enabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(enabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(enabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("structured"));
    QCOMPARE(commandBridge.enabled(), true);

    const QVariantMap disabled = provider.DisableSession(
        session,
        QVariantMap{{QStringLiteral("require_compositor_command"), true}});
    QVERIFY(disabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(disabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(disabled.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("structured"));
    QCOMPARE(commandBridge.enabled(), false);

    const QVariantMap released = provider.ReleaseSession(session, QStringLiteral("test-release"));
    QVERIFY(released.value(QStringLiteral("ok")).toBool());
    QCOMPARE(released.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_applied")).toBool(),
             true);
    QCOMPARE(released.value(QStringLiteral("results")).toMap()
                 .value(QStringLiteral("compositor_primitive_source")).toString(),
             QStringLiteral("structured"));
    QCOMPARE(commandBridge.released(), true);

    QCOMPARE(commandBridge.structuredCalls(), 4);
    QVERIFY(commandBridge.commands().contains(QStringLiteral("inputcapture enable %1").arg(session)));
}

void InputCaptureServiceDbusTest::contract_primitiveServiceStructuredBridgePreferred()
{
    FakeStructuredInputCaptureBridge bridge;
    CompositorInputCapturePrimitiveService primitive(&bridge);

    const QString session = QStringLiteral("/org/desktop/portal/session/direct/ic_structured_1");
    const QVariantList barriers{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("b1")},
            {QStringLiteral("x1"), 0},
            {QStringLiteral("y1"), 0},
            {QStringLiteral("x2"), 0},
            {QStringLiteral("y2"), 250},
        }
    };

    const QVariantMap setBarriers = primitive.SetPointerBarriers(session, barriers, QVariantMap{});
    QVERIFY(setBarriers.value(QStringLiteral("ok")).toBool());
    QCOMPARE(bridge.lastSession(), session);
    QCOMPARE(bridge.barrierCount(), 1);

    const QVariantMap enabled = primitive.EnableSession(session, QVariantMap{});
    QVERIFY(enabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(bridge.enabled(), true);

    const QVariantMap disabled = primitive.DisableSession(session, QVariantMap{});
    QVERIFY(disabled.value(QStringLiteral("ok")).toBool());
    QCOMPARE(bridge.enabled(), false);

    const QVariantMap released = primitive.ReleaseSession(session, QStringLiteral("test-release"));
    QVERIFY(released.value(QStringLiteral("ok")).toBool());
    QCOMPARE(bridge.released(), true);

    QCOMPARE(bridge.structuredCalls(), 4);
    QCOMPARE(bridge.commands().size(), 0);
}

QTEST_MAIN(InputCaptureServiceDbusTest)
#include "inputcaptureservice_dbus_test.moc"
