#include "componenthealthcontroller.h"

#include "src/core/system/missingcomponentcontroller.h"

ComponentHealthController::ComponentHealthController(QObject *parent)
    : QObject(parent)
    , m_missingController(new Slm::System::MissingComponentController(this))
{
}

QVariantList ComponentHealthController::missingComponentsForDomain(const QString &domain) const
{
    return m_missingController->missingComponentsForDomain(domain);
}

QVariantMap ComponentHealthController::installComponent(const QString &componentId) const
{
    return m_missingController->installComponent(componentId);
}
