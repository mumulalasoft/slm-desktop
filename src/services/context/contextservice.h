#pragma once

#include "ruleengine.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace Slm::Context {

class ContextService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop.Context")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit ContextService(QObject *parent = nullptr);
    ~ContextService() override;

    bool start(QString *error = nullptr);
    bool serviceRegistered() const;

public slots:
    QVariantMap GetContext() const;
    QVariantMap Subscribe() const;
    QVariantMap Ping() const;
#ifdef SLM_CONTEXT_TEST_HOOKS
    void InjectContextForTest(const QVariantMap &context);
#endif

signals:
    void serviceRegisteredChanged();
    void ContextChanged(const QVariantMap &context);
    // New canonical signal name (kept alongside PowerStateChanged for compatibility).
    void PowerChanged(const QVariantMap &power);
    // Legacy signal name retained for existing consumers.
    void PowerStateChanged(const QVariantMap &power);
    void TimePeriodChanged(const QString &period);
    void ActivityChanged(const QVariantMap &activity);
    void PerformanceChanged(const QVariantMap &performance);

private slots:
    void refreshContext();

private:
    void publishContextDelta(const QVariantMap &prev, const QVariantMap &next);
    void registerDbusService();
    QVariantMap buildContext() const;
    QVariantMap buildTimeContext(const QVariantMap &settingsSnapshot) const;
    QVariantMap buildPowerContext() const;
    QVariantMap buildDisplayContext() const;
    QVariantMap buildActivityContext() const;
    QVariantMap buildSystemContext() const;
    QVariantMap buildNetworkContext() const;
    QVariantMap buildAttentionContext() const;
    QVariantMap fetchSettingsSnapshot() const;

    bool m_serviceRegistered = false;
    QVariantMap m_context;
    QTimer m_refreshTimer;
    RuleEngine m_ruleEngine;
};

} // namespace Slm::Context
