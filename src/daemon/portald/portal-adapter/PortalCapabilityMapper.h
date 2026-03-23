#pragma once

#include "../../../core/permissions/AccessContext.h"
#include "../../../core/permissions/Capability.h"

#include <QObject>
#include <QString>

namespace Slm::PortalAdapter {

enum class PortalRequestKind {
    OneShot = 0,
    Session
};

struct PortalMethodSpec {
    bool valid = false;
    QString method;
    Slm::Permissions::Capability capability = Slm::Permissions::Capability::Unknown;
    PortalRequestKind requestKind = PortalRequestKind::OneShot;
    Slm::Permissions::SensitivityLevel sensitivity = Slm::Permissions::SensitivityLevel::Low;
    bool requiresUserGesture = false;
    bool persistenceAllowed = false;
    bool revocableSession = false;
    bool directResponse = false;
};

class PortalCapabilityMapper : public QObject
{
    Q_OBJECT

public:
    explicit PortalCapabilityMapper(QObject *parent = nullptr);

    PortalMethodSpec mapMethod(const QString &portalMethod) const;
};

} // namespace Slm::PortalAdapter
