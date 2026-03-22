#include "AccessContext.h"

#include <QDateTime>

namespace Slm::Permissions {

QString sensitivityToString(SensitivityLevel level)
{
    switch (level) {
    case SensitivityLevel::Low:
        return QStringLiteral("Low");
    case SensitivityLevel::Medium:
        return QStringLiteral("Medium");
    case SensitivityLevel::High:
        return QStringLiteral("High");
    case SensitivityLevel::Critical:
    default:
        return QStringLiteral("Critical");
    }
}

SensitivityLevel sensitivityFromString(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized.compare(QStringLiteral("Low"), Qt::CaseInsensitive) == 0) {
        return SensitivityLevel::Low;
    }
    if (normalized.compare(QStringLiteral("Medium"), Qt::CaseInsensitive) == 0) {
        return SensitivityLevel::Medium;
    }
    if (normalized.compare(QStringLiteral("High"), Qt::CaseInsensitive) == 0) {
        return SensitivityLevel::High;
    }
    return SensitivityLevel::Critical;
}

QVariantMap accessContextToVariantMap(const AccessContext &context)
{
    return {
        {QStringLiteral("capability"), capabilityToString(context.capability)},
        {QStringLiteral("resourceType"), context.resourceType},
        {QStringLiteral("resourceId"), context.resourceId},
        {QStringLiteral("initiatedByUserGesture"), context.initiatedByUserGesture},
        {QStringLiteral("initiatedFromOfficialUI"), context.initiatedFromOfficialUI},
        {QStringLiteral("sensitivityLevel"), sensitivityToString(context.sensitivityLevel)},
        {QStringLiteral("timestamp"), context.timestamp},
        {QStringLiteral("sessionOnly"), context.sessionOnly},
    };
}

AccessContext accessContextFromVariantMap(const QVariantMap &map)
{
    AccessContext context;
    context.capability = capabilityFromString(map.value(QStringLiteral("capability")).toString());
    context.resourceType = map.value(QStringLiteral("resourceType")).toString().trimmed();
    context.resourceId = map.value(QStringLiteral("resourceId")).toString().trimmed();
    context.initiatedByUserGesture = map.value(QStringLiteral("initiatedByUserGesture")).toBool();
    context.initiatedFromOfficialUI = map.value(QStringLiteral("initiatedFromOfficialUI")).toBool();
    context.sensitivityLevel = sensitivityFromString(map.value(QStringLiteral("sensitivityLevel")).toString());
    context.timestamp = map.value(QStringLiteral("timestamp")).toLongLong();
    if (context.timestamp <= 0) {
        context.timestamp = QDateTime::currentMSecsSinceEpoch();
    }
    context.sessionOnly = map.value(QStringLiteral("sessionOnly")).toBool();
    return context;
}

} // namespace Slm::Permissions

