#pragma once

#include "CallerIdentity.h"

#include <QSet>
#include <QStringList>

namespace Slm::Permissions {

class TrustResolver {
public:
    TrustResolver();

    void setOfficialAppIds(const QStringList &ids);
    void setOfficialDesktopFileIds(const QStringList &ids);
    void setPrivilegedServiceNames(const QStringList &serviceNames);

    CallerIdentity resolveTrust(const CallerIdentity &baseIdentity) const;

private:
    QSet<QString> m_officialAppIds;
    QSet<QString> m_officialDesktopFileIds;
    QSet<QString> m_privilegedServiceNames;
};

} // namespace Slm::Permissions

