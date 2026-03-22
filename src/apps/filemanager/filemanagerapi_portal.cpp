#include "filemanagerapi.h"

#include <QClipboard>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QGuiApplication>
#include <QMetaObject>
#include <thread>

namespace {

constexpr const char kPortalService[] = "org.slm.Desktop.Portal";
constexpr const char kPortalPath[] = "/org/slm/Desktop/Portal";
constexpr const char kPortalIface[] = "org.slm.Desktop.Portal";
constexpr int kPortalDbusTimeoutMs = 190000;
constexpr const char kErrDaemonUnavailable[] = "daemon-unavailable";
constexpr const char kErrDaemonTimeout[] = "daemon-timeout";
constexpr const char kErrDaemonDbusError[] = "daemon-dbus-error";

enum class DbusFailure {
    None,
    Unavailable,
    Timeout,
    Error,
};

template<typename ReplyT>
static DbusFailure classifyDbusReplyErrorPortal(const ReplyT &reply)
{
    if (reply.isValid()) {
        return DbusFailure::None;
    }
    const QDBusError err = reply.error();
    const QString name = err.name();
    if (err.type() == QDBusError::NoReply || name.endsWith(QStringLiteral(".Timeout"))) {
        return DbusFailure::Timeout;
    }
    if (err.type() == QDBusError::ServiceUnknown
            || err.type() == QDBusError::UnknownObject
            || err.type() == QDBusError::UnknownInterface
            || name.endsWith(QStringLiteral(".ServiceUnknown"))
            || name.endsWith(QStringLiteral(".UnknownObject"))
            || name.endsWith(QStringLiteral(".UnknownInterface"))) {
        return DbusFailure::Unavailable;
    }
    return DbusFailure::Error;
}

static QString dbusFailureCodePortal(DbusFailure failure)
{
    switch (failure) {
    case DbusFailure::None:
        return QString();
    case DbusFailure::Unavailable:
        return QString::fromLatin1(kErrDaemonUnavailable);
    case DbusFailure::Timeout:
        return QString::fromLatin1(kErrDaemonTimeout);
    case DbusFailure::Error:
    default:
        return QString::fromLatin1(kErrDaemonDbusError);
    }
}

static bool callPortalServiceMapPortal(const QString &method,
                                       const QVariantList &args,
                                       QVariantMap *mapOut,
                                       QString *failureCodeOut = nullptr)
{
    QDBusInterface iface(QString::fromLatin1(kPortalService),
                         QString::fromLatin1(kPortalPath),
                         QString::fromLatin1(kPortalIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
    }
    iface.setTimeout(kPortalDbusTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = dbusFailureCodePortal(classifyDbusReplyErrorPortal(reply));
        }
        return false;
    }
    if (mapOut) {
        *mapOut = reply.value();
    }
    return true;
}

} // namespace

QString FileManagerApi::startPortalFileChooser(const QVariantMap &options,
                                               const QString &requestId)
{
    QString rid = requestId.trimmed();
    if (rid.isEmpty()) {
        rid = QStringLiteral("portal-chooser-%1").arg(QDateTime::currentMSecsSinceEpoch());
    }
    const QVariantMap opts = options;
    std::thread([this, rid, opts]() {
        QVariantMap result;
        QString daemonError;
        if (!callPortalServiceMapPortal(QStringLiteral("FileChooser"), {opts}, &result, &daemonError)) {
            result = makeResult(false, daemonError.isEmpty() ? QStringLiteral("portal-unavailable")
                                                             : daemonError);
        }
        if (!result.contains(QStringLiteral("requestId"))) {
            result.insert(QStringLiteral("requestId"), rid);
        }
        QMetaObject::invokeMethod(this, [this, rid, result]() {
            emit portalFileChooserFinished(rid, result);
        }, Qt::QueuedConnection);
    }).detach();
    return rid;
}

QVariantMap FileManagerApi::copyTextToClipboard(const QString &text) const
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (!cb) {
        return makeResult(false, QStringLiteral("clipboard-unavailable"));
    }
    cb->setText(text, QClipboard::Clipboard);
    cb->setText(text, QClipboard::Selection);
    return makeResult(true);
}
