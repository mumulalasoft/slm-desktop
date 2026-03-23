#include "kwinwaylandipcclient.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QTimer>

namespace {
constexpr auto kKWinService = "org.kde.KWin";
constexpr auto kKWinPath = "/KWin";
constexpr auto kKWinIface = "org.kde.KWin";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
constexpr auto kVdmPath = "/VirtualDesktopManager";
constexpr auto kVdmIface = "org.kde.KWin.VirtualDesktopManager";
constexpr auto kWindowIfaceA = "org.kde.KWin.Window";
constexpr auto kWindowIfaceB = "org.kde.kwin.Window";
constexpr auto kInputCaptureBridgePath = "/org/slm/Compositor/InputCapture";
constexpr auto kInputCaptureBridgeIface = "org.slm.Compositor.InputCapture";
}

KWinWaylandIpcClient::KWinWaylandIpcClient(QObject *parent)
    : QObject(parent)
    , m_probeTimer(new QTimer(this))
{
    m_probeTimer->setSingleShot(false);
    m_probeTimer->setInterval(5000);
    connect(m_probeTimer, &QTimer::timeout, this, &KWinWaylandIpcClient::refreshConnected);
    m_probeTimer->start();
    if (QDBusConnection::sessionBus().isConnected()) {
        auto *kwinWatcher = new QDBusServiceWatcher(QString::fromLatin1(kKWinService),
                                                    QDBusConnection::sessionBus(),
                                                    QDBusServiceWatcher::WatchForOwnerChange,
                                                    this);
        connect(kwinWatcher,
                &QDBusServiceWatcher::serviceOwnerChanged,
                this,
                [this](const QString &, const QString &oldOwner, const QString &newOwner) {
                    onServiceOwnerChanged(QString::fromLatin1(kKWinService), oldOwner, newOwner);
                });
    }
    refreshConnected();
}

KWinWaylandIpcClient::~KWinWaylandIpcClient() = default;

QString KWinWaylandIpcClient::socketPath() const
{
    return QString();
}

bool KWinWaylandIpcClient::connected() const
{
    return m_connected;
}

QVariantMap KWinWaylandIpcClient::lastEvent() const
{
    return m_lastEvent;
}

void KWinWaylandIpcClient::reconnect()
{
    refreshConnected();
}

bool KWinWaylandIpcClient::sendCommand(const QString &command)
{
    const QString cmd = command.trimmed();
    if (cmd.isEmpty()) {
        publishEvent(QStringLiteral("command"), cmd, false, QStringLiteral("empty-command"));
        return false;
    }

    refreshConnected();
    if (!m_connected) {
        publishEvent(QStringLiteral("command"), cmd, false, QStringLiteral("kwin-unavailable"));
        return false;
    }

    QString error;
    bool ok = false;
    if (cmd == QStringLiteral("switcher-next")) {
        ok = invokeKWinShortcut(QStringLiteral("Walk Through Windows"), &error);
    } else if (cmd == QStringLiteral("switcher-prev")) {
        ok = invokeKWinShortcut(QStringLiteral("Walk Through Windows (Reverse)"), &error);
    } else if (cmd.startsWith(QStringLiteral("overview "))
               || cmd.startsWith(QStringLiteral("workspace "))) {
        ok = invokeAnyShortcut({QStringLiteral("Toggle Overview"),
                                QStringLiteral("Overview"),
                                QStringLiteral("Present Windows")},
                               &error);
    } else if (cmd.startsWith(QStringLiteral("launchpad "))) {
        // KWin has no direct Launchpad equivalent; keep command accepted to avoid noisy retries.
        ok = true;
    } else if (cmd == QStringLiteral("show-desktop")) {
        ok = invokeAnyShortcut({QStringLiteral("Show Desktop"),
                                QStringLiteral("MinimizeAll")},
                               &error);
    } else if (cmd.startsWith(QStringLiteral("space set "))) {
        bool parseOk = false;
        const int desktop = cmd.mid(QStringLiteral("space set ").size()).trimmed().toInt(&parseOk);
        if (!parseOk || desktop <= 0) {
            error = QStringLiteral("invalid-space-index");
            ok = false;
        } else {
            QDBusInterface kwin(QString::fromLatin1(kKWinService),
                                QString::fromLatin1(kKWinPath),
                                QString::fromLatin1(kKWinIface),
                                QDBusConnection::sessionBus());
            if (kwin.isValid()) {
                QDBusReply<void> reply = kwin.call(QStringLiteral("setCurrentDesktop"), desktop);
                ok = reply.isValid();
            }
            if (!ok) {
                ok = setIntProperty(QString::fromLatin1(kKWinPath),
                                    QString::fromLatin1(kKWinIface),
                                    QStringLiteral("currentDesktop"),
                                    desktop,
                                    &error);
            }
            if (!ok) {
                ok = setIntProperty(QString::fromLatin1(kVdmPath),
                                    QString::fromLatin1(kVdmIface),
                                    QStringLiteral("currentDesktop"),
                                    desktop,
                                    &error);
            }
        }
    } else if (cmd.startsWith(QStringLiteral("space count "))) {
        bool parseOk = false;
        const int count = cmd.mid(QStringLiteral("space count ").size()).trimmed().toInt(&parseOk);
        if (!parseOk || count <= 0) {
            error = QStringLiteral("invalid-space-count");
            ok = false;
        } else {
            QDBusInterface kwin(QString::fromLatin1(kKWinService),
                                QString::fromLatin1(kKWinPath),
                                QString::fromLatin1(kKWinIface),
                                QDBusConnection::sessionBus());
            if (kwin.isValid()) {
                QDBusReply<void> reply = kwin.call(QStringLiteral("setNumberOfDesktops"), count);
                ok = reply.isValid();
            }
            if (!ok) {
                ok = setIntProperty(QString::fromLatin1(kKWinPath),
                                    QString::fromLatin1(kKWinIface),
                                    QStringLiteral("numberOfDesktops"),
                                    count,
                                    &error);
            }
        }
    } else if (cmd.startsWith(QStringLiteral("focus-view "))) {
        const QString viewId = cmd.mid(QStringLiteral("focus-view ").size()).trimmed();
        if (viewId.isEmpty()) {
            error = QStringLiteral("missing-view-id");
            ok = false;
        } else if (viewId.startsWith('/')) {
            ok = activateWindowByPath(viewId, &error);
        } else {
            error = QStringLiteral("unsupported-view-id-format");
            ok = false;
        }
    } else if (cmd.startsWith(QStringLiteral("close-view "))) {
        const QString viewId = cmd.mid(QStringLiteral("close-view ").size()).trimmed();
        if (viewId.startsWith('/')) {
            QDBusInterface win(QString::fromLatin1(kKWinService),
                               viewId,
                               QString::fromLatin1(kWindowIfaceA),
                               QDBusConnection::sessionBus());
            QDBusReply<void> reply = win.call(QStringLiteral("closeWindow"));
            ok = reply.isValid();
            if (!ok) {
                QDBusInterface winB(QString::fromLatin1(kKWinService),
                                    viewId,
                                    QString::fromLatin1(kWindowIfaceB),
                                    QDBusConnection::sessionBus());
                reply = winB.call(QStringLiteral("closeWindow"));
                ok = reply.isValid();
            }
            if (!ok) {
                error = QStringLiteral("close-window-failed");
            }
        } else {
            error = QStringLiteral("unsupported-view-id-format");
            ok = false;
        }
    } else if (cmd == QStringLiteral("progress hide")
               || cmd.startsWith(QStringLiteral("shellpopup "))
               || cmd.startsWith(QStringLiteral("dock mode "))) {
        // Managed by shell UI state; not a compositor capability on KWin path.
        ok = true;
    } else if (cmd.startsWith(QStringLiteral("inputcapture "))) {
        const QString rest = cmd.mid(QStringLiteral("inputcapture ").size()).trimmed();
        const int space = rest.indexOf(' ');
        const QString op = (space > 0) ? rest.left(space).trimmed() : rest;
        const QString arg = (space > 0) ? rest.mid(space + 1).trimmed() : QString();
        ok = invokeInputCaptureBridge(op, arg, &error);
        if (!ok && error.isEmpty()) {
            error = QStringLiteral("inputcapture-bridge-failed");
        }
    } else {
        error = QStringLiteral("unsupported-command-on-kwin");
        ok = false;
    }

    publishEvent(QStringLiteral("command"), cmd, ok, error);
    return ok;
}

bool KWinWaylandIpcClient::supportsInputCaptureCommands() const
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        return false;
    }
    const QString service = inputCaptureBridgeService();
    if (service.isEmpty()) {
        return false;
    }
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    if (!iface) {
        return false;
    }
    const QDBusReply<bool> reply = iface->isServiceRegistered(service);
    return reply.isValid() && reply.value();
}

QVariantMap KWinWaylandIpcClient::setInputCapturePointerBarriers(const QString &sessionPath,
                                                                 const QVariantList &barriers,
                                                                 const QVariantMap &options)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("missing-session-path")},
        };
    }
    return callInputCaptureBridge(QStringLiteral("SetPointerBarriers"),
                                  QVariantList{session, barriers, options});
}

QVariantMap KWinWaylandIpcClient::enableInputCaptureSession(const QString &sessionPath,
                                                            const QVariantMap &options)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("missing-session-path")},
        };
    }
    return callInputCaptureBridge(QStringLiteral("EnableSession"),
                                  QVariantList{session, options});
}

QVariantMap KWinWaylandIpcClient::disableInputCaptureSession(const QString &sessionPath,
                                                             const QVariantMap &options)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("missing-session-path")},
        };
    }
    return callInputCaptureBridge(QStringLiteral("DisableSession"),
                                  QVariantList{session, options});
}

QVariantMap KWinWaylandIpcClient::releaseInputCaptureSession(const QString &sessionPath,
                                                             const QString &reason)
{
    const QString session = sessionPath.trimmed();
    if (session.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("missing-session-path")},
        };
    }
    return callInputCaptureBridge(
        QStringLiteral("ReleaseSession"),
        QVariantList{session, reason.trimmed().isEmpty()
                                   ? QStringLiteral("released-by-shell")
                                   : reason.trimmed()});
}

void KWinWaylandIpcClient::refreshConnected()
{
    bool next = false;
    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
        if (iface != nullptr) {
            QDBusReply<bool> reply = iface->isServiceRegistered(QString::fromLatin1(kKWinService));
            next = reply.isValid() && reply.value();
        }
    }
    if (next != m_connected) {
        m_connected = next;
        emit connectedChanged();
    }
}

void KWinWaylandIpcClient::onServiceOwnerChanged(const QString &name,
                                                 const QString &oldOwner,
                                                 const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)
    if (name == QString::fromLatin1(kKWinService)) {
        refreshConnected();
    }
}

bool KWinWaylandIpcClient::invokeKWinShortcut(const QString &shortcutName, QString *errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    QDBusInterface kwin(QString::fromLatin1(kKWinService),
                        QString::fromLatin1(kKWinPath),
                        QString::fromLatin1(kKWinIface),
                        QDBusConnection::sessionBus());
    if (!kwin.isValid()) {
        if (errorOut) {
            *errorOut = QStringLiteral("kwin-dbus-interface-invalid");
        }
        return false;
    }
    QDBusReply<void> reply = kwin.call(QStringLiteral("invokeShortcut"), shortcutName);
    if (!reply.isValid()) {
        if (errorOut) {
            *errorOut = reply.error().message().trimmed();
            if (errorOut->isEmpty()) {
                *errorOut = QStringLiteral("kwin-shortcut-invoke-failed");
            }
        }
        return false;
    }
    return true;
}

bool KWinWaylandIpcClient::invokeAnyShortcut(const QStringList &shortcutNames, QString *errorOut)
{
    QString lastError;
    for (const QString &name : shortcutNames) {
        if (name.trimmed().isEmpty()) {
            continue;
        }
        QString err;
        if (invokeKWinShortcut(name, &err)) {
            if (errorOut) {
                errorOut->clear();
            }
            return true;
        }
        if (!err.isEmpty()) {
            lastError = err;
        }
    }
    if (errorOut) {
        *errorOut = lastError.isEmpty()
                        ? QStringLiteral("kwin-shortcut-invoke-failed")
                        : lastError;
    }
    return false;
}

bool KWinWaylandIpcClient::setIntProperty(const QString &path,
                                          const QString &iface,
                                          const QString &property,
                                          int value,
                                          QString *errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    QDBusInterface props(QString::fromLatin1(kKWinService),
                         path,
                         QString::fromLatin1(kPropsIface),
                         QDBusConnection::sessionBus());
    if (!props.isValid()) {
        if (errorOut) {
            *errorOut = QStringLiteral("kwin-properties-interface-invalid");
        }
        return false;
    }
    QDBusReply<void> reply = props.call(QStringLiteral("Set"),
                                        iface,
                                        property,
                                        QVariant::fromValue(QDBusVariant(value)));
    if (!reply.isValid()) {
        if (errorOut) {
            *errorOut = reply.error().message().trimmed();
            if (errorOut->isEmpty()) {
                *errorOut = QStringLiteral("kwin-properties-set-failed");
            }
        }
        return false;
    }
    return true;
}

bool KWinWaylandIpcClient::activateWindowByPath(const QString &path, QString *errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }

    QDBusInterface win(QString::fromLatin1(kKWinService),
                       path,
                       QString::fromLatin1(kWindowIfaceA),
                       QDBusConnection::sessionBus());
    if (win.isValid()) {
        QDBusReply<void> r = win.call(QStringLiteral("setKeepAbove"), true);
        Q_UNUSED(r)
    }

    if (setIntProperty(path, QString::fromLatin1(kWindowIfaceA), QStringLiteral("active"), 1, errorOut)) {
        return true;
    }
    if (setIntProperty(path, QString::fromLatin1(kWindowIfaceB), QStringLiteral("active"), 1, errorOut)) {
        return true;
    }

    QDBusInterface winA(QString::fromLatin1(kKWinService),
                        path,
                        QString::fromLatin1(kWindowIfaceA),
                        QDBusConnection::sessionBus());
    if (winA.isValid()) {
        QDBusReply<void> reply = winA.call(QStringLiteral("activate"));
        if (reply.isValid()) {
            return true;
        }
    }
    QDBusInterface winB(QString::fromLatin1(kKWinService),
                        path,
                        QString::fromLatin1(kWindowIfaceB),
                        QDBusConnection::sessionBus());
    if (winB.isValid()) {
        QDBusReply<void> reply = winB.call(QStringLiteral("activate"));
        if (reply.isValid()) {
            return true;
        }
    }

    if (errorOut && errorOut->isEmpty()) {
        *errorOut = QStringLiteral("focus-window-failed");
    }
    return false;
}

bool KWinWaylandIpcClient::invokeInputCaptureBridge(const QString &operation,
                                                    const QString &sessionOrPayload,
                                                    QString *errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    const QString op = operation.trimmed().toLower();
    if (op.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("missing-inputcapture-operation");
        }
        return false;
    }

    QVariantMap result;
    if (op == QStringLiteral("enable")) {
        result = callInputCaptureBridge(QStringLiteral("EnableSession"),
                                        QVariantList{sessionOrPayload, QVariantMap{}});
    } else if (op == QStringLiteral("disable")) {
        result = callInputCaptureBridge(QStringLiteral("DisableSession"),
                                        QVariantList{sessionOrPayload, QVariantMap{}});
    } else if (op == QStringLiteral("release")) {
        result = callInputCaptureBridge(
            QStringLiteral("ReleaseSession"),
            QVariantList{sessionOrPayload, QStringLiteral("released-by-shell")});
    } else if (op == QStringLiteral("set-barriers")) {
        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(sessionOrPayload.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
            if (errorOut) {
                *errorOut = QStringLiteral("invalid-inputcapture-set-barriers-payload");
            }
            return false;
        }
        const QVariantMap payload = json.object().toVariantMap();
        const QString session = payload.value(QStringLiteral("session")).toString().trimmed();
        const QVariantList barriers = payload.value(QStringLiteral("barriers")).toList();
        result = callInputCaptureBridge(QStringLiteral("SetPointerBarriers"),
                                        QVariantList{session, barriers, QVariantMap{}});
    } else {
        if (errorOut) {
            *errorOut = QStringLiteral("unsupported-inputcapture-operation");
        }
        return false;
    }

    const bool ok = result.value(QStringLiteral("ok"), false).toBool();
    if (!ok && errorOut) {
        *errorOut = result.value(QStringLiteral("reason")).toString().trimmed();
        if (errorOut->isEmpty()) {
            *errorOut = QStringLiteral("inputcapture-bridge-denied");
        }
    }
    return ok;
}

QVariantMap KWinWaylandIpcClient::callInputCaptureBridge(const QString &methodName,
                                                         const QVariantList &args)
{
    const QString method = methodName.trimmed();
    if (method.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("missing-method")},
        };
    }
    const QString service = inputCaptureBridgeService();
    const QString path = inputCaptureBridgePath();
    const QString ifaceName = inputCaptureBridgeInterface();
    if (service.isEmpty() || path.isEmpty() || ifaceName.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("inputcapture-bridge-not-configured")},
        };
    }
    if (service == QStringLiteral("org.slm.Compositor.InputCaptureBackend")
        && path == QString::fromLatin1(kInputCaptureBridgePath)
        && ifaceName == QString::fromLatin1(kInputCaptureBridgeIface)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("inputcapture-bridge-self-recursion")},
        };
    }

    QDBusInterface bridge(service, path, ifaceName, QDBusConnection::sessionBus());
    if (!bridge.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), QStringLiteral("inputcapture-bridge-interface-invalid")},
        };
    }
    const QDBusMessage reply = bridge.callWithArgumentList(QDBus::Block, method, args);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("reason"), reply.errorMessage().trimmed().isEmpty()
                                           ? QStringLiteral("inputcapture-bridge-call-failed")
                                           : reply.errorMessage().trimmed()},
        };
    }
    if (reply.arguments().isEmpty()) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("results"), QVariantMap{}},
        };
    }
    const QVariantMap value = reply.arguments().constFirst().toMap();
    if (value.isEmpty()) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("results"), QVariantMap{}},
        };
    }
    return value;
}

QString KWinWaylandIpcClient::inputCaptureBridgeService() const
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    return env.value(QStringLiteral("SLM_INPUTCAPTURE_COMPOSITOR_SERVICE")).trimmed();
}

QString KWinWaylandIpcClient::inputCaptureBridgePath() const
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    return env.value(QStringLiteral("SLM_INPUTCAPTURE_COMPOSITOR_PATH"),
                     QString::fromLatin1(kInputCaptureBridgePath))
        .trimmed();
}

QString KWinWaylandIpcClient::inputCaptureBridgeInterface() const
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    return env.value(QStringLiteral("SLM_INPUTCAPTURE_COMPOSITOR_IFACE"),
                     QString::fromLatin1(kInputCaptureBridgeIface))
        .trimmed();
}

void KWinWaylandIpcClient::publishEvent(const QString &eventName,
                                        const QString &command,
                                        bool ok,
                                        const QString &error)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("channel"), QStringLiteral("windowing"));
    payload.insert(QStringLiteral("backend"), QStringLiteral("kwin-wayland"));
    payload.insert(QStringLiteral("event"), eventName);
    payload.insert(QStringLiteral("command"), command);
    payload.insert(QStringLiteral("ok"), ok);
    payload.insert(QStringLiteral("error"), error);
    payload.insert(QStringLiteral("timestampMs"), QDateTime::currentMSecsSinceEpoch());
    m_lastEvent = payload;
    emit lastEventChanged();
    emit eventReceived(eventName, payload);
}
