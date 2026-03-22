#include "TrustResolver.h"

namespace Slm::Permissions {
namespace {

QSet<QString> normalizeSet(const QStringList &in)
{
    QSet<QString> out;
    for (const QString &raw : in) {
        const QString token = raw.trimmed().toLower();
        if (!token.isEmpty()) {
            out.insert(token);
        }
    }
    return out;
}

} // namespace

TrustResolver::TrustResolver()
{
    setOfficialAppIds({
        QStringLiteral("appdesktop_shell"),
        QStringLiteral("desktopd"),
        QStringLiteral("slm-fileopsd"),
        QStringLiteral("slm-devicesd"),
        QStringLiteral("slm-portald"),
    });
    setOfficialDesktopFileIds({
        QStringLiteral("appdesktop_shell.desktop"),
    });
    setPrivilegedServiceNames({
        QStringLiteral("org.slm.desktopd"),
        QStringLiteral("org.slm.desktop.fileoperations"),
        QStringLiteral("org.slm.desktop.devices"),
        QStringLiteral("org.slm.desktop.portal"),
        QStringLiteral("org.freedesktop.impl.portal.desktop.slm"),
    });
}

void TrustResolver::setOfficialAppIds(const QStringList &ids)
{
    m_officialAppIds = normalizeSet(ids);
}

void TrustResolver::setOfficialDesktopFileIds(const QStringList &ids)
{
    m_officialDesktopFileIds = normalizeSet(ids);
}

void TrustResolver::setPrivilegedServiceNames(const QStringList &serviceNames)
{
    m_privilegedServiceNames = normalizeSet(serviceNames);
}

CallerIdentity TrustResolver::resolveTrust(const CallerIdentity &baseIdentity) const
{
    CallerIdentity out = baseIdentity;
    const QString appId = out.appId.trimmed().toLower();
    const QString desktopId = out.desktopFileId.trimmed().toLower();
    const QString bus = out.busName.trimmed().toLower();
    const QString exe = out.executablePath.trimmed().toLower();

    const bool officialComponent = m_officialAppIds.contains(appId)
                                   || m_officialDesktopFileIds.contains(desktopId)
                                   || exe.contains(QStringLiteral("desktop_shell"), Qt::CaseInsensitive);
    const bool privilegedService = m_privilegedServiceNames.contains(bus)
                                   || bus.startsWith(QStringLiteral("org.slm.desktop."));

    out.isOfficialComponent = officialComponent;
    if (officialComponent) {
        out.trustLevel = TrustLevel::CoreDesktopComponent;
    } else if (privilegedService) {
        out.trustLevel = TrustLevel::PrivilegedDesktopService;
    } else {
        out.trustLevel = TrustLevel::ThirdPartyApplication;
    }
    return out;
}

} // namespace Slm::Permissions
