#pragma once

#include <QObject>
#include <QVariantMap>

class QDBusInterface;
class QDBusServiceWatcher;

namespace Slm::Context {

class ContextClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(bool reduceAnimation READ reduceAnimation NOTIFY reduceAnimationChanged)
    Q_PROPERTY(bool disableBlur READ disableBlur NOTIFY disableBlurChanged)
    Q_PROPERTY(bool disableHeavyEffects READ disableHeavyEffects NOTIFY disableHeavyEffectsChanged)
    Q_PROPERTY(QVariantMap context READ context NOTIFY contextChanged)

public:
    explicit ContextClient(QObject *parent = nullptr);
    ~ContextClient() override;

    bool serviceAvailable() const;
    bool reduceAnimation() const;
    bool disableBlur() const;
    bool disableHeavyEffects() const;
    QVariantMap context() const;

    Q_INVOKABLE void refresh();

signals:
    void serviceAvailableChanged();
    void reduceAnimationChanged();
    void disableBlurChanged();
    void disableHeavyEffectsChanged();
    void contextChanged();

private slots:
    void onContextChanged(const QVariantMap &context);
    void onPowerChanged(const QVariantMap &power);
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void bindSignals();
    void applyContext(const QVariantMap &context);
    void setServiceAvailable(bool available);
    void setReduceAnimation(bool reduce);
    void setDisableBlur(bool disable);
    void setDisableHeavyEffects(bool disable);

    QDBusInterface *m_iface = nullptr;
    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_serviceAvailable = false;
    bool m_reduceAnimation = false;
    bool m_disableBlur = false;
    bool m_disableHeavyEffects = false;
    QVariantMap m_context;
};

} // namespace Slm::Context
