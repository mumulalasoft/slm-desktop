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
    Q_INVOKABLE bool hasBlockingMissingForDomain(const QString &domain) const;
    Q_INVOKABLE QVariantMap installComponent(const QString &componentId) const;
    Q_INVOKABLE QVariantMap evaluatePackagePolicy(const QString &tool,
                                                  const QString &arguments) const;

private:
    Slm::System::MissingComponentController *m_missingController = nullptr;
};
