#include "compositorinputcaptureprimitiveservice.h"

#include <QDBusConnection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

namespace {
constexpr const char kPath[] = "/org/slm/Compositor/InputCapture";
}

CompositorInputCapturePrimitiveService::CompositorInputCapturePrimitiveService(
    QObject *commandBridge,
    QObject *parent)
    : QObject(parent)
    , m_commandBridge(commandBridge)
{
    registerDbusObject();
}

CompositorInputCapturePrimitiveService::~CompositorInputCapturePrimitiveService()
{
    if (!m_objectRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
}

bool CompositorInputCapturePrimitiveService::objectRegistered() const
{
    return m_objectRegistered;
}

QVariantMap CompositorInputCapturePrimitiveService::SetPointerBarriers(const QString &sessionPath,
                                                                       const QVariantList &barriers,
                                                                       const QVariantMap &options)
{
    Q_UNUSED(options)
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return fail(QStringLiteral("missing-session-path"));
    }
    QString reason;
    if (!validateBarriers(barriers, &reason)) {
        return fail(reason);
    }

    QJsonArray barrierArray;
    for (const QVariant &entry : barriers) {
        barrierArray.push_back(QJsonObject::fromVariantMap(entry.toMap()));
    }
    const QJsonObject payload{
        {QStringLiteral("session"), normalized},
        {QStringLiteral("barriers"), barrierArray},
    };
    const QVariantMap structured = invokeStructured(
        QStringLiteral("setInputCapturePointerBarriers"),
        QVariantList{normalized, barriers, options});
    const bool applied = structured.value(QStringLiteral("ok")).toBool()
        || forwardCommand(QStringLiteral("inputcapture set-barriers %1")
                              .arg(QString::fromUtf8(QJsonDocument(payload)
                                                         .toJson(QJsonDocument::Compact))));
    if (!applied) {
        return fail(QStringLiteral("command-rejected"),
                    {{QStringLiteral("session_handle"), normalized},
                     {QStringLiteral("command"), QStringLiteral("set-barriers")}});
    }
    return ok({
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("barrier_count"), barriers.size()},
        {QStringLiteral("applied"), true},
    });
}

QVariantMap CompositorInputCapturePrimitiveService::EnableSession(const QString &sessionPath,
                                                                  const QVariantMap &options)
{
    Q_UNUSED(options)
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return fail(QStringLiteral("missing-session-path"));
    }
    const QVariantMap structured = invokeStructured(QStringLiteral("enableInputCaptureSession"),
                                                    QVariantList{normalized, options});
    const bool applied = structured.value(QStringLiteral("ok")).toBool()
        || forwardCommand(QStringLiteral("inputcapture enable %1").arg(normalized));
    if (!applied) {
        return fail(QStringLiteral("command-rejected"),
                    {{QStringLiteral("session_handle"), normalized},
                     {QStringLiteral("command"), QStringLiteral("enable")}});
    }
    return ok({
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("applied"), true},
    });
}

QVariantMap CompositorInputCapturePrimitiveService::DisableSession(const QString &sessionPath,
                                                                   const QVariantMap &options)
{
    Q_UNUSED(options)
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return fail(QStringLiteral("missing-session-path"));
    }
    const QVariantMap structured = invokeStructured(QStringLiteral("disableInputCaptureSession"),
                                                    QVariantList{normalized, options});
    const bool applied = structured.value(QStringLiteral("ok")).toBool()
        || forwardCommand(QStringLiteral("inputcapture disable %1").arg(normalized));
    if (!applied) {
        return fail(QStringLiteral("command-rejected"),
                    {{QStringLiteral("session_handle"), normalized},
                     {QStringLiteral("command"), QStringLiteral("disable")}});
    }
    return ok({
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("disabled"), true},
        {QStringLiteral("applied"), true},
    });
}

QVariantMap CompositorInputCapturePrimitiveService::ReleaseSession(const QString &sessionPath,
                                                                   const QString &reason)
{
    Q_UNUSED(reason)
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return fail(QStringLiteral("missing-session-path"));
    }
    const QVariantMap structured = invokeStructured(QStringLiteral("releaseInputCaptureSession"),
                                                    QVariantList{normalized, reason});
    const bool applied = structured.value(QStringLiteral("ok")).toBool()
        || forwardCommand(QStringLiteral("inputcapture release %1").arg(normalized));
    if (!applied) {
        return fail(QStringLiteral("command-rejected"),
                    {{QStringLiteral("session_handle"), normalized},
                     {QStringLiteral("command"), QStringLiteral("release")}});
    }
    return ok({
        {QStringLiteral("session_handle"), normalized},
        {QStringLiteral("released"), true},
        {QStringLiteral("applied"), true},
    });
}

QVariantMap CompositorInputCapturePrimitiveService::GetState() const
{
    return ok({
        {QStringLiteral("object_path"), QString::fromLatin1(kPath)},
        {QStringLiteral("object_registered"), m_objectRegistered},
    });
}

void CompositorInputCapturePrimitiveService::registerDbusObject()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_objectRegistered = false;
        emit objectRegisteredChanged();
        return;
    }
    m_objectRegistered = bus.registerObject(QString::fromLatin1(kPath),
                                            this,
                                            QDBusConnection::ExportAllSlots |
                                                QDBusConnection::ExportAllSignals |
                                                QDBusConnection::ExportAllProperties);
    emit objectRegisteredChanged();
}

QVariantMap CompositorInputCapturePrimitiveService::ok(const QVariantMap &results) const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("results"), results},
    };
}

QVariantMap CompositorInputCapturePrimitiveService::fail(const QString &reason,
                                                         const QVariantMap &results) const
{
    QVariantMap out = results;
    out.insert(QStringLiteral("reason"), reason.trimmed());
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("primitive-failed")},
        {QStringLiteral("reason"), reason.trimmed()},
        {QStringLiteral("results"), out},
    };
}

QVariantMap CompositorInputCapturePrimitiveService::invokeStructured(const QString &method,
                                                                     const QVariantList &args) const
{
    if (!m_commandBridge) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("reason"), QStringLiteral("bridge-missing")}};
    }
    QVariantMap reply;
    bool invoked = false;
    const QString normalized = method.trimmed();
    if (normalized == QStringLiteral("setInputCapturePointerBarriers") && args.size() == 3) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "setInputCapturePointerBarriers",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QVariantList, args.at(1).toList()),
            Q_ARG(QVariantMap, args.at(2).toMap()));
    } else if (normalized == QStringLiteral("enableInputCaptureSession") && args.size() == 2) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "enableInputCaptureSession",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QVariantMap, args.at(1).toMap()));
    } else if (normalized == QStringLiteral("disableInputCaptureSession") && args.size() == 2) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "disableInputCaptureSession",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QVariantMap, args.at(1).toMap()));
    } else if (normalized == QStringLiteral("releaseInputCaptureSession") && args.size() == 2) {
        invoked = QMetaObject::invokeMethod(
            m_commandBridge,
            "releaseInputCaptureSession",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, reply),
            Q_ARG(QString, args.at(0).toString()),
            Q_ARG(QString, args.at(1).toString()));
    }
    if (!invoked) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("reason"), QStringLiteral("structured-method-unavailable")}};
    }
    if (reply.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("reason"), QStringLiteral("structured-empty-reply")}};
    }
    return reply;
}

bool CompositorInputCapturePrimitiveService::forwardCommand(const QString &command) const
{
    const QString normalized = command.trimmed();
    if (normalized.isEmpty() || !m_commandBridge) {
        return false;
    }
    bool accepted = false;
    const bool invoked = QMetaObject::invokeMethod(m_commandBridge,
                                                   "sendCommand",
                                                   Qt::DirectConnection,
                                                   Q_RETURN_ARG(bool, accepted),
                                                   Q_ARG(QString, normalized));
    return invoked && accepted;
}

bool CompositorInputCapturePrimitiveService::validateBarriers(const QVariantList &barriers,
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
