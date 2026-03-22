#pragma once

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QVariant>

namespace FileManagerApiDaemonCommon {

constexpr const char kFileOpsService[] = "org.slm.Desktop.FileOperations";
constexpr const char kFileOpsPath[] = "/org/slm/Desktop/FileOperations";
constexpr const char kFileOpsIface[] = "org.slm.Desktop.FileOperations";
constexpr int kDbusTimeoutMs = 5000;
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
inline DbusFailure classifyDbusReplyError(const ReplyT &reply)
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

inline QString dbusFailureCode(DbusFailure failure)
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

inline bool callFileOpsServiceJob(const QString &method,
                                  const QVariantList &args,
                                  QString *jobIdOut = nullptr,
                                  QString *failureCodeOut = nullptr)
{
    QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                         QString::fromLatin1(kFileOpsPath),
                         QString::fromLatin1(kFileOpsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
    }
    iface.setTimeout(kDbusTimeoutMs);
    QDBusReply<QString> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = dbusFailureCode(classifyDbusReplyError(reply));
        }
        return false;
    }
    if (jobIdOut) {
        *jobIdOut = reply.value().trimmed();
    }
    return true;
}

inline bool callFileOpsServiceBool(const QString &method,
                                   const QVariantList &args,
                                   bool *okOut = nullptr,
                                   QString *failureCodeOut = nullptr)
{
    QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                         QString::fromLatin1(kFileOpsPath),
                         QString::fromLatin1(kFileOpsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
    }
    iface.setTimeout(kDbusTimeoutMs);
    QDBusReply<bool> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = dbusFailureCode(classifyDbusReplyError(reply));
        }
        return false;
    }
    if (okOut) {
        *okOut = reply.value();
    }
    return true;
}

inline bool callFileOpsServiceMap(const QString &method,
                                  const QVariantList &args,
                                  QVariantMap *resultOut = nullptr,
                                  QString *failureCodeOut = nullptr)
{
    QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                         QString::fromLatin1(kFileOpsPath),
                         QString::fromLatin1(kFileOpsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
    }
    iface.setTimeout(kDbusTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = dbusFailureCode(classifyDbusReplyError(reply));
        }
        return false;
    }
    if (resultOut) {
        *resultOut = reply.value();
    }
    return true;
}

inline bool callFileOpsServiceList(const QString &method,
                                   const QVariantList &args,
                                   QVariantList *resultOut = nullptr,
                                   QString *failureCodeOut = nullptr)
{
    QDBusInterface iface(QString::fromLatin1(kFileOpsService),
                         QString::fromLatin1(kFileOpsPath),
                         QString::fromLatin1(kFileOpsIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
    }
    iface.setTimeout(kDbusTimeoutMs);
    QDBusReply<QVariantList> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = dbusFailureCode(classifyDbusReplyError(reply));
        }
        return false;
    }
    if (resultOut) {
        *resultOut = reply.value();
    }
    return true;
}

inline QString normalizedPathForPolicy(const QString &path)
{
    QString out = QFileInfo(path).absoluteFilePath();
    if (out.isEmpty()) {
        return QString();
    }
    out = QDir::cleanPath(out);
    QFileInfo fi(out);
    if (fi.exists()) {
        const QString canonical = fi.canonicalFilePath();
        if (!canonical.isEmpty()) {
            out = QDir::cleanPath(canonical);
        }
    }
    return out;
}

inline QString protectedDesktopPath()
{
    return QDir::cleanPath(QDir::homePath() + QStringLiteral("/Desktop"));
}

inline bool isProtectedDesktopFolderPath(const QString &path)
{
    const QString normalized = normalizedPathForPolicy(path);
    if (normalized.isEmpty()) {
        return false;
    }
    return normalized == protectedDesktopPath();
}

} // namespace FileManagerApiDaemonCommon
