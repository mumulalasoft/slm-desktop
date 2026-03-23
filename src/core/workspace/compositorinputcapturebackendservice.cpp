#include "compositorinputcapturebackendservice.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcessEnvironment>

namespace {
constexpr const char kService[] = "org.slm.Compositor.InputCaptureBackend";
constexpr const char kPath[] = "/org/slm/Compositor/InputCaptureBackend";
constexpr const char kDefaultPrimitivePath[] = "/org/slm/Compositor/InputCapture";
constexpr const char kDefaultPrimitiveIface[] = "org.slm.Compositor.InputCapture";
}

CompositorInputCaptureBackendService::CompositorInputCaptureBackendService(
    QObject *commandBridge,
    QObject *parent)
    : QObject(parent)
    , m_commandBridge(commandBridge)
{
    registerDbusService();
}

CompositorInputCaptureBackendService::~CompositorInputCaptureBackendService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool CompositorInputCaptureBackendService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString CompositorInputCaptureBackendService::activeSession() const
{
    return m_activeSession;
}

int CompositorInputCaptureBackendService::enabledSessionCount() const
{
    return m_enabledSessionCount;
}

QVariantMap CompositorInputCaptureBackendService::CreateSession(const QString &sessionPath,
                                                                const QString &appId,
                                                                const QVariantMap &options)
{
    Q_UNUSED(options)
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return deny(QStringLiteral("missing-session-path"));
    }
    SessionRecord record = m_sessions.value(normalized);
    record.sessionHandle = normalized;
    record.appId = appId.trimmed();
    m_sessions.insert(normalized, record);
    return ok({
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("created"), true},
    });
}

QVariantMap CompositorInputCaptureBackendService::SetPointerBarriers(const QString &sessionPath,
                                                                     const QVariantList &barriers,
                                                                     const QVariantMap &options)
{
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return deny(QStringLiteral("missing-session-path"));
    }
    if (!m_sessions.contains(normalized)) {
        return deny(QStringLiteral("missing-session"));
    }
    QString reason;
    if (!validateBarriers(barriers, &reason)) {
        return deny(reason);
    }
    const bool requireCompositorCommand =
        options.value(QStringLiteral("require_compositor_command")).toBool();
    const QVariantMap primitiveReply = invokePrimitiveOperation(
        QStringLiteral("SetPointerBarriers"),
        QVariantList{normalized, barriers, options});
    const bool primitiveConfigured =
        primitiveReply.value(QStringLiteral("configured")).toBool();
    const bool primitiveApplied = primitiveReply.value(QStringLiteral("ok")).toBool();
    const QString primitiveSource = primitiveReply.value(QStringLiteral("source")).toString();
    SessionRecord record = m_sessions.value(normalized);
    const QVariantList previousBarriers = record.barriers;
    record.barriers = barriers;
    m_sessions.insert(normalized, record);
    emit SessionStateChanged(normalized, record.enabled, record.barriers.size());

    QJsonArray barrierArray;
    for (const QVariant &entry : barriers) {
        barrierArray.push_back(QJsonObject::fromVariantMap(entry.toMap()));
    }
    const QJsonObject payload{
        {QStringLiteral("session"), normalized},
        {QStringLiteral("barriers"), barrierArray},
    };
    const bool forwarded = forwardCommand(QStringLiteral("inputcapture set-barriers %1")
                                              .arg(QString::fromUtf8(QJsonDocument(payload)
                                                                         .toJson(QJsonDocument::Compact))));

    QVariantMap results{
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("barriers_set"), true},
        {QStringLiteral("barrier_count"), barriers.size()},
        {QStringLiteral("compositor_command_forwarded"), forwarded},
    };
    if (!forwarded && !primitiveApplied) {
        if (requireCompositorCommand) {
            SessionRecord rollback = m_sessions.value(normalized);
            rollback.barriers = previousBarriers;
            m_sessions.insert(normalized, rollback);
            emit SessionStateChanged(normalized, rollback.enabled, rollback.barriers.size());
            return deny(QStringLiteral("compositor-bridge-unavailable"),
                        {{QStringLiteral("session_handle"), normalized},
                         {QStringLiteral("compositor_command"), QStringLiteral("set-barriers")},
                         {QStringLiteral("compositor_primitive"), QStringLiteral("SetPointerBarriers")},
                         {QStringLiteral("requires_native_binding"), true}});
        }
        results.insert(QStringLiteral("compositor_command_reason"),
                       QStringLiteral("command-unsupported-or-bridge-missing"));
    }
    results.insert(QStringLiteral("compositor_primitive_configured"), primitiveConfigured);
    results.insert(QStringLiteral("compositor_primitive_applied"), primitiveApplied);
    if (!primitiveSource.trimmed().isEmpty()) {
        results.insert(QStringLiteral("compositor_primitive_source"), primitiveSource);
    }
    if (primitiveConfigured && !primitiveApplied) {
        results.insert(QStringLiteral("compositor_primitive_reason"),
                       primitiveReply.value(QStringLiteral("reason")).toString());
    }
    return ok(results);
}

QVariantMap CompositorInputCaptureBackendService::EnableSession(const QString &sessionPath,
                                                                const QVariantMap &options)
{
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return deny(QStringLiteral("missing-session-path"));
    }
    if (!m_sessions.contains(normalized)) {
        return deny(QStringLiteral("missing-session"));
    }
    const bool allowPreempt = options.value(QStringLiteral("allow_preempt")).toBool();
    const bool requireCompositorCommand =
        options.value(QStringLiteral("require_compositor_command")).toBool();
    const QVariantMap primitiveReply = invokePrimitiveOperation(
        QStringLiteral("EnableSession"),
        QVariantList{normalized, options});
    const bool primitiveConfigured =
        primitiveReply.value(QStringLiteral("configured")).toBool();
    const bool primitiveApplied = primitiveReply.value(QStringLiteral("ok")).toBool();
    const QString primitiveSource = primitiveReply.value(QStringLiteral("source")).toString();
    QString active;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        if (it.key() == normalized) {
            continue;
        }
        if (it->enabled) {
            active = it.key();
            break;
        }
    }
    if (!active.isEmpty() && !allowPreempt) {
        return deny(QStringLiteral("conflict-active-session"),
                    {{QStringLiteral("conflict_session"), active},
                     {QStringLiteral("requires_mediation"), true},
                     {QStringLiteral("mediation_action"),
                      QStringLiteral("inputcapture-preempt-consent")},
                     {QStringLiteral("mediation_scope"), QStringLiteral("session-conflict")}});
    }
    if (!active.isEmpty() && allowPreempt) {
        SessionRecord previous = m_sessions.value(active);
        previous.enabled = false;
        m_sessions.insert(active, previous);
        emit SessionStateChanged(active, false, previous.barriers.size());
    }

    SessionRecord current = m_sessions.value(normalized);
    current.enabled = true;
    m_sessions.insert(normalized, current);
    emit SessionStateChanged(normalized, true, current.barriers.size());
    setActiveSession(normalized);
    setEnabledSessionCount(recomputeEnabledSessionCount());
    emit CaptureActiveChanged(true, m_activeSession);

    const bool forwarded = forwardCommand(QStringLiteral("inputcapture enable %1").arg(normalized));
    if (!forwarded && !primitiveApplied && requireCompositorCommand) {
        SessionRecord rollback = m_sessions.value(normalized);
        rollback.enabled = false;
        m_sessions.insert(normalized, rollback);
        if (!active.isEmpty() && allowPreempt) {
            SessionRecord prevRollback = m_sessions.value(active);
            prevRollback.enabled = true;
            m_sessions.insert(active, prevRollback);
            emit SessionStateChanged(active, true, prevRollback.barriers.size());
            setActiveSession(active);
        } else {
            setActiveSession(firstEnabledSessionKey());
        }
        setEnabledSessionCount(recomputeEnabledSessionCount());
        emit SessionStateChanged(normalized, false, rollback.barriers.size());
        emit CaptureActiveChanged(!m_activeSession.isEmpty(), m_activeSession);
        return deny(QStringLiteral("compositor-bridge-unavailable"),
                    {{QStringLiteral("session_handle"), normalized},
                     {QStringLiteral("compositor_command"), QStringLiteral("enable")},
                     {QStringLiteral("compositor_primitive"), QStringLiteral("EnableSession")},
                     {QStringLiteral("requires_native_binding"), true}});
    }

    QVariantMap results{
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("compositor_command_forwarded"), forwarded},
        {QStringLiteral("compositor_primitive_configured"), primitiveConfigured},
        {QStringLiteral("compositor_primitive_applied"), primitiveApplied},
    };
    if (!primitiveSource.trimmed().isEmpty()) {
        results.insert(QStringLiteral("compositor_primitive_source"), primitiveSource);
    }
    if (!forwarded && !primitiveApplied) {
        results.insert(QStringLiteral("compositor_command_reason"),
                       QStringLiteral("command-unsupported-or-bridge-missing"));
    }
    if (primitiveConfigured && !primitiveApplied) {
        results.insert(QStringLiteral("compositor_primitive_reason"),
                       primitiveReply.value(QStringLiteral("reason")).toString());
    }
    if (!active.isEmpty() && allowPreempt) {
        results.insert(QStringLiteral("preempted"), true);
        results.insert(QStringLiteral("preempted_session"), active);
    }
    return ok(results);
}

QVariantMap CompositorInputCaptureBackendService::DisableSession(const QString &sessionPath,
                                                                 const QVariantMap &options)
{
    const QString normalized = sessionPath.trimmed();
    const bool requireCompositorCommand =
        options.value(QStringLiteral("require_compositor_command")).toBool();
    const QVariantMap primitiveReply = invokePrimitiveOperation(
        QStringLiteral("DisableSession"),
        QVariantList{normalized, options});
    const bool primitiveConfigured =
        primitiveReply.value(QStringLiteral("configured")).toBool();
    const bool primitiveApplied = primitiveReply.value(QStringLiteral("ok")).toBool();
    const QString primitiveSource = primitiveReply.value(QStringLiteral("source")).toString();
    if (normalized.isEmpty()) {
        return deny(QStringLiteral("missing-session-path"));
    }
    if (!m_sessions.contains(normalized)) {
        return deny(QStringLiteral("missing-session"));
    }
    SessionRecord record = m_sessions.value(normalized);
    const bool previouslyEnabled = record.enabled;
    record.enabled = false;
    m_sessions.insert(normalized, record);
    emit SessionStateChanged(normalized, false, record.barriers.size());

    if (m_activeSession == normalized) {
        setActiveSession(firstEnabledSessionKey());
    }
    setEnabledSessionCount(recomputeEnabledSessionCount());
    emit CaptureActiveChanged(!m_activeSession.isEmpty(), m_activeSession);

    const bool forwarded = forwardCommand(QStringLiteral("inputcapture disable %1").arg(normalized));
    if (!forwarded && !primitiveApplied && requireCompositorCommand) {
        SessionRecord rollback = m_sessions.value(normalized);
        rollback.enabled = previouslyEnabled;
        m_sessions.insert(normalized, rollback);
        emit SessionStateChanged(normalized, rollback.enabled, rollback.barriers.size());
        if (previouslyEnabled) {
            setActiveSession(normalized);
        }
        setEnabledSessionCount(recomputeEnabledSessionCount());
        emit CaptureActiveChanged(!m_activeSession.isEmpty(), m_activeSession);
        QVariantMap denyResults{
            {QStringLiteral("session_handle"), normalized},
            {QStringLiteral("compositor_command"), QStringLiteral("disable")},
            {QStringLiteral("compositor_primitive"), QStringLiteral("DisableSession")},
            {QStringLiteral("requires_native_binding"), true},
        };
        return deny(QStringLiteral("compositor-bridge-unavailable"), denyResults);
    }

    QVariantMap results{
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("disabled"), true},
        {QStringLiteral("compositor_command_forwarded"), forwarded},
        {QStringLiteral("compositor_primitive_configured"), primitiveConfigured},
        {QStringLiteral("compositor_primitive_applied"), primitiveApplied},
    };
    if (!primitiveSource.trimmed().isEmpty()) {
        results.insert(QStringLiteral("compositor_primitive_source"), primitiveSource);
    }
    if (!forwarded && !primitiveApplied) {
        results.insert(QStringLiteral("compositor_command_reason"),
                       QStringLiteral("command-unsupported-or-bridge-missing"));
    }
    if (primitiveConfigured && !primitiveApplied) {
        results.insert(QStringLiteral("compositor_primitive_reason"),
                       primitiveReply.value(QStringLiteral("reason")).toString());
    }
    return ok(results);
}

QVariantMap CompositorInputCaptureBackendService::ReleaseSession(const QString &sessionPath,
                                                                 const QString &reason)
{
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return deny(QStringLiteral("missing-session-path"));
    }
    const QVariantMap primitiveReply = invokePrimitiveOperation(
        QStringLiteral("ReleaseSession"),
        QVariantList{normalized, reason});
    const bool primitiveConfigured =
        primitiveReply.value(QStringLiteral("configured")).toBool();
    const bool primitiveApplied = primitiveReply.value(QStringLiteral("ok")).toBool();
    const QString primitiveSource = primitiveReply.value(QStringLiteral("source")).toString();
    const bool existed = m_sessions.remove(normalized) > 0;
    if (existed && m_activeSession == normalized) {
        setActiveSession(firstEnabledSessionKey());
    }
    if (existed) {
        setEnabledSessionCount(recomputeEnabledSessionCount());
        emit CaptureActiveChanged(!m_activeSession.isEmpty(), m_activeSession);
    }

    const bool forwarded = forwardCommand(QStringLiteral("inputcapture release %1").arg(normalized));

    QVariantMap results{
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("released"), true},
        {QStringLiteral("had_session"), existed},
        {QStringLiteral("compositor_command_forwarded"), forwarded},
        {QStringLiteral("compositor_primitive_configured"), primitiveConfigured},
        {QStringLiteral("compositor_primitive_applied"), primitiveApplied},
        {QStringLiteral("reason"),
         reason.trimmed().isEmpty() ? QStringLiteral("released") : reason.trimmed()},
    };
    if (!primitiveSource.trimmed().isEmpty()) {
        results.insert(QStringLiteral("compositor_primitive_source"), primitiveSource);
    }
    if (!forwarded && !primitiveApplied) {
        results.insert(QStringLiteral("compositor_command_reason"),
                       QStringLiteral("command-unsupported-or-bridge-missing"));
    }
    if (primitiveConfigured && !primitiveApplied) {
        results.insert(QStringLiteral("compositor_primitive_reason"),
                       primitiveReply.value(QStringLiteral("reason")).toString());
    }
    return ok(results);
}

QVariantMap CompositorInputCaptureBackendService::GetState() const
{
    QVariantList sessions;
    int enabledCount = 0;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        QVariantMap entry{
            {QStringLiteral("session_handle"), it->sessionHandle},
            {QStringLiteral("app_id"), it->appId},
            {QStringLiteral("enabled"), it->enabled},
            {QStringLiteral("barrier_count"), it->barriers.size()},
            {QStringLiteral("barriers"), it->barriers},
        };
        if (it->enabled) {
            ++enabledCount;
        }
        sessions.push_back(entry);
    }
    return ok({
        {QStringLiteral("session_count"), m_sessions.size()},
        {QStringLiteral("enabled_count"), enabledCount},
        {QStringLiteral("active"), !m_activeSession.isEmpty()},
        {QStringLiteral("active_session"), m_activeSession},
        {QStringLiteral("compositor_command_supported"),
         bridgeHasCapability(QStringLiteral("command.inputcapture.enable"))
             && bridgeHasCapability(QStringLiteral("command.inputcapture.disable"))
             && bridgeHasCapability(QStringLiteral("command.inputcapture.release"))},
        {QStringLiteral("compositor_command_capabilities"),
         QVariantMap{
             {QStringLiteral("enable"),
              bridgeHasCapability(QStringLiteral("command.inputcapture.enable"))},
             {QStringLiteral("disable"),
              bridgeHasCapability(QStringLiteral("command.inputcapture.disable"))},
             {QStringLiteral("release"),
              bridgeHasCapability(QStringLiteral("command.inputcapture.release"))},
         }},
        {QStringLiteral("sessions"), sessions},
        {QStringLiteral("backend"), QStringLiteral("compositor-native")},
    });
}

void CompositorInputCaptureBackendService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    const bool okRegistered = bus.registerObject(QString::fromLatin1(kPath),
                                                 this,
                                                 QDBusConnection::ExportAllSlots |
                                                     QDBusConnection::ExportAllSignals |
                                                     QDBusConnection::ExportAllProperties);
    if (!okRegistered) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

QVariantMap CompositorInputCaptureBackendService::ok(const QVariantMap &results) const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("response"), 0u},
        {QStringLiteral("results"), results},
    };
}

QVariantMap CompositorInputCaptureBackendService::deny(const QString &reason,
                                                       const QVariantMap &results) const
{
    QVariantMap out = results;
    out.insert(QStringLiteral("reason"), reason.trimmed());
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("response"), 2u},
        {QStringLiteral("error"), QStringLiteral("permission-denied")},
        {QStringLiteral("reason"), reason.trimmed()},
        {QStringLiteral("results"), out},
    };
}

bool CompositorInputCaptureBackendService::validateBarriers(const QVariantList &barriers,
                                                            QString *reasonOut) const
{
    for (const QVariant &entry : barriers) {
        const QVariantMap b = entry.toMap();
        if (b.value(QStringLiteral("id")).toString().trimmed().isEmpty()) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("missing-barrier-id");
            }
            return false;
        }
        bool okX1 = false;
        bool okY1 = false;
        bool okX2 = false;
        bool okY2 = false;
        const int x1 = b.value(QStringLiteral("x1")).toInt(&okX1);
        const int y1 = b.value(QStringLiteral("y1")).toInt(&okY1);
        const int x2 = b.value(QStringLiteral("x2")).toInt(&okX2);
        const int y2 = b.value(QStringLiteral("y2")).toInt(&okY2);
        if (!(okX1 && okY1 && okX2 && okY2)) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("invalid-barrier-coordinate");
            }
            return false;
        }
        if (x1 == x2 && y1 == y2) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("zero-length-barrier");
            }
            return false;
        }
        if (x1 != x2 && y1 != y2) {
            if (reasonOut) {
                *reasonOut = QStringLiteral("barrier-must-be-axis-aligned");
            }
            return false;
        }
    }
    return true;
}

bool CompositorInputCaptureBackendService::forwardCommand(const QString &command) const
{
    const QString normalized = command.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    if (!m_commandBridge) {
        if (debugEnabled()) {
            qInfo().noquote() << "[inputcapture][compositor-provider] bridge-missing command="
                              << normalized;
        }
        return false;
    }
    bool accepted = false;
    const bool invoked = QMetaObject::invokeMethod(m_commandBridge,
                                                   "sendCommand",
                                                   Qt::DirectConnection,
                                                   Q_RETURN_ARG(bool, accepted),
                                                   Q_ARG(QString, normalized));
    if (debugEnabled()) {
        qInfo().noquote() << "[inputcapture][compositor-provider] forward command="
                          << normalized << "invoked=" << invoked << "accepted=" << accepted;
    }
    return invoked && accepted;
}

bool CompositorInputCaptureBackendService::debugEnabled() const
{
    return QProcessEnvironment::systemEnvironment()
               .value(QStringLiteral("SLM_INPUTCAPTURE_DEBUG"))
               .toInt()
        > 0;
}

bool CompositorInputCaptureBackendService::bridgeHasCapability(const QString &name) const
{
    const QString key = name.trimmed();
    if (key.isEmpty() || !m_commandBridge) {
        return false;
    }
    bool supported = false;
    const bool invoked = QMetaObject::invokeMethod(m_commandBridge,
                                                   "hasCapability",
                                                   Qt::DirectConnection,
                                                   Q_RETURN_ARG(bool, supported),
                                                   Q_ARG(QString, key));
    if (!invoked) {
        return false;
    }
    return supported;
}

QVariantMap CompositorInputCaptureBackendService::invokePrimitiveOperation(
    const QString &method,
    const QVariantList &args) const
{
    const QString op = method.trimmed();
    if (op.isEmpty()) {
        return {{QStringLiteral("configured"), false},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("none")},
                {QStringLiteral("reason"), QStringLiteral("missing-operation")}};
    }

    const QVariantMap structured = invokeStructuredPrimitiveOperation(op, args);
    if (structured.value(QStringLiteral("configured")).toBool()) {
        return structured;
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString service = env.value(QStringLiteral("SLM_INPUTCAPTURE_PRIMITIVE_SERVICE")).trimmed();
    const QString path = env.value(QStringLiteral("SLM_INPUTCAPTURE_PRIMITIVE_PATH"),
                                   QString::fromLatin1(kDefaultPrimitivePath)).trimmed();
    const QString iface = env.value(QStringLiteral("SLM_INPUTCAPTURE_PRIMITIVE_IFACE"),
                                    QString::fromLatin1(kDefaultPrimitiveIface)).trimmed();
    if (service.isEmpty()) {
        service = QString::fromLatin1(kService);
    }
    if (path.isEmpty() || iface.isEmpty()) {
        return {{QStringLiteral("configured"), false},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("none")},
                {QStringLiteral("reason"), QStringLiteral("primitive-not-configured")}};
    }
    if (service == QString::fromLatin1(kService)
        && path == QString::fromLatin1(kPath)
        && iface == QStringLiteral("org.slm.Compositor.InputCaptureBackend")) {
        return {{QStringLiteral("configured"), true},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("dbus")},
                {QStringLiteral("reason"), QStringLiteral("primitive-self-recursion")}};
    }
    QDBusInterface primitive(service, path, iface, QDBusConnection::sessionBus());
    if (!primitive.isValid()) {
        return {{QStringLiteral("configured"), true},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("dbus")},
                {QStringLiteral("reason"), QStringLiteral("primitive-interface-invalid")}};
    }
    QDBusMessage reply = primitive.callWithArgumentList(QDBus::Block, op, args);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        return {{QStringLiteral("configured"), true},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("dbus")},
                {QStringLiteral("reason"), reply.errorMessage().trimmed().isEmpty()
                                               ? QStringLiteral("primitive-call-failed")
                                               : reply.errorMessage().trimmed()}};
    }
    if (!reply.arguments().isEmpty()) {
        const QVariantMap asMap = reply.arguments().constFirst().toMap();
        if (!asMap.isEmpty()) {
            const bool ok = asMap.value(QStringLiteral("ok"), false).toBool();
            return {{QStringLiteral("configured"), true},
                    {QStringLiteral("ok"), ok},
                    {QStringLiteral("source"), QStringLiteral("dbus")},
                    {QStringLiteral("reason"), asMap.value(QStringLiteral("reason")).toString()},
                    {QStringLiteral("reply"), asMap}};
        }
    }
    return {{QStringLiteral("configured"), true},
            {QStringLiteral("ok"), true},
            {QStringLiteral("source"), QStringLiteral("dbus")},
            {QStringLiteral("reason"), QString()}};
}

QVariantMap CompositorInputCaptureBackendService::invokeStructuredPrimitiveOperation(
    const QString &method,
    const QVariantList &args) const
{
    if (!m_commandBridge) {
        return {{QStringLiteral("configured"), false},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("none")},
                {QStringLiteral("reason"), QStringLiteral("bridge-missing")}};
    }

    QVariantMap reply;
    bool invoked = false;
    const QString op = method.trimmed();
    if (op == QStringLiteral("SetPointerBarriers") && args.size() == 3) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "setInputCapturePointerBarriers",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QVariantList, args.at(1).toList()),
            Q_ARG(QVariantMap, args.at(2).toMap()));
    } else if (op == QStringLiteral("EnableSession") && args.size() == 2) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "enableInputCaptureSession",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QVariantMap, args.at(1).toMap()));
    } else if (op == QStringLiteral("DisableSession") && args.size() == 2) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "disableInputCaptureSession",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QVariantMap, args.at(1).toMap()));
    } else if (op == QStringLiteral("ReleaseSession") && args.size() == 2) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "releaseInputCaptureSession",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QString, args.at(1).toString()));
    }

    if (!invoked) {
        return {{QStringLiteral("configured"), false},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("none")},
                {QStringLiteral("reason"), QStringLiteral("structured-method-unavailable")}};
    }
    if (reply.isEmpty()) {
        return {{QStringLiteral("configured"), true},
                {QStringLiteral("ok"), false},
                {QStringLiteral("source"), QStringLiteral("structured")},
                {QStringLiteral("reason"), QStringLiteral("structured-empty-reply")}};
    }

    return {{QStringLiteral("configured"), true},
            {QStringLiteral("ok"), reply.value(QStringLiteral("ok"), false).toBool()},
            {QStringLiteral("source"), QStringLiteral("structured")},
            {QStringLiteral("reason"), reply.value(QStringLiteral("reason")).toString()},
            {QStringLiteral("reply"), reply}};
}

void CompositorInputCaptureBackendService::setActiveSession(const QString &sessionHandle)
{
    const QString normalized = sessionHandle.trimmed();
    if (m_activeSession == normalized) {
        return;
    }
    m_activeSession = normalized;
    emit activeSessionChanged();
}

void CompositorInputCaptureBackendService::setEnabledSessionCount(int count)
{
    const int normalized = qMax(0, count);
    if (m_enabledSessionCount == normalized) {
        return;
    }
    m_enabledSessionCount = normalized;
    emit enabledSessionCountChanged();
}

int CompositorInputCaptureBackendService::recomputeEnabledSessionCount() const
{
    int count = 0;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        if (it->enabled) {
            ++count;
        }
    }
    return count;
}

QString CompositorInputCaptureBackendService::firstEnabledSessionKey() const
{
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        if (it->enabled) {
            return it.key();
        }
    }
    return {};
}
