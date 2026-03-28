#include "componenthealthcontroller.h"

#include "src/core/system/componentregistry.h"
#include "src/core/system/dependencyguard.h"

ComponentHealthController::ComponentHealthController(QObject *parent)
    : QObject(parent)
{
}

QVariantList ComponentHealthController::missingComponentsForDomain(const QString &domain) const
{
    return Slm::System::ComponentRegistry::missingForDomain(domain);
}

QVariantMap ComponentHealthController::installComponent(const QString &componentId) const
{
    Slm::System::ComponentRequirement req;
    if (!Slm::System::ComponentRegistry::findById(componentId, &req)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("unsupported-component")},
            {QStringLiteral("componentId"), componentId.trimmed().toLower()},
        };
    }
    if (!req.autoInstallable) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("component-not-auto-installable")},
            {QStringLiteral("componentId"), req.id},
        };
    }
    return Slm::System::installComponentWithPolkit(req);
}
