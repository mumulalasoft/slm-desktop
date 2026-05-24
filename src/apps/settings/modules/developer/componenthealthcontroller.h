#pragma once

#include <QFutureWatcher>
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
    Q_INVOKABLE void evaluatePackagePolicy(const QString &tool, const QString &arguments);
    Q_PROPERTY(bool evaluating READ evaluating NOTIFY evaluatingChanged)
    bool evaluating() const { return m_evaluating; }

signals:
    void policyEvaluated(const QVariantMap &result);
    void evaluatingChanged();

private:
    Slm::System::MissingComponentController *m_missingController = nullptr;
    QFutureWatcher<QVariantMap> *m_policyWatcher = nullptr;
    bool m_evaluating = false;
};
