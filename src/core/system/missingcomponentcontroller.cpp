#include "missingcomponentcontroller.h"

#include "componentregistry.h"
#include "dependencyguard.h"

namespace Slm::System {

MissingComponentController::MissingComponentController(QObject *parent)
    : QObject(parent)
{
}

QVariantList MissingComponentController::missingComponentsForDomain(const QString &domain) const
{
    return ComponentRegistry::missingForDomain(domain);
}

QVariantMap MissingComponentController::installComponent(const QString &componentId) const
{
    ComponentRequirement req;
    const QString id = componentId.trimmed().toLower();
    if (!ComponentRegistry::findById(id, &req)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("unsupported-component")},
            {QStringLiteral("componentId"), id},
        };
    }
    if (!req.autoInstallable) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("component-not-auto-installable")},
            {QStringLiteral("componentId"), req.id},
        };
    }
    return installComponentWithPolkit(req);
}

QVariantMap MissingComponentController::installComponentForDomain(const QString &domain,
                                                                  const QString &componentId) const
{
    const QString targetId = componentId.trimmed().toLower();
    const QList<ComponentRequirement> allowed = ComponentRegistry::forDomain(domain);
    for (const ComponentRequirement &req : allowed) {
        if (req.id == targetId) {
            return installComponent(targetId);
        }
    }
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("unsupported-component")},
        {QStringLiteral("componentId"), targetId},
        {QStringLiteral("domain"), domain.trimmed().toLower()},
    };
}

} // namespace Slm::System
