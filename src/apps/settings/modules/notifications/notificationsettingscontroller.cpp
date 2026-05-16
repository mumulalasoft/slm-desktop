#include "notificationsettingscontroller.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDateTime>
#include <QFileInfo>
#include <QSettings>
#include <QVariantMap>
#include <algorithm>

namespace {
constexpr const char kService[] = "org.example.Desktop.Notification";
constexpr const char kPath[] = "/org/example/Desktop/Notification";
constexpr const char kIface[] = "org.example.Desktop.Notification";
constexpr const char kOrg[] = "SLM";
constexpr const char kApp[] = "slm-settings";

QString sanitizeToken(QString token)
{
    token = token.trimmed().toLower();
    if (token.isEmpty()) {
        return QStringLiteral("unknown.app");
    }
    if (token.contains(QLatin1Char('/'))) {
        token = QFileInfo(token).fileName();
    }
    if (token.endsWith(QStringLiteral(".desktop"))) {
        token.chop(8);
    }
    token.replace(QLatin1Char(' '), QLatin1Char('.'));
    QString out;
    out.reserve(token.size());
    for (QChar ch : token) {
        const bool allowed = ch.isLetterOrNumber()
                || ch == QLatin1Char('.')
                || ch == QLatin1Char('-')
                || ch == QLatin1Char('_');
        out.push_back(allowed ? ch : QLatin1Char('.'));
    }
    while (out.contains(QStringLiteral(".."))) {
        out.replace(QStringLiteral(".."), QStringLiteral("."));
    }
    while (out.startsWith(QLatin1Char('.'))) {
        out.remove(0, 1);
    }
    while (out.endsWith(QLatin1Char('.'))) {
        out.chop(1);
    }
    return out.isEmpty() ? QStringLiteral("unknown.app") : out;
}
} // namespace

NotificationSettingsController::NotificationSettingsController(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kService),
                                          QString::fromLatin1(kPath),
                                          QString::fromLatin1(kIface),
                                          QStringLiteral("NotificationAdded"),
                                          this,
                                          SLOT(refresh()));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kService),
                                          QString::fromLatin1(kPath),
                                          QString::fromLatin1(kIface),
                                          QStringLiteral("NotificationRemoved"),
                                          this,
                                          SLOT(refresh()));
    refresh();
}

QVariantList NotificationSettingsController::appRows() const
{
    return m_appRows;
}

bool NotificationSettingsController::notifyServiceAvailable() const
{
    return m_notifyServiceAvailable;
}

QString NotificationSettingsController::normalizedAppId(const QString &raw) const
{
    return sanitizeToken(raw);
}

QString NotificationSettingsController::prefKey(const QString &appId, const QString &name) const
{
    const QString app = normalizedAppId(appId);
    const QString key = sanitizeToken(name).replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("notifications/apps/%1/%2").arg(app, key);
}

void NotificationSettingsController::setNotifyServiceAvailable(bool available)
{
    if (m_notifyServiceAvailable == available) {
        return;
    }
    m_notifyServiceAvailable = available;
    emit notifyServiceAvailableChanged();
}

void NotificationSettingsController::refresh()
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        setNotifyServiceAvailable(false);
        if (!m_appRows.isEmpty()) {
            m_appRows.clear();
            emit appRowsChanged();
        }
        return;
    }

    setNotifyServiceAvailable(true);

    const QDBusPendingCall call = iface.asyncCall(QStringLiteral("GetAll"));
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QVariantList> reply = *w;
        w->deleteLater();
        if (reply.isError()) {
            setNotifyServiceAvailable(false);
            if (!m_appRows.isEmpty()) {
                m_appRows.clear();
                emit appRowsChanged();
            }
            return;
        }
        setNotifyServiceAvailable(true);
        recomputeRows(reply.value());
    });
}

void NotificationSettingsController::recomputeRows(const QVariantList &items)
{
    QHash<QString, QVariantMap> byApp;

    for (const QVariant &rowVar : items) {
        const QVariantMap row = rowVar.toMap();
        QString appId = normalizedAppId(row.value(QStringLiteral("app_id")).toString());
        QString appName = row.value(QStringLiteral("app_name")).toString().trimmed();
        if (appName.isEmpty()) {
            appName = appId;
        }

        QVariantMap agg = byApp.value(appId);
        if (agg.isEmpty()) {
            agg.insert(QStringLiteral("appId"), appId);
            agg.insert(QStringLiteral("appName"), appName);
            agg.insert(QStringLiteral("appIcon"), row.value(QStringLiteral("icon")).toString());
            agg.insert(QStringLiteral("count"), 0);
            agg.insert(QStringLiteral("unreadCount"), 0);
            agg.insert(QStringLiteral("lastSummary"), QString());
            agg.insert(QStringLiteral("lastBody"), QString());
            agg.insert(QStringLiteral("lastTimestamp"), QString());
        }

        agg.insert(QStringLiteral("count"), agg.value(QStringLiteral("count")).toInt() + 1);
        if (!row.value(QStringLiteral("read")).toBool()) {
            agg.insert(QStringLiteral("unreadCount"), agg.value(QStringLiteral("unreadCount")).toInt() + 1);
        }

        if (agg.value(QStringLiteral("lastSummary")).toString().isEmpty()) {
            agg.insert(QStringLiteral("lastSummary"), row.value(QStringLiteral("title")).toString());
            agg.insert(QStringLiteral("lastBody"), row.value(QStringLiteral("body")).toString());
            agg.insert(QStringLiteral("lastTimestamp"), row.value(QStringLiteral("timestamp")).toString());
        }

        byApp.insert(appId, agg);
    }

    QVariantList out;
    out.reserve(byApp.size());
    for (auto it = byApp.constBegin(); it != byApp.constEnd(); ++it) {
        out.push_back(it.value());
    }

    std::sort(out.begin(), out.end(), [](const QVariant &aVar, const QVariant &bVar) {
        const QVariantMap a = aVar.toMap();
        const QVariantMap b = bVar.toMap();
        const int aUnread = a.value(QStringLiteral("unreadCount")).toInt();
        const int bUnread = b.value(QStringLiteral("unreadCount")).toInt();
        if (aUnread != bUnread) {
            return aUnread > bUnread;
        }
        const int aCount = a.value(QStringLiteral("count")).toInt();
        const int bCount = b.value(QStringLiteral("count")).toInt();
        if (aCount != bCount) {
            return aCount > bCount;
        }
        return a.value(QStringLiteral("appName")).toString().toLower()
             < b.value(QStringLiteral("appName")).toString().toLower();
    });

    m_appRows = out;
    emit appRowsChanged();
}

bool NotificationSettingsController::sendPreviewNotification(const QString &appId,
                                                             const QString &title,
                                                             const QString &body,
                                                             const QString &priority,
                                                             bool grouped)
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        setNotifyServiceAvailable(false);
        return false;
    }

    setNotifyServiceAvailable(true);
    const QString effectiveAppId = grouped ? normalizedAppId(appId)
                                           : QStringLiteral("%1.%2")
                                                 .arg(normalizedAppId(appId))
                                                 .arg(QDateTime::currentMSecsSinceEpoch());
    QDBusReply<uint> reply = iface.call(QStringLiteral("Notify"),
                                        effectiveAppId,
                                        title,
                                        body,
                                        QStringLiteral("preferences-system-notifications"),
                                        QStringList{QStringLiteral("default"), QStringLiteral("Open")},
                                        priority);
    const bool ok = reply.isValid();
    if (ok) {
        refresh();
    }
    return ok;
}

bool NotificationSettingsController::clearHistory()
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        setNotifyServiceAvailable(false);
        return false;
    }
    setNotifyServiceAvailable(true);

    const QDBusMessage reply = iface.call(QStringLiteral("ClearAll"));
    const bool ok = reply.type() != QDBusMessage::ErrorMessage;
    if (ok) {
        refresh();
    }
    return ok;
}

QVariantMap NotificationSettingsController::appPreferences(const QString &appId) const
{
    const QString normalized = normalizedAppId(appId);
    const QVariantMap defaults = {
        {QStringLiteral("allow_notifications"), true},
        {QStringLiteral("show_popups"), true},
        {QStringLiteral("show_in_pulse_feed"), true},
        {QStringLiteral("sound"), true},
        {QStringLiteral("badge"), true},
        {QStringLiteral("priority"), QStringLiteral("normal")},
        {QStringLiteral("grouping"), QStringLiteral("auto")},
        {QStringLiteral("lockscreen_visibility"), QStringLiteral("show")}
    };

    QSettings settings(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
    QVariantMap out = defaults;
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        out.insert(it.key(), settings.value(prefKey(normalized, it.key()), it.value()));
    }
    return out;
}

QVariant NotificationSettingsController::appPreference(const QString &appId,
                                                       const QString &name,
                                                       const QVariant &fallback) const
{
    QSettings settings(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
    return settings.value(prefKey(appId, name), fallback);
}

bool NotificationSettingsController::setAppPreference(const QString &appId,
                                                      const QString &name,
                                                      const QVariant &value)
{
    const QString key = prefKey(appId, name);
    if (key.isEmpty()) {
        return false;
    }

    QSettings settings(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
    settings.setValue(key, value);
    settings.sync();
    return settings.status() == QSettings::NoError;
}
