#pragma once

#include "Capability.h"

#include <QString>
#include <QVariantMap>

namespace Slm::Permissions {

enum class SensitivityLevel {
    Low = 0,
    Medium,
    High,
    Critical
};

struct AccessContext {
    Capability capability = Capability::Unknown;
    QString resourceType;
    QString resourceId;
    bool initiatedByUserGesture = false;
    bool initiatedFromOfficialUI = false;
    SensitivityLevel sensitivityLevel = SensitivityLevel::Low;
    qint64 timestamp = 0;
    bool sessionOnly = false;
};

QString sensitivityToString(SensitivityLevel level);
SensitivityLevel sensitivityFromString(const QString &value);
QVariantMap accessContextToVariantMap(const AccessContext &context);
AccessContext accessContextFromVariantMap(const QVariantMap &map);

} // namespace Slm::Permissions

