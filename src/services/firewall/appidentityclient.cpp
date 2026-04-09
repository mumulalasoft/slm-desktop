#include "appidentityclient.h"

#include <QString>

namespace Slm::Firewall {

QVariantMap AppIdentityClient::resolveByPid(qint64 pid) const
{
    return {
        {QStringLiteral("app_name"), QStringLiteral("Unknown App")},
        {QStringLiteral("app_id"), QStringLiteral("unknown")},
        {QStringLiteral("pid"), pid},
        {QStringLiteral("executable"), QStringLiteral("unknown")},
        {QStringLiteral("source"), QStringLiteral("manual")},
        {QStringLiteral("trust_level"), QStringLiteral("unknown")},
        {QStringLiteral("context"), QStringLiteral("cli")},
    };
}

} // namespace Slm::Firewall
