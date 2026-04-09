#pragma once

#include <QVariantMap>

namespace Slm::Firewall {

class AppIdentityClient
{
public:
    QVariantMap resolveByPid(qint64 pid) const;
};

} // namespace Slm::Firewall
