#pragma once

#include <QDBusMessage>
#include <QString>

namespace Slm::Permissions {

enum class TrustLevel {
    CoreDesktopComponent = 0,
    PrivilegedDesktopService,
    ThirdPartyApplication
};

struct CallerIdentity {
    QString busName;
    QString appId;
    QString desktopFileId;
    QString executablePath;
    qint64 pid = -1;
    qint64 uid = -1;
    bool sandboxed = false;
    TrustLevel trustLevel = TrustLevel::ThirdPartyApplication;
    bool isOfficialComponent = false;

    bool isValid() const { return !busName.trimmed().isEmpty(); }
};

QString trustLevelToString(TrustLevel level);
TrustLevel trustLevelFromString(const QString &value);
CallerIdentity resolveCallerIdentityFromDbus(const QDBusMessage &message);

} // namespace Slm::Permissions

