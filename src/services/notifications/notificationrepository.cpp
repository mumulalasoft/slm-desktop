#include "notificationrepository.h"

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
    case LifecycleStateRole:
        return item.lifecycleState;
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
    roles[LifecycleStateRole] = "lifecycleState";
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

int NotificationListModel::unreadCountForAppId(const QString &appId) const
{
    const QString wanted = appId.trimmed();
    if (wanted.isEmpty()) {
        return 0;
    }
    int unread = 0;
    for (const NotificationEntry &entry : m_items) {
        if (entry.read) {
            continue;
        }
        if (entry.appId == wanted) {
            ++unread;
        }
    }
    return unread;
}

int NotificationListModel::unreadCountForGroup(const QString &groupId) const
{
    const QString wanted = groupId.trimmed();
    if (wanted.isEmpty()) {
        return 0;
    }
    int unread = 0;
    for (const NotificationEntry &entry : m_items) {
        if (entry.read) {
            continue;
        }
        if (entry.groupId == wanted) {
            ++unread;
        }
    }
    return unread;
}

QString NotificationListModel::displayNameForGroup(const QString &groupId) const
{
    const QString wanted = groupId.trimmed();
    if (wanted.isEmpty()) {
        return QStringLiteral("Application");
    }
    for (const NotificationEntry &entry : m_items) {
        if (entry.groupId != wanted) {
            continue;
        }
        const QString name = entry.appName.trimmed();
        if (!name.isEmpty()) {
            return name;
        }
        const QString id = entry.appId.trimmed();
        if (!id.isEmpty()) {
            return id;
        }
    }
    return wanted;
}

int NotificationListModel::countForAppId(const QString &appId, uint excludeId) const
{
    const QString wanted = appId.trimmed();
    if (wanted.isEmpty()) {
        return 0;
    }
    int count = 0;
    for (const NotificationEntry &entry : m_items) {
        if (excludeId > 0 && entry.id == excludeId) {
            continue;
        }
        if (entry.appId == wanted) {
            ++count;
        }
    }
    return count;
}

bool NotificationListModel::setLifecycleById(uint id, const QString &state)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id != id) {
            continue;
        }
        if (m_items.at(i).lifecycleState == state) {
            return false;
        }
        m_items[i].lifecycleState = state;
        const QModelIndex idx = index(i, 0);
        emit dataChanged(idx, idx, {LifecycleStateRole});
        return true;
    }
    return false;
}

NotificationRepository::NotificationRepository(QObject *parent)
    : QObject(parent)
{
    m_notificationsModel = new NotificationListModel(this);
    m_bannerModel = new NotificationListModel(this);
}

NotificationListModel* NotificationRepository::notificationsModel() const
{
    return m_notificationsModel;
}

NotificationListModel* NotificationRepository::bannerModel() const
{
    return m_bannerModel;
}

int NotificationRepository::count() const
{
    return m_notificationsModel ? m_notificationsModel->count() : 0;
}

int NotificationRepository::unreadCount() const
{
    return m_notificationsModel ? m_notificationsModel->unreadCount() : 0;
}

uint NotificationRepository::nextId()
{
    return m_nextId++;
}

uint NotificationRepository::currentNextId() const
{
    return m_nextId;
}

void NotificationRepository::ensureNextIdAtLeast(uint candidate)
{
    if (candidate > m_nextId) {
        m_nextId = candidate;
    }
}

QVariantMap NotificationRepository::latestNotification() const
{
    return m_latestNotification;
}

void NotificationRepository::updateLatestNotification(const NotificationEntry &entry)
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
}

QVector<NotificationEntry> NotificationRepository::entries() const
{
    QVector<NotificationEntry> rows;
    if (!m_notificationsModel) {
        return rows;
    }
    const int rowCount = m_notificationsModel->rowCount();
    rows.reserve(rowCount);
    for (int i = 0; i < rowCount; ++i) {
        const QModelIndex idx = m_notificationsModel->index(i, 0);
        NotificationEntry entry;
        entry.id = static_cast<uint>(m_notificationsModel->data(idx, NotificationListModel::IdRole).toUInt());
        entry.appName = m_notificationsModel->data(idx, NotificationListModel::AppNameRole).toString();
        entry.appId = m_notificationsModel->data(idx, NotificationListModel::AppIdRole).toString();
        entry.appIcon = m_notificationsModel->data(idx, NotificationListModel::AppIconRole).toString();
        entry.summary = m_notificationsModel->data(idx, NotificationListModel::SummaryRole).toString();
        entry.body = m_notificationsModel->data(idx, NotificationListModel::BodyRole).toString();
        entry.actions = m_notificationsModel->data(idx, NotificationListModel::ActionsRole).toStringList();
        entry.priority = m_notificationsModel->data(idx, NotificationListModel::PriorityRole).toString();
        entry.groupId = m_notificationsModel->data(idx, NotificationListModel::GroupIdRole).toString();
        entry.read = m_notificationsModel->data(idx, NotificationListModel::ReadRole).toBool();
        entry.banner = m_notificationsModel->data(idx, NotificationListModel::BannerRole).toBool();
        entry.urgency = m_notificationsModel->data(idx, NotificationListModel::UrgencyRole).toInt();
        entry.lifecycleState = m_notificationsModel->data(idx, NotificationListModel::LifecycleStateRole).toString();
        entry.timestamp = QDateTime::fromString(m_notificationsModel->data(idx, NotificationListModel::TimestampRole).toString(),
                                                Qt::ISODate);
        rows.push_back(entry);
    }
    return rows;
}
