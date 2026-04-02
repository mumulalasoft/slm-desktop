#include "ClipboardServiceClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QDBusReply>

namespace Slm::Clipboard {
namespace {
constexpr const char kService[] = "org.desktop.Clipboard";
constexpr const char kPath[] = "/org/desktop/Clipboard";
constexpr const char kIface[] = "org.desktop.Clipboard";
}

ClipboardServiceClient::ClipboardServiceClient(QObject *parent)
    : QObject(parent)
    , m_iface(new QDBusInterface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIface),
                                 QDBusConnection::sessionBus(),
                                 this))
{
    bindSignals();
    refreshServiceAvailability();
    refreshHistory();
}

ClipboardServiceClient::~ClipboardServiceClient() = default;

bool ClipboardServiceClient::serviceAvailable() const
{
    return m_serviceAvailable;
}

QVariantList ClipboardServiceClient::history() const
{
    return m_history;
}

QVariantList ClipboardServiceClient::getHistory(int limit)
{
    if (!m_iface || !m_iface->isValid()) {
        return {};
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("GetHistory"), limit);
    return reply.isValid() ? reply.value() : QVariantList{};
}

QVariantList ClipboardServiceClient::search(const QString &query, int limit)
{
    if (!m_iface || !m_iface->isValid()) {
        return {};
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("Search"), query, limit);
    return reply.isValid() ? reply.value() : QVariantList{};
}

bool ClipboardServiceClient::pasteItem(qlonglong id)
{
    if (!m_iface || !m_iface->isValid() || id <= 0) {
        return false;
    }
    QDBusReply<bool> reply = m_iface->call(QStringLiteral("PasteItem"), id);
    return reply.isValid() && reply.value();
}

bool ClipboardServiceClient::deleteItem(qlonglong id)
{
    if (!m_iface || !m_iface->isValid() || id <= 0) {
        return false;
    }
    QDBusReply<bool> reply = m_iface->call(QStringLiteral("DeleteItem"), id);
    if (reply.isValid() && reply.value()) {
        refreshHistory();
        return true;
    }
    return false;
}

bool ClipboardServiceClient::pinItem(qlonglong id, bool pinned)
{
    if (!m_iface || !m_iface->isValid() || id <= 0) {
        return false;
    }
    QDBusReply<bool> reply = m_iface->call(QStringLiteral("PinItem"), id, pinned);
    if (reply.isValid() && reply.value()) {
        refreshHistory();
        return true;
    }
    return false;
}

bool ClipboardServiceClient::clearHistory()
{
    if (!m_iface || !m_iface->isValid()) {
        return false;
    }
    QDBusReply<bool> reply = m_iface->call(QStringLiteral("ClearHistory"));
    if (reply.isValid() && reply.value()) {
        refreshHistory();
        return true;
    }
    return false;
}

void ClipboardServiceClient::refreshHistory(int limit)
{
    const QVariantList updated = getHistory(limit);
    if (updated == m_history) {
        return;
    }
    m_history = updated;
    emit historyChanged();
}

void ClipboardServiceClient::onHistoryUpdated()
{
    refreshHistory();
}

void ClipboardServiceClient::onClipboardChanged(const QVariantMap &item)
{
    refreshHistory();
    emit clipboardChanged(item);
}

void ClipboardServiceClient::onNameOwnerChanged(const QString &name,
                                                const QString &oldOwner,
                                                const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (name != QString::fromLatin1(kService)) {
        return;
    }
    const bool nowAvailable = !newOwner.isEmpty();
    if (m_serviceAvailable != nowAvailable) {
        m_serviceAvailable = nowAvailable;
        emit serviceAvailableChanged();
    }
    if (nowAvailable) {
        refreshHistory();
    } else if (!m_history.isEmpty()) {
        m_history.clear();
        emit historyChanged();
    }
}

void ClipboardServiceClient::bindSignals()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("HistoryUpdated"),
                this,
                SLOT(onHistoryUpdated()));

    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("ClipboardChanged"),
                this,
                SLOT(onClipboardChanged(QVariantMap)));

    auto *watcher = new QDBusServiceWatcher(QString::fromLatin1(kService),
                                             bus,
                                             QDBusServiceWatcher::WatchForOwnerChange,
                                             this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &ClipboardServiceClient::onNameOwnerChanged);
}

void ClipboardServiceClient::refreshServiceAvailability()
{
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    const bool available = iface
            ? iface->isServiceRegistered(QString::fromLatin1(kService)).value()
            : false;
    if (m_serviceAvailable == available) {
        return;
    }
    m_serviceAvailable = available;
    emit serviceAvailableChanged();
}

} // namespace Slm::Clipboard
