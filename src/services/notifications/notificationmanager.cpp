#include "notificationmanager.h"

#include "desktopnotificationadaptor.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDebug>

namespace {
constexpr const char *kFreedesktopNotificationService = "org.freedesktop.Notifications";
constexpr const char *kFreedesktopNotificationPath = "/org/freedesktop/Notifications";
constexpr const char *kDesktopNotificationService = "org.example.Desktop.Notification";
constexpr const char *kDesktopNotificationPath = "/org/example/Desktop/Notification";
}

NotificationListModel::NotificationListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int NotificationListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant NotificationListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }
    const NotificationEntry &item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return static_cast<int>(item.id);
    case AppNameRole:
        return item.appName;
    case AppIdRole:
        return item.appId;
    case AppIconRole:
        return item.appIcon;
    case SummaryRole:
        return item.summary;
    case BodyRole:
        return item.body;
    case ActionsRole:
        return item.actions;
    case PriorityRole:
        return item.priority;
    case GroupIdRole:
        return item.groupId;
    case ReadRole:
        return item.read;
    case BannerRole:
        return item.banner;
    case UrgencyRole:
        return item.urgency;
    case TimestampRole:
        return item.timestamp.toString(Qt::ISODate);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> NotificationListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "notificationId";
    roles[AppNameRole] = "appName";
    roles[AppIdRole] = "appId";
    roles[AppIconRole] = "appIcon";
    roles[SummaryRole] = "summary";
    roles[BodyRole] = "body";
    roles[ActionsRole] = "actions";
    roles[PriorityRole] = "priority";
    roles[GroupIdRole] = "groupId";
    roles[ReadRole] = "read";
    roles[BannerRole] = "banner";
    roles[UrgencyRole] = "urgency";
    roles[TimestampRole] = "timestamp";
    return roles;
}

void NotificationListModel::upsert(const NotificationEntry &entry)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == entry.id) {
            m_items[i] = entry;
            const QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx);
            return;
        }
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(entry);
    endInsertRows();
}

bool NotificationListModel::removeById(uint id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            m_items.removeAt(i);
            endRemoveRows();
            return true;
        }
    }
    return false;
}

bool NotificationListModel::markReadById(uint id, bool read)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id != id) {
            continue;
        }
        if (m_items.at(i).read == read) {
            return false;
        }
        m_items[i].read = read;
        const QModelIndex idx = index(i, 0);
        emit dataChanged(idx, idx, {ReadRole});
        return true;
    }
    return false;
}

int NotificationListModel::markAllRead(bool read)
{
    int changed = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).read == read) {
            continue;
        }
        m_items[i].read = read;
        const QModelIndex idx = index(i, 0);
        emit dataChanged(idx, idx, {ReadRole});
        ++changed;
    }
    return changed;
}

void NotificationListModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }
    beginResetModel();
    m_items.clear();
    endResetModel();
}

int NotificationListModel::count() const
{
    return m_items.size();
}

int NotificationListModel::unreadCount() const
{
    int unread = 0;
    for (const NotificationEntry &entry : m_items) {
        if (!entry.read) {
            ++unread;
        }
    }
    return unread;
}

int NotificationListModel::unreadCountForApp(const QString &appName) const
{
    const QString wanted = appName.trimmed();
    if (wanted.isEmpty()) {
        return 0;
    }
    int unread = 0;
    for (const NotificationEntry &entry : m_items) {
        if (entry.read) {
            continue;
        }
        if (entry.appName == wanted) {
            ++unread;
        }
    }
    return unread;
}

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
{
    m_model = new NotificationListModel(this);
    m_bannerModel = new NotificationListModel(this);
    new DesktopNotificationAdaptor(this);
    registerDbusService();
}

NotificationManager::~NotificationManager()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (m_serviceRegistered) {
        bus.unregisterObject(QString::fromLatin1(kFreedesktopNotificationPath));
        bus.unregisterService(QString::fromLatin1(kFreedesktopNotificationService));
    }
    if (m_desktopServiceRegistered) {
        bus.unregisterObject(QString::fromLatin1(kDesktopNotificationPath));
        bus.unregisterService(QString::fromLatin1(kDesktopNotificationService));
    }
}

bool NotificationManager::serviceRegistered() const
{
    return m_serviceRegistered;
}

bool NotificationManager::desktopServiceRegistered() const
{
    return m_desktopServiceRegistered;
}

QAbstractListModel* NotificationManager::notifications() const
{
    return m_model;
}

QAbstractListModel* NotificationManager::bannerNotifications() const
{
    return m_bannerModel;
}

int NotificationManager::count() const
{
    return m_model ? m_model->count() : 0;
}

int NotificationManager::unreadCount() const
{
    return m_model ? m_model->unreadCount() : 0;
}

int NotificationManager::unreadCountForApp(const QString &appName) const
{
    return m_model ? m_model->unreadCountForApp(appName) : 0;
}

bool NotificationManager::doNotDisturb() const
{
    return m_doNotDisturb;
}

void NotificationManager::setDoNotDisturb(bool enabled)
{
    if (m_doNotDisturb == enabled) {
        return;
    }
    m_doNotDisturb = enabled;
    emit doNotDisturbChanged();
}

QVariantMap NotificationManager::latestNotification() const
{
    return m_latestNotification;
}

int NotificationManager::bubbleDurationMs() const
{
    return m_bubbleDurationMs;
}

void NotificationManager::setBubbleDurationMs(int durationMs)
{
    const int clamped = qBound(1000, durationMs, 60000);
    if (m_bubbleDurationMs == clamped) {
        return;
    }
    m_bubbleDurationMs = clamped;
    emit bubbleDurationMsChanged();
}

bool NotificationManager::centerVisible() const
{
    return m_centerVisible;
}

void NotificationManager::clearAll()
{
    const int before = count();
    const int beforeUnread = unreadCount();
    if (m_model) {
        m_model->clear();
    }
    if (m_bannerModel) {
        m_bannerModel->clear();
    }
    emitCountIfChanged(before);
    emitUnreadCountIfChanged(beforeUnread);
}

bool NotificationManager::closeById(uint id)
{
    const int before = count();
    const int beforeUnread = unreadCount();
    const bool ok = m_model && m_model->removeById(id);
    if (m_bannerModel) {
        m_bannerModel->removeById(id);
    }
    if (ok) {
        emit NotificationClosed(id, 2u);
        emit NotificationRemoved(id);
        emitCountIfChanged(before);
        emitUnreadCountIfChanged(beforeUnread);
    }
    return ok;
}

void NotificationManager::toggleCenter()
{
    m_centerVisible = !m_centerVisible;
    if (m_centerVisible) {
        markAllRead();
    }
    emit centerVisibleChanged();
}

QVariantList NotificationManager::getAll() const
{
    return GetAll();
}

uint NotificationManager::notifySimple(const QString &appId,
                                       const QString &title,
                                       const QString &body,
                                       const QString &icon,
                                       const QStringList &actions,
                                       const QString &priority)
{
    return NotifyModern(appId, title, body, icon, actions, priority);
}

bool NotificationManager::dismiss(uint id)
{
    return Dismiss(id);
}

QStringList NotificationManager::GetCapabilities() const
{
    return {
        QStringLiteral("body"),
        QStringLiteral("body-markup"),
        QStringLiteral("actions")
    };
}

void NotificationManager::GetServerInformation(QString &name,
                                               QString &vendor,
                                               QString &version,
                                               QString &specVersion) const
{
    name = QStringLiteral("SLM Notification Center");
    vendor = QStringLiteral("SLM");
    version = QStringLiteral("0.2");
    specVersion = QStringLiteral("1.2");
}

uint NotificationManager::Notify(const QString &appName,
                                 uint replacesId,
                                 const QString &appIcon,
                                 const QString &summary,
                                 const QString &body,
                                 const QStringList &actions,
                                 const QVariantMap &hints,
                                 int expireTimeout)
{
    NotificationEntry entry;
    entry.id = replacesId > 0 ? replacesId : m_nextId++;
    entry.appName = appName.trimmed().isEmpty() ? QStringLiteral("Application") : appName.trimmed();
    entry.appId = entry.appName;
    entry.appIcon = appIcon.trimmed();
    entry.summary = summary;
    entry.body = body;
    entry.actions = actions;
    entry.urgency = urgencyFromHints(hints);
    entry.priority = (entry.urgency >= 2) ? QStringLiteral("high")
                                           : ((entry.urgency <= 0) ? QStringLiteral("low")
                                                                   : QStringLiteral("normal"));
    entry.groupId = entry.appId;
    entry.read = false;
    entry.banner = !m_doNotDisturb && entry.priority != QStringLiteral("low");
    entry.expireTimeoutMs = expireTimeout;
    entry.timestamp = QDateTime::currentDateTime();
    return upsertNotification(entry, m_doNotDisturb || entry.priority == QStringLiteral("low"));
}

void NotificationManager::CloseNotification(uint id)
{
    closeById(id);
}

uint NotificationManager::NotifyModern(const QString &appId,
                                       const QString &title,
                                       const QString &body,
                                       const QString &icon,
                                       const QStringList &actions,
                                       const QString &priority)
{
    NotificationEntry entry;
    entry.id = m_nextId++;
    entry.appId = appId.trimmed().isEmpty() ? QStringLiteral("unknown.app") : appId.trimmed();
    entry.appName = entry.appId;
    entry.appIcon = icon.trimmed();
    entry.summary = title;
    entry.body = body;
    entry.actions = actions;
    entry.priority = normalizePriority(priority);
    entry.urgency = (entry.priority == QStringLiteral("high")) ? 2
                  : ((entry.priority == QStringLiteral("low")) ? 0 : 1);
    entry.groupId = entry.appId;
    entry.read = false;
    entry.banner = !m_doNotDisturb && entry.priority != QStringLiteral("low");
    entry.expireTimeoutMs = -1;
    entry.timestamp = QDateTime::currentDateTime();
    return upsertNotification(entry, m_doNotDisturb || entry.priority == QStringLiteral("low"));
}

bool NotificationManager::Dismiss(uint id)
{
    return closeById(id);
}

bool NotificationManager::dismissBanner(uint id)
{
    return m_bannerModel ? m_bannerModel->removeById(id) : false;
}

QVariantList NotificationManager::GetAll() const
{
    QVariantList rows;
    if (!m_model) {
        return rows;
    }
    const int countRows = m_model->rowCount();
    rows.reserve(countRows);
    for (int i = 0; i < countRows; ++i) {
        const QModelIndex idx = m_model->index(i, 0);
        NotificationEntry entry;
        entry.id = static_cast<uint>(m_model->data(idx, NotificationListModel::IdRole).toUInt());
        entry.appName = m_model->data(idx, NotificationListModel::AppNameRole).toString();
        entry.appId = m_model->data(idx, NotificationListModel::AppIdRole).toString();
        entry.appIcon = m_model->data(idx, NotificationListModel::AppIconRole).toString();
        entry.summary = m_model->data(idx, NotificationListModel::SummaryRole).toString();
        entry.body = m_model->data(idx, NotificationListModel::BodyRole).toString();
        entry.actions = m_model->data(idx, NotificationListModel::ActionsRole).toStringList();
        entry.priority = m_model->data(idx, NotificationListModel::PriorityRole).toString();
        entry.groupId = m_model->data(idx, NotificationListModel::GroupIdRole).toString();
        entry.read = m_model->data(idx, NotificationListModel::ReadRole).toBool();
        entry.banner = m_model->data(idx, NotificationListModel::BannerRole).toBool();
        entry.urgency = m_model->data(idx, NotificationListModel::UrgencyRole).toInt();
        entry.timestamp = QDateTime::fromString(m_model->data(idx, NotificationListModel::TimestampRole).toString(),
                                                Qt::ISODate);
        rows.push_back(toVariantMap(entry));
    }
    return rows;
}

void NotificationManager::ClearAll()
{
    clearAll();
}

bool NotificationManager::ToggleCenter()
{
    toggleCenter();
    return m_centerVisible;
}

void NotificationManager::markAllRead()
{
    const int beforeUnread = unreadCount();
    if (!m_model) {
        return;
    }
    m_model->markAllRead(true);
    emitUnreadCountIfChanged(beforeUnread);
}

bool NotificationManager::markRead(uint id, bool read)
{
    const int beforeUnread = unreadCount();
    if (!m_model) {
        return false;
    }
    const bool changed = m_model->markReadById(id, read);
    if (changed && read && m_bannerModel) {
        m_bannerModel->removeById(id);
    }
    emitUnreadCountIfChanged(beforeUnread);
    return changed;
}

void NotificationManager::invokeAction(uint id, const QString &actionKey)
{
    emit ActionInvoked(id, actionKey.trimmed().isEmpty() ? QStringLiteral("default") : actionKey.trimmed());
}

void NotificationManager::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "NotificationManager: session DBus not connected";
        return;
    }

    auto iface = bus.interface();
    if (!iface) {
        qWarning() << "NotificationManager: DBus interface unavailable";
        return;
    }

    // Freedesktop compatibility service (can be passive if another daemon already owns it).
    if (iface->isServiceRegistered(QString::fromLatin1(kFreedesktopNotificationService)).value()) {
        qInfo() << "NotificationManager: freedesktop service already owned, passive compatibility mode";
        m_serviceRegistered = false;
    } else {
        if (!bus.registerService(QString::fromLatin1(kFreedesktopNotificationService))) {
            qWarning() << "NotificationManager: failed to register freedesktop service:"
                       << bus.lastError().name() << bus.lastError().message();
            m_serviceRegistered = false;
        } else {
            const bool registeredObject = bus.registerObject(QString::fromLatin1(kFreedesktopNotificationPath),
                                                             this,
                                                             QDBusConnection::ExportAllSlots |
                                                                 QDBusConnection::ExportAllSignals);
            if (!registeredObject) {
                qWarning() << "NotificationManager: failed to register freedesktop object path";
                bus.unregisterService(QString::fromLatin1(kFreedesktopNotificationService));
                m_serviceRegistered = false;
            } else {
                m_serviceRegistered = true;
            }
        }
    }
    emit serviceRegisteredChanged();

    // New desktop-native service.
    if (!iface->isServiceRegistered(QString::fromLatin1(kDesktopNotificationService)).value()
        && bus.registerService(QString::fromLatin1(kDesktopNotificationService))) {
        const bool ok = bus.registerObject(QString::fromLatin1(kDesktopNotificationPath),
                                           this,
                                           QDBusConnection::ExportAdaptors |
                                               QDBusConnection::ExportScriptableSignals);
        m_desktopServiceRegistered = ok;
        if (!ok) {
            qWarning() << "NotificationManager: failed to register desktop notification object path";
            bus.unregisterService(QString::fromLatin1(kDesktopNotificationService));
        }
    } else {
        if (iface->isServiceRegistered(QString::fromLatin1(kDesktopNotificationService)).value()) {
            qInfo() << "NotificationManager: desktop notification service already owned";
        } else {
            qWarning() << "NotificationManager: failed to register desktop notification service:"
                       << bus.lastError().name() << bus.lastError().message();
        }
        m_desktopServiceRegistered = false;
    }
    emit desktopServiceRegisteredChanged();
}

int NotificationManager::urgencyFromHints(const QVariantMap &hints) const
{
    const QVariant urgency = hints.value(QStringLiteral("urgency"));
    bool ok = false;
    const int parsed = urgency.toInt(&ok);
    if (ok) {
        return qBound(0, parsed, 2);
    }
    return 1;
}

void NotificationManager::emitCountIfChanged(int previousCount)
{
    if (previousCount != count()) {
        emit countChanged();
    }
}

void NotificationManager::emitUnreadCountIfChanged(int previousUnreadCount)
{
    if (previousUnreadCount != unreadCount()) {
        emit unreadCountChanged();
    }
}

QString NotificationManager::normalizePriority(const QString &priority) const
{
    const QString lowered = priority.trimmed().toLower();
    if (lowered == QStringLiteral("low") ||
        lowered == QStringLiteral("normal") ||
        lowered == QStringLiteral("high")) {
        return lowered;
    }
    return QStringLiteral("normal");
}

uint NotificationManager::upsertNotification(const NotificationEntry &entry, bool suppressBanner)
{
    const int before = count();
    const int beforeUnread = unreadCount();
    if (m_model) {
        m_model->upsert(entry);
    }
    if (m_bannerModel) {
        if (entry.banner) {
            m_bannerModel->upsert(entry);
        } else {
            m_bannerModel->removeById(entry.id);
        }
    }
    emitCountIfChanged(before);
    emitUnreadCountIfChanged(beforeUnread);
    emit NotificationAdded(entry.id);

    if (!suppressBanner) {
        updateLatestNotification(entry);
    }
    return entry.id;
}

QVariantMap NotificationManager::toVariantMap(const NotificationEntry &entry) const
{
    return {
        {QStringLiteral("id"), static_cast<int>(entry.id)},
        {QStringLiteral("app_id"), entry.appId},
        {QStringLiteral("app_name"), entry.appName},
        {QStringLiteral("title"), entry.summary},
        {QStringLiteral("body"), entry.body},
        {QStringLiteral("icon"), entry.appIcon},
        {QStringLiteral("timestamp"), entry.timestamp.toString(Qt::ISODate)},
        {QStringLiteral("actions"), entry.actions},
        {QStringLiteral("priority"), entry.priority},
        {QStringLiteral("group_id"), entry.groupId},
        {QStringLiteral("read"), entry.read},
        {QStringLiteral("banner"), entry.banner}
    };
}

void NotificationManager::updateLatestNotification(const NotificationEntry &entry)
{
    m_latestNotification = {
        {QStringLiteral("id"), static_cast<int>(entry.id)},
        {QStringLiteral("appId"), entry.appId},
        {QStringLiteral("appName"), entry.appName},
        {QStringLiteral("appIcon"), entry.appIcon},
        {QStringLiteral("summary"), entry.summary},
        {QStringLiteral("body"), entry.body},
        {QStringLiteral("urgency"), entry.urgency},
        {QStringLiteral("priority"), entry.priority},
        {QStringLiteral("timestamp"), entry.timestamp.toString(Qt::ISODate)}
    };
    emit latestNotificationChanged();
}
