#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::System {

class MissingComponentController : public QObject
{
    Q_OBJECT
public:
    explicit MissingComponentController(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList missingComponentsForDomain(const QString &domain) const;
    Q_INVOKABLE QVariantList blockingMissingComponentsForDomain(const QString &domain) const;
    Q_INVOKABLE bool hasBlockingMissingForDomain(const QString &domain) const;
    Q_INVOKABLE QVariantMap installComponent(const QString &componentId) const;
    Q_INVOKABLE QVariantMap installComponentForDomain(const QString &domain,
                                                      const QString &componentId) const;
};

} // namespace Slm::System
