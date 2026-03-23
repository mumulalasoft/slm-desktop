#pragma once

#include "../../../core/permissions/CallerIdentity.h"

#include <QDBusMessage>
#include <QString>

namespace Slm::PortalAdapter {

struct ResolvedPortalAppContext
{
    QString appId;
    QString desktopFileId;
    QString executablePath;
    QString busName;
    QString parentWindowRaw;
    QString parentWindowType;
    QString parentWindowId;
    bool parentWindowValid = true;
    bool appIdFromCaller = false;
};

class PortalAppContextResolver
{
public:
    static ResolvedPortalAppContext resolve(const QDBusMessage &message,
                                            const QString &requestedAppId,
                                            const QString &parentWindow);

    static ResolvedPortalAppContext resolve(const Slm::Permissions::CallerIdentity &caller,
                                            const QString &requestedAppId,
                                            const QString &parentWindow);
};

} // namespace Slm::PortalAdapter

