#include "inputcapturebackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QProcessEnvironment>

namespace {
constexpr const char kDefaultDbusService[] = "org.slm.Desktop.InputCaptureBackend";
constexpr const char kDefaultDbusPath[] = "/org/slm/Desktop/InputCaptureBackend";
constexpr const char kDefaultDbusIface[] = "org.slm.Desktop.InputCaptureBackend";
constexpr const char kNativeCompositorServiceA[] = "org.slm.Compositor.InputCaptureBackend";
constexpr const char kNativeCompositorPathA[] = "/org/slm/Compositor/InputCaptureBackend";
constexpr const char kNativeCompositorIfaceA[] = "org.slm.Compositor.InputCaptureBackend";
constexpr const char kNativeCompositorServiceB[] = "org.slm.Compositor.InputCapture";
constexpr const char kNativeCompositorPathB[] = "/org/slm/Compositor/InputCapture";
constexpr const char kNativeCompositorIfaceB[] = "org.slm.Compositor.InputCapture";

QVariantMap baseResult(const QString &backend,
                       const QString &operation,
                       const QString &sessionPath)
{
    return {
        {QStringLiteral("backend"), backend},
        {QStringLiteral("operation"), operation},
        {QStringLiteral("session"), sessionPath},
        {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()},
    };
}

class HelperProcessInputCaptureBackend final : public InputCaptureBackend
{
public:
    explicit HelperProcessInputCaptureBackend(QString helperPath)
        : m_helperPath(std::move(helperPath))
    {
    }

    QString backendName() const override
    {
        return QStringLiteral("helper-process");
    }

    QVariantMap capabilityReport() const override
    {
        const QFileInfo info(m_helperPath);
        return {
            {QStringLiteral("name"), backendName()},
            {QStringLiteral("available"), info.exists() && info.isExecutable()},
            {QStringLiteral("active"), false},
            {QStringLiteral("supportsPointerBarriers"), true},
            {QStringLiteral("mode"), QStringLiteral("external-helper")},
            {QStringLiteral("helper_path"), m_helperPath},
        };
    }

    QVariantMap createSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options) override
    {
        QVariantMap payload{
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("app_id"), appId},
            {QStringLiteral("options"), options},
        };
        return run(QStringLiteral("create"), sessionPath, payload);
    }

    QVariantMap setPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options) override
    {
        QVariantMap payload{
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("barriers"), barriers},
            {QStringLiteral("options"), options},
        };
        return run(QStringLiteral("set_barriers"), sessionPath, payload);
    }

    QVariantMap enableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        QVariantMap payload{
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("options"), options},
        };
        return run(QStringLiteral("enable"), sessionPath, payload);
    }

    QVariantMap disableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        QVariantMap payload{
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("options"), options},
        };
        return run(QStringLiteral("disable"), sessionPath, payload);
    }

    QVariantMap releaseSession(const QString &sessionPath, const QString &reason) override
    {
        QVariantMap payload{
            {QStringLiteral("session_handle"), sessionPath},
            {QStringLiteral("reason"), reason},
        };
        return run(QStringLiteral("release"), sessionPath, payload);
    }

private:
    QVariantMap run(const QString &operation,
                    const QString &sessionPath,
                    const QVariantMap &payload) const
    {
        QVariantMap result = baseResult(backendName(), operation, sessionPath);

        QFileInfo info(m_helperPath);
        if (!info.exists() || !info.isExecutable()) {
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("helper-not-executable"));
            result.insert(QStringLiteral("helper_path"), m_helperPath);
            return result;
        }

        QProcess proc;
        proc.setProgram(m_helperPath);
        proc.setArguments({operation});
        proc.setProcessChannelMode(QProcess::SeparateChannels);
        proc.start();
        if (!proc.waitForStarted(1000)) {
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("helper-start-failed"));
            result.insert(QStringLiteral("helper_path"), m_helperPath);
            return result;
        }

        QJsonObject obj = QJsonObject::fromVariantMap(payload);
        proc.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        proc.closeWriteChannel();
        const bool finished = proc.waitForFinished(3000);
        const QByteArray out = proc.readAllStandardOutput();
        const QByteArray err = proc.readAllStandardError();

        if (!finished) {
            proc.kill();
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("helper-timeout"));
            result.insert(QStringLiteral("stderr"), QString::fromUtf8(err));
            return result;
        }

        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("helper-nonzero-exit"));
            result.insert(QStringLiteral("exit_code"), proc.exitCode());
            result.insert(QStringLiteral("stderr"), QString::fromUtf8(err));
            return result;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(out);
        if (!doc.isObject()) {
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("helper-invalid-json"));
            result.insert(QStringLiteral("stdout"), QString::fromUtf8(out));
            return result;
        }

        const QVariantMap helper = doc.object().toVariantMap();
        result.insert(QStringLiteral("applied"), helper.value(QStringLiteral("ok"), true).toBool());
        result.insert(QStringLiteral("helper"), helper);
        if (!result.value(QStringLiteral("applied")).toBool()) {
            result.insert(QStringLiteral("reason"),
                          helper.value(QStringLiteral("reason"),
                                       QStringLiteral("helper-operation-failed")).toString());
        }
        return result;
    }

    QString m_helperPath;
};

class DbusDirectInputCaptureBackend final : public InputCaptureBackend
{
public:
    DbusDirectInputCaptureBackend(QString service,
                                  QString path,
                                  QString iface,
                                  QString mode = QStringLiteral("dbus-forward"))
        : m_service(std::move(service))
        , m_path(std::move(path))
        , m_iface(std::move(iface))
        , m_mode(std::move(mode))
    {
    }

    QString backendName() const override
    {
        return QStringLiteral("dbus-direct");
    }

    QVariantMap capabilityReport() const override
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        bool available = false;
        if (bus.isConnected() && bus.interface()) {
            const QDBusReply<bool> reply = bus.interface()->isServiceRegistered(m_service);
            available = reply.isValid() && reply.value();
        }
        return {
            {QStringLiteral("name"), backendName()},
            {QStringLiteral("available"), available},
            {QStringLiteral("active"), false},
            {QStringLiteral("supportsPointerBarriers"), true},
            {QStringLiteral("mode"), m_mode},
            {QStringLiteral("service"), m_service},
            {QStringLiteral("path"), m_path},
            {QStringLiteral("interface"), m_iface},
        };
    }

    QVariantMap createSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options) override
    {
        return call(QStringLiteral("CreateSession"), sessionPath, appId, options);
    }

    QVariantMap setPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options) override
    {
        return call(QStringLiteral("SetPointerBarriers"), sessionPath, barriers, options);
    }

    QVariantMap enableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        return call(QStringLiteral("EnableSession"), sessionPath, options);
    }

    QVariantMap disableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        return call(QStringLiteral("DisableSession"), sessionPath, options);
    }

    QVariantMap releaseSession(const QString &sessionPath, const QString &reason) override
    {
        return call(QStringLiteral("ReleaseSession"), sessionPath, reason);
    }

private:
    template <typename... Args>
    QVariantMap call(const QString &method, const QString &sessionPath, Args&&... args) const
    {
        QVariantMap result = baseResult(backendName(), method.toLower(), sessionPath);

        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("session-bus-unavailable"));
            return result;
        }

        QDBusInterface iface(m_service, m_path, m_iface, bus);
        if (!iface.isValid()) {
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("backend-interface-unavailable"));
            result.insert(QStringLiteral("service"), m_service);
            return result;
        }

        QDBusReply<QVariantMap> reply = iface.call(method,
                                                   sessionPath,
                                                   std::forward<Args>(args)...);
        if (!reply.isValid()) {
            result.insert(QStringLiteral("applied"), false);
            result.insert(QStringLiteral("reason"), QStringLiteral("backend-call-failed"));
            result.insert(QStringLiteral("service"), m_service);
            return result;
        }

        const QVariantMap payload = reply.value();
        result.insert(QStringLiteral("reply"), payload);
        const bool ok = payload.value(QStringLiteral("ok"), true).toBool();
        result.insert(QStringLiteral("applied"), ok);
        if (!ok) {
            result.insert(QStringLiteral("reason"),
                          payload.value(QStringLiteral("reason"),
                                        QStringLiteral("backend-denied")).toString());
        }
        return result;
    }

    QString m_service;
    QString m_path;
    QString m_iface;
    QString m_mode;
};

class NullInputCaptureBackend final : public InputCaptureBackend
{
public:
    QString backendName() const override
    {
        return QStringLiteral("null");
    }

    QVariantMap capabilityReport() const override
    {
        return {
            {QStringLiteral("name"), backendName()},
            {QStringLiteral("available"), true},
            {QStringLiteral("active"), false},
            {QStringLiteral("supportsPointerBarriers"), false},
            {QStringLiteral("mode"), QStringLiteral("noop")},
        };
    }

    QVariantMap createSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return {
            {QStringLiteral("backend"), backendName()},
            {QStringLiteral("applied"), false},
            {QStringLiteral("session"), sessionPath},
            {QStringLiteral("app_id"), appId},
            {QStringLiteral("reason"), QStringLiteral("no-input-capture-backend")},
        };
    }

    QVariantMap setPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return {
            {QStringLiteral("backend"), backendName()},
            {QStringLiteral("applied"), false},
            {QStringLiteral("session"), sessionPath},
            {QStringLiteral("barrier_count"), barriers.size()},
            {QStringLiteral("reason"), QStringLiteral("no-input-capture-backend")},
        };
    }

    QVariantMap enableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return {
            {QStringLiteral("backend"), backendName()},
            {QStringLiteral("applied"), false},
            {QStringLiteral("session"), sessionPath},
            {QStringLiteral("reason"), QStringLiteral("no-input-capture-backend")},
        };
    }

    QVariantMap disableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return {
            {QStringLiteral("backend"), backendName()},
            {QStringLiteral("applied"), false},
            {QStringLiteral("session"), sessionPath},
            {QStringLiteral("reason"), QStringLiteral("no-input-capture-backend")},
        };
    }

    QVariantMap releaseSession(const QString &sessionPath, const QString &reason) override
    {
        return {
            {QStringLiteral("backend"), backendName()},
            {QStringLiteral("applied"), false},
            {QStringLiteral("session"), sessionPath},
            {QStringLiteral("reason"), reason.trimmed().isEmpty()
                                            ? QStringLiteral("released")
                                            : reason.trimmed()},
            {QStringLiteral("details"), QStringLiteral("no-input-capture-backend")},
        };
    }
};

class KWinProbeInputCaptureBackend final : public InputCaptureBackend
{
public:
    QString backendName() const override
    {
        return QStringLiteral("kwin-probe");
    }

    QVariantMap capabilityReport() const override
    {
        return {
            {QStringLiteral("name"), backendName()},
            {QStringLiteral("available"), m_connected},
            {QStringLiteral("active"), false},
            {QStringLiteral("supportsPointerBarriers"), false},
            {QStringLiteral("mode"), QStringLiteral("probe")},
        };
    }

    QVariantMap createSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return operationResult(QStringLiteral("create"), sessionPath, appId, QVariantMap{});
    }

    QVariantMap setPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return operationResult(QStringLiteral("set-barriers"),
                               sessionPath,
                               QString(),
                               {{QStringLiteral("barrier_count"), barriers.size()}});
    }

    QVariantMap enableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return operationResult(QStringLiteral("enable"), sessionPath, QString(), QVariantMap{});
    }

    QVariantMap disableSession(const QString &sessionPath, const QVariantMap &options) override
    {
        Q_UNUSED(options)
        return operationResult(QStringLiteral("disable"), sessionPath, QString(), QVariantMap{});
    }

    QVariantMap releaseSession(const QString &sessionPath, const QString &reason) override
    {
        return operationResult(QStringLiteral("release"),
                               sessionPath,
                               QString(),
                               {{QStringLiteral("reason"), reason}});
    }

    explicit KWinProbeInputCaptureBackend(bool connected)
        : m_connected(connected)
    {
    }

private:
    QVariantMap operationResult(const QString &operation,
                                const QString &sessionPath,
                                const QString &appId,
                                const QVariantMap &extra) const
    {
        QVariantMap map{
            {QStringLiteral("backend"), backendName()},
            {QStringLiteral("operation"), operation},
            {QStringLiteral("session"), sessionPath},
            {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()},
            {QStringLiteral("applied"), false},
            {QStringLiteral("reason"), QStringLiteral("kwin-inputcapture-not-implemented")},
        };
        if (!appId.isEmpty()) {
            map.insert(QStringLiteral("app_id"), appId);
        }
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
            map.insert(it.key(), it.value());
        }
        return map;
    }

    bool m_connected = false;
};

bool hasKWinSessionService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return false;
    }
    const QDBusReply<bool> reply = iface->isServiceRegistered(QStringLiteral("org.kde.KWin"));
    return reply.isValid() && reply.value();
}

bool hasSessionService(const QString &service)
{
    if (service.trimmed().isEmpty()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return false;
    }
    const QDBusReply<bool> reply = iface->isServiceRegistered(service.trimmed());
    return reply.isValid() && reply.value();
}

QVariantMap detectNativeCompositorBackend()
{
    const QVariantList candidates{
        QVariantMap{
            {QStringLiteral("service"), QString::fromLatin1(kNativeCompositorServiceA)},
            {QStringLiteral("path"), QString::fromLatin1(kNativeCompositorPathA)},
            {QStringLiteral("iface"), QString::fromLatin1(kNativeCompositorIfaceA)},
        },
        QVariantMap{
            {QStringLiteral("service"), QString::fromLatin1(kNativeCompositorServiceB)},
            {QStringLiteral("path"), QString::fromLatin1(kNativeCompositorPathB)},
            {QStringLiteral("iface"), QString::fromLatin1(kNativeCompositorIfaceB)},
        },
    };

    for (const QVariant &entryValue : candidates) {
        const QVariantMap entry = entryValue.toMap();
        const QString service = entry.value(QStringLiteral("service")).toString().trimmed();
        if (service.isEmpty() || !hasSessionService(service)) {
            continue;
        }
        return entry;
    }
    return {};
}

} // namespace

std::unique_ptr<InputCaptureBackend> InputCaptureBackend::createDefault()
{
    const QString dbusService = QProcessEnvironment::systemEnvironment()
                                    .value(QStringLiteral("SLM_INPUTCAPTURE_DBUS_SERVICE"))
                                    .trimmed();
    if (!dbusService.isEmpty()) {
        const QString dbusPath = QProcessEnvironment::systemEnvironment()
                                     .value(QStringLiteral("SLM_INPUTCAPTURE_DBUS_PATH"),
                                            QStringLiteral("/org/slm/Desktop/InputCaptureBackend"))
                                     .trimmed();
        const QString dbusIface = QProcessEnvironment::systemEnvironment()
                                      .value(QStringLiteral("SLM_INPUTCAPTURE_DBUS_IFACE"),
                                             QStringLiteral("org.slm.Desktop.InputCaptureBackend"))
                                      .trimmed();
        return std::make_unique<DbusDirectInputCaptureBackend>(dbusService,
                                                               dbusPath,
                                                               dbusIface);
    }
    const QVariantMap native = detectNativeCompositorBackend();
    if (!native.isEmpty()) {
        return std::make_unique<DbusDirectInputCaptureBackend>(
            native.value(QStringLiteral("service")).toString().trimmed(),
            native.value(QStringLiteral("path")).toString().trimmed(),
            native.value(QStringLiteral("iface")).toString().trimmed(),
            QStringLiteral("native-compositor-dbus"));
    }
    if (hasSessionService(QString::fromLatin1(kDefaultDbusService))) {
        return std::make_unique<DbusDirectInputCaptureBackend>(QString::fromLatin1(kDefaultDbusService),
                                                               QString::fromLatin1(kDefaultDbusPath),
                                                               QString::fromLatin1(kDefaultDbusIface));
    }

    const QString helper = QProcessEnvironment::systemEnvironment()
                               .value(QStringLiteral("SLM_INPUTCAPTURE_HELPER"))
                               .trimmed();
    if (!helper.isEmpty()) {
        return std::make_unique<HelperProcessInputCaptureBackend>(helper);
    }
    if (hasKWinSessionService()) {
        return std::make_unique<KWinProbeInputCaptureBackend>(true);
    }
    return std::make_unique<NullInputCaptureBackend>();
}
