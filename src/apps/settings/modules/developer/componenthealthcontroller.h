#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::System {
class MissingComponentController;
}

class ComponentHealthController : public QObject
{
    Q_OBJECT
public:
    explicit ComponentHealthController(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList missingComponentsForDomain(const QString &domain) const;
    Q_INVOKABLE QVariantMap installComponent(const QString &componentId) const;

private:
    Slm::System::MissingComponentController *m_missingController = nullptr;
};
