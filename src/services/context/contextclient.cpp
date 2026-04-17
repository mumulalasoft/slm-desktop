#include "contextclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>

namespace Slm::Context {
namespace {
constexpr auto kService = "org.desktop.Context";
constexpr auto kPath = "/org/desktop/Context";
constexpr auto kIface = "org.desktop.Context";
}

ContextClient::ContextClient(QObject *parent)
    : QObject(parent)
    , m_iface(new QDBusInterface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIface),
                                 QDBusConnection::sessionBus(),
                                 this))
{
    bindSignals();
    refresh();
}

ContextClient::~ContextClient() = default;

bool ContextClient::serviceAvailable() const
{
    return m_serviceAvailable;
}

bool ContextClient::reduceAnimation() const
{
    return m_reduceAnimation;
}

bool ContextClient::disableBlur() const
{
    return m_disableBlur;
}

bool ContextClient::disableHeavyEffects() const
{
    return m_disableHeavyEffects;
}

QVariantMap ContextClient::context() const
{
    return m_context;
}

void ContextClient::refresh()
{
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    const bool available = iface
            ? iface->isServiceRegistered(QString::fromLatin1(kService)).value()
            : false;
    setServiceAvailable(available);

    if (!m_iface || !m_iface->isValid()) {
        setReduceAnimation(false);
        setDisableBlur(false);
        setDisableHeavyEffects(false);
        if (!m_context.isEmpty()) {
            m_context.clear();
            emit contextChanged();
        }
        return;
    }

    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetContext"));
    if (!reply.isValid()) {
        setReduceAnimation(false);
        setDisableBlur(false);
        setDisableHeavyEffects(false);
        if (!m_context.isEmpty()) {
            m_context.clear();
            emit contextChanged();
        }
        return;
    }

    const QVariantMap payload = reply.value();
    if (!payload.value(QStringLiteral("ok")).toBool()) {
        setReduceAnimation(false);
        setDisableBlur(false);
        setDisableHeavyEffects(false);
        if (!m_context.isEmpty()) {
            m_context.clear();
            emit contextChanged();
        }
        return;
    }
    applyContext(payload.value(QStringLiteral("context")).toMap());
}

void ContextClient::onContextChanged(const QVariantMap &context)
{
    applyContext(context);
}

void ContextClient::onPowerChanged(const QVariantMap &power)
{
    // Compatibility path: newer services may emit PowerChanged and legacy ones
    // may emit PowerStateChanged. Refresh only when the local snapshot is stale.
    if (m_context.value(QStringLiteral("power")).toMap() == power) {
        return;
    }
    refresh();
}

void ContextClient::onNameOwnerChanged(const QString &name,
                                       const QString &oldOwner,
                                       const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (name != QString::fromLatin1(kService)) {
        return;
    }
    setServiceAvailable(!newOwner.isEmpty());
    if (newOwner.isEmpty()) {
        setReduceAnimation(false);
        setDisableBlur(false);
        setDisableHeavyEffects(false);
        if (!m_context.isEmpty()) {
            m_context.clear();
            emit contextChanged();
        }
        return;
    }
    refresh();
}

void ContextClient::bindSignals()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("ContextChanged"),
                this,
                SLOT(onContextChanged(QVariantMap)));
    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("PowerChanged"),
                this,
                SLOT(onPowerChanged(QVariantMap)));
    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("PowerStateChanged"),
                this,
                SLOT(onPowerChanged(QVariantMap)));

    m_watcher = new QDBusServiceWatcher(QString::fromLatin1(kService),
                                        bus,
                                        QDBusServiceWatcher::WatchForOwnerChange,
                                        this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &ContextClient::onNameOwnerChanged);
}

void ContextClient::applyContext(const QVariantMap &context)
{
    if (m_context != context) {
        m_context = context;
        emit contextChanged();
    }
    const QVariantMap rules = context.value(QStringLiteral("rules")).toMap();
    const QVariantMap actions = rules.value(QStringLiteral("actions")).toMap();
    setReduceAnimation(actions.value(QStringLiteral("ui.reduceAnimation")).toBool());
    setDisableBlur(actions.value(QStringLiteral("ui.disableBlur")).toBool());
    setDisableHeavyEffects(actions.value(QStringLiteral("ui.disableHeavyEffects")).toBool());
}

void ContextClient::setServiceAvailable(bool available)
{
    if (m_serviceAvailable == available) {
        return;
    }
    m_serviceAvailable = available;
    emit serviceAvailableChanged();
}

void ContextClient::setReduceAnimation(bool reduce)
{
    if (m_reduceAnimation == reduce) {
        return;
    }
    m_reduceAnimation = reduce;
    emit reduceAnimationChanged();
}

void ContextClient::setDisableBlur(bool disable)
{
    if (m_disableBlur == disable) {
        return;
    }
    m_disableBlur = disable;
    emit disableBlurChanged();
}

void ContextClient::setDisableHeavyEffects(bool disable)
{
    if (m_disableHeavyEffects == disable) {
        return;
    }
    m_disableHeavyEffects = disable;
    emit disableHeavyEffectsChanged();
}

} // namespace Slm::Context
