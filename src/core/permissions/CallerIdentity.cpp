#include "CallerIdentity.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QFileInfo>

namespace Slm::Permissions {

QString trustLevelToString(TrustLevel level)
{
    switch (level) {
    case TrustLevel::CoreDesktopComponent:
        return QStringLiteral("CoreDesktopComponent");
    case TrustLevel::PrivilegedDesktopService:
        return QStringLiteral("PrivilegedDesktopService");
    case TrustLevel::ThirdPartyApplication:
    default:
        return QStringLiteral("ThirdPartyApplication");
    }
}

TrustLevel trustLevelFromString(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized.compare(QStringLiteral("CoreDesktopComponent"), Qt::CaseInsensitive) == 0) {
        return TrustLevel::CoreDesktopComponent;
    }
    if (normalized.compare(QStringLiteral("PrivilegedDesktopService"), Qt::CaseInsensitive) == 0) {
        return TrustLevel::PrivilegedDesktopService;
    }
    return TrustLevel::ThirdPartyApplication;
}

CallerIdentity resolveCallerIdentityFromDbus(const QDBusMessage &message)
{
    CallerIdentity identity;
    identity.busName = message.service().trimmed();
    if (identity.busName.isEmpty()) {
        return identity;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return identity;
    }

    const QDBusReply<uint> pidReply = iface->servicePid(identity.busName);
    if (pidReply.isValid()) {
        identity.pid = static_cast<qint64>(pidReply.value());
    }

    const QDBusReply<uint> uidReply = iface->serviceUid(identity.busName);
    if (uidReply.isValid()) {
        identity.uid = static_cast<qint64>(uidReply.value());
    }

    if (identity.pid > 0) {
        const QString procLink = QStringLiteral("/proc/%1/exe").arg(identity.pid);
        const QFileInfo fi(procLink);
        identity.executablePath = fi.symLinkTarget().trimmed();
        if (!identity.executablePath.isEmpty()) {
            identity.appId = QFileInfo(identity.executablePath).baseName();
            if (!identity.appId.isEmpty()) {
                identity.desktopFileId = identity.appId + QStringLiteral(".desktop");
            }
        }
    }

    const QString lowerBus = identity.busName.toLower();
    const QString lowerExe = identity.executablePath.toLower();
    identity.sandboxed = lowerBus.startsWith(QStringLiteral("org.freedesktop.portal"))
                         || lowerExe.contains(QStringLiteral("/flatpak/"))
                         || lowerExe.contains(QStringLiteral("/snap/"));
    return identity;
}

} // namespace Slm::Permissions
