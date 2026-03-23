#include "notificationmanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDebug>

namespace {
constexpr const char *kNotificationService = "org.freedesktop.Notifications";
constexpr const char *kNotificationPath = "/org/freedesktop/Notifications";
} // namespace

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
    case AppIconRole:
        return item.appIcon;
    case SummaryRole:
        return item.summary;
    case BodyRole:
        return item.body;
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
    roles[AppIconRole] = "appIcon";
    roles[SummaryRole] = "summary";
    roles[BodyRole] = "body";
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

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
{
    m_model = new NotificationListModel(this);
    registerDbusService();
}

NotificationManager::~NotificationManager()
{
    if (m_serviceRegistered) {
        QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kNotificationPath));
        QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kNotificationService));
    }
}

bool NotificationManager::serviceRegistered() const
{
    return m_serviceRegistered;
}

QAbstractListModel* NotificationManager::notifications() const
{
    return m_model;
}

int NotificationManager::count() const
{
    return m_model ? m_model->count() : 0;
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

void NotificationManager::clearAll()
{
    const int before = count();
    if (m_model) {
        m_model->clear();
    }
    emitCountIfChanged(before);
}

bool NotificationManager::closeById(uint id)
{
    const int before = count();
    const bool ok = m_model && m_model->removeById(id);
    if (ok) {
        emit NotificationClosed(id, 2u);
        emitCountIfChanged(before);
    }
    return ok;
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
    version = QStringLiteral("0.1");
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
    Q_UNUSED(actions)

    NotificationEntry entry;
    if (replacesId > 0) {
        entry.id = replacesId;
    } else {
        entry.id = m_nextId++;
    }
    entry.appName = appName.trimmed().isEmpty() ? QStringLiteral("Application") : appName.trimmed();
    entry.appIcon = appIcon.trimmed();
    entry.summary = summary;
    entry.body = body;
    entry.urgency = urgencyFromHints(hints);
    entry.expireTimeoutMs = expireTimeout;
    entry.timestamp = QDateTime::currentDateTime();

    if (m_doNotDisturb) {
        return entry.id;
    }

    m_latestNotification = {
        {QStringLiteral("id"), static_cast<int>(entry.id)},
        {QStringLiteral("appName"), entry.appName},
        {QStringLiteral("appIcon"), entry.appIcon},
        {QStringLiteral("summary"), entry.summary},
        {QStringLiteral("body"), entry.body},
        {QStringLiteral("urgency"), entry.urgency},
        {QStringLiteral("timestamp"), entry.timestamp.toString(Qt::ISODate)}
    };
    emit latestNotificationChanged();

    const int before = count();
    if (m_model) {
        m_model->upsert(entry);
    }
    emitCountIfChanged(before);
    return entry.id;
}

void NotificationManager::CloseNotification(uint id)
{
    closeById(id);
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

    if (iface->isServiceRegistered(QString::fromLatin1(kNotificationService)).value()) {
        qInfo() << "NotificationManager: service already owned, running in passive mode";
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kNotificationService))) {
        qWarning() << "NotificationManager: failed to register service:"
                   << bus.lastError().name() << bus.lastError().message();
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool registeredObject = bus.registerObject(QString::fromLatin1(kNotificationPath),
                                                     this,
                                                     QDBusConnection::ExportAllSlots |
                                                         QDBusConnection::ExportAllSignals);
    if (!registeredObject) {
        qWarning() << "NotificationManager: failed to register object path";
        bus.unregisterService(QString::fromLatin1(kNotificationService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
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
