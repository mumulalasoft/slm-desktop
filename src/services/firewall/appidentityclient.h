#pragma once

#include <QVariantMap>

namespace Slm::Firewall {

class AppIdentityClient
{
public:
    QVariantMap resolveByPid(qint64 pid) const;

private:
    QVariantMap resolveViaAppd(qint64 pid) const;
    QVariantMap resolveViaProc(qint64 pid) const;
};

} // namespace Slm::Firewall
