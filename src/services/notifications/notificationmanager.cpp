#include "notificationmanager.h"

#include "desktopnotificationadaptor.h"
#include "notificationgroupingengine.h"
#include "notificationlifecycleengine.h"
#include "notificationpolicyengine.h"
#include "notificationstore.h"
#include "notificationrepository.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr const char *kFreedesktopNotificationService = "org.freedesktop.Notifications";
constexpr const char *kFreedesktopNotificationPath = "/org/freedesktop/Notifications";
constexpr const char *kDesktopNotificationService = "org.example.Desktop.Notification";
constexpr const char *kDesktopNotificationPath = "/org/example/Desktop/Notification";

QString normalizedAppIdToken(const QString &value)
{
    QString s = value.trimmed().toLower();
    if (s.isEmpty()) {
        return {};
    }
    if (s.contains(QLatin1Char('/'))) {
        s = QFileInfo(s).fileName();
    }
    if (s.endsWith(QStringLiteral(".desktop"))) {
        s.chop(8);
    }
    s.replace(QLatin1Char(' '), QLatin1Char('.'));
    QString out;
    out.reserve(s.size());
    for (QChar ch : s) {
        const bool ok = ch.isLetterOrNumber()
                || ch == QLatin1Char('.')
                || ch == QLatin1Char('-')
                || ch == QLatin1Char('_');
        if (ok) {
            out.push_back(ch);
        } else {
            out.push_back(QLatin1Char('.'));
        }
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
    return out;
}

QString displayNameFromAppId(const QString &appId)
{
    QString id = normalizedAppIdToken(appId);
    if (id.isEmpty()) {
        return QStringLiteral("Application");
    }
    const int dot = id.lastIndexOf(QLatin1Char('.'));
    QString tail = (dot >= 0 && dot + 1 < id.size()) ? id.mid(dot + 1) : id;
    if (tail.isEmpty()) {
        tail = id;
    }
    tail.replace(QLatin1Char('-'), QLatin1Char(' '));
    tail.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!tail.isEmpty()) {
        tail[0] = tail[0].toUpper();
    }
    return tail;
}

QString appIdFromNotifyPayload(const QString &appName, const QVariantMap &hints)
{
    const QStringList candidates = {
        hints.value(QStringLiteral("app-id")).toString(),
        hints.value(QStringLiteral("desktop-entry")).toString(),
        hints.value(QStringLiteral("desktop-file")).toString(),
        appName
    };
    for (const QString &candidate : candidates) {
        const QString id = normalizedAppIdToken(candidate);
        if (!id.isEmpty()) {
            return id;
        }
    }
    return QStringLiteral("unknown.app");
}

bool readBoolPolicy(const QSettings &settings, const QString &key, bool fallback)
{
    return settings.value(key, fallback).toBool();
}

void applyLifecycleToModels(NotificationRepository *repository, uint id, const QString &state)
{
    if (!repository || id == 0) {
        return;
    }
    if (repository->notificationsModel()) {
        repository->notificationsModel()->setLifecycleById(id, state);
    }
    if (repository->bannerModel()) {
        repository->bannerModel()->setLifecycleById(id, state);
    }
}
}

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
{
    m_repository = new NotificationRepository(this);
    m_lifecycle = new NotificationLifecycleEngine();
    new DesktopNotificationAdaptor(this);
    registerDbusService();

    // Resolve history file path under the app data dir.
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_historyFilePath = QDir(dataDir).filePath(QStringLiteral("notification_history.json"));

    // Debounced save: write at most once per 2 s when notifications change.
    m_saveTimer.setInterval(2000);
    m_saveTimer.setSingleShot(true);
    connect(&m_saveTimer, &QTimer::timeout, this, &NotificationManager::saveHistory);

    reloadPolicySnapshot();
    loadHistory();
}

NotificationManager::~NotificationManager()
{
    delete m_lifecycle;
    m_lifecycle = nullptr;
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
    return m_repository->notificationsModel();
}

QAbstractListModel* NotificationManager::bannerNotifications() const
{
    return m_repository->bannerModel();
}

int NotificationManager::count() const
{
    return m_repository->notificationsModel() ? m_repository->notificationsModel()->count() : 0;
}

int NotificationManager::unreadCount() const
{
    return m_repository->notificationsModel() ? m_repository->notificationsModel()->unreadCount() : 0;
}

int NotificationManager::unreadCountForAppId(const QString &appId) const
{
    return m_repository->notificationsModel() ? m_repository->notificationsModel()->unreadCountForAppId(appId) : 0;
}

int NotificationManager::unreadCountForGroup(const QString &groupId) const
{
    return m_repository->notificationsModel() ? m_repository->notificationsModel()->unreadCountForGroup(groupId) : 0;
}

QString NotificationManager::groupDisplayName(const QString &groupId) const
{
    return m_repository->notificationsModel() ? m_repository->notificationsModel()->displayNameForGroup(groupId) : QStringLiteral("Application");
}

void NotificationManager::setGroupExpanded(const QString &groupId, bool expanded)
{
    const QString wanted = groupId.trimmed();
    if (wanted.isEmpty() || !m_repository || !m_lifecycle) {
        return;
    }
    const QVector<NotificationEntry> all = m_repository->entries();
    const QString nextState = NotificationLifecycleEngine::stateName(
            expanded ? NotificationLifecycleEngine::State::Expanded
                     : NotificationLifecycleEngine::State::Collapsing);
    bool changedAny = false;
    for (const NotificationEntry &entry : all) {
        if (entry.groupId != wanted) {
            continue;
        }
        if (expanded) {
            m_lifecycle->onExpanded(entry.id);
        } else {
            m_lifecycle->onCollapsed(entry.id);
        }
        applyLifecycleToModels(m_repository, entry.id, nextState);
        changedAny = true;
    }
    if (changedAny) {
        m_saveTimer.start();
    }
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
    if (m_doNotDisturb && m_repository->bannerModel()) {
        m_repository->bannerModel()->clear();
    }
    reloadPolicySnapshot();
    emit doNotDisturbChanged();
}

QVariantMap NotificationManager::latestNotification() const
{
    return m_repository->latestNotification();
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
    if (m_repository->notificationsModel()) {
        m_repository->notificationsModel()->clear();
    }
    if (m_repository->bannerModel()) {
        m_repository->bannerModel()->clear();
    }
    m_bannerAttemptHistoryByApp.clear();
    if (m_lifecycle) {
        m_lifecycle->clear();
    }
    emitCountIfChanged(before);
    emitUnreadCountIfChanged(beforeUnread);
    m_saveTimer.start();
}

bool NotificationManager::closeById(uint id)
{
    const int before = count();
    const int beforeUnread = unreadCount();
    const bool ok = m_repository->notificationsModel() && m_repository->notificationsModel()->removeById(id);
    if (m_repository->bannerModel()) {
        m_repository->bannerModel()->removeById(id);
    }
    if (ok) {
        if (m_lifecycle) {
            m_lifecycle->onArchived(id);
            applyLifecycleToModels(m_repository, id,
                                   NotificationLifecycleEngine::stateName(NotificationLifecycleEngine::State::Archived));
        }
        emit NotificationClosed(id, 2u);
        emit NotificationRemoved(id);
        emitCountIfChanged(before);
        emitUnreadCountIfChanged(beforeUnread);
        m_saveTimer.start();
    }
    return ok;
}

void NotificationManager::toggleCenter()
{
    m_centerVisible = !m_centerVisible;
    if (m_centerVisible) {
        markAllRead();
        if (m_repository->bannerModel()) {
            m_repository->bannerModel()->clear();
        }
    }
    emit centerVisibleChanged();
}

void NotificationManager::setRuntimeContext(bool fullscreen, bool screenShare, bool focusMode)
{
    const bool changed = (m_runtimeContext.fullscreen != fullscreen)
            || (m_runtimeContext.screenShare != screenShare)
            || (m_runtimeContext.focusMode != focusMode);
    if (!changed) {
        return;
    }
    m_runtimeContext.fullscreen = fullscreen;
    m_runtimeContext.screenShare = screenShare;
    m_runtimeContext.focusMode = focusMode;
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
    entry.id = replacesId > 0 ? replacesId : m_repository->nextId();
    entry.appId = appIdFromNotifyPayload(appName, hints);
    const QString trimmedAppName = appName.trimmed();
    entry.appName = trimmedAppName.isEmpty()
            ? displayNameFromAppId(entry.appId)
            : trimmedAppName;
    entry.appIcon = appIcon.trimmed();
    entry.summary = summary;
    entry.body = body;
    entry.actions = actions;
    entry.urgency = urgencyFromHints(hints);
    entry.priority = (entry.urgency >= 2) ? QStringLiteral("high")
                                          : ((entry.urgency <= 0) ? QStringLiteral("low")
                                                                  : QStringLiteral("normal"));
    entry.groupId = NotificationGroupingEngine::groupIdFor(entry);
    entry.read = false;
    reloadPolicySnapshot();
    if (shouldDropNotification(entry)) {
        return entry.id;
    }
    entry.banner = shouldShowBanner(entry);
    entry.expireTimeoutMs = expireTimeout;
    entry.timestamp = QDateTime::currentDateTime();
    return upsertNotification(entry, !entry.banner);
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
    entry.id = m_repository->nextId();
    entry.appId = normalizedAppIdToken(appId);
    if (entry.appId.isEmpty()) {
        entry.appId = QStringLiteral("unknown.app");
    }
    entry.appName = displayNameFromAppId(entry.appId);
    entry.appIcon = icon.trimmed();
    entry.summary = title;
    entry.body = body;
    entry.actions = actions;
    reloadPolicySnapshot();
    entry.priority = normalizePriority(priority);
    entry.urgency = NotificationPolicyEngine::urgencyForPriority(entry.priority);
    entry.groupId = NotificationGroupingEngine::groupIdFor(entry);
    entry.read = false;
    if (shouldDropNotification(entry)) {
        return entry.id;
    }
    entry.banner = shouldShowBanner(entry);
    entry.expireTimeoutMs = -1;
    entry.timestamp = QDateTime::currentDateTime();
    return upsertNotification(entry, !entry.banner);
}

bool NotificationManager::Dismiss(uint id)
{
    return closeById(id);
}

bool NotificationManager::dismissBanner(uint id)
{
    const bool removed = m_repository->bannerModel() ? m_repository->bannerModel()->removeById(id) : false;
    if (removed && m_lifecycle) {
        m_lifecycle->onCollapsed(id);
        applyLifecycleToModels(m_repository, id,
                               NotificationLifecycleEngine::stateName(NotificationLifecycleEngine::State::Collapsing));
    }
    return removed;
}

QVariantList NotificationManager::GetAll() const
{
    QVariantList rows;
    const QVector<NotificationEntry> entries = m_repository ? m_repository->entries() : QVector<NotificationEntry>{};
    rows.reserve(entries.size());
    for (const NotificationEntry &entry : entries) {
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
    if (!m_repository->notificationsModel()) {
        return;
    }
    m_repository->notificationsModel()->markAllRead(true);
    emitUnreadCountIfChanged(beforeUnread);
}

bool NotificationManager::markRead(uint id, bool read)
{
    const int beforeUnread = unreadCount();
    if (!m_repository->notificationsModel()) {
        return false;
    }
    const bool changed = m_repository->notificationsModel()->markReadById(id, read);
    if (changed && read && m_repository->bannerModel()) {
        m_repository->bannerModel()->removeById(id);
        if (m_lifecycle) {
            m_lifecycle->onArchived(id);
            applyLifecycleToModels(m_repository, id,
                                   NotificationLifecycleEngine::stateName(NotificationLifecycleEngine::State::Archived));
        }
    }
    emitUnreadCountIfChanged(beforeUnread);
    if (changed) {
        m_saveTimer.start();
    }
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
    return NotificationPolicyEngine::normalizePriority(priority);
}

void NotificationManager::reloadPolicySnapshot()
{
    QSettings settings(QStringLiteral("SLM"), QStringLiteral("slm-settings"));
    m_notificationsEnabled = readBoolPolicy(settings, QStringLiteral("settings/notifications/enabled"), true);
    m_policyAllowCriticalAlerts = readBoolPolicy(settings, QStringLiteral("settings/notifications/allow_critical_alerts"), true);
    m_policySilenceFullscreen = readBoolPolicy(settings, QStringLiteral("settings/notifications/silence_fullscreen"), true);
    m_policySilenceScreenShare = readBoolPolicy(settings, QStringLiteral("settings/notifications/silence_screen_share"), true);
    m_policyFocusModeIntegration = readBoolPolicy(settings, QStringLiteral("settings/notifications/focus_mode_integration"), true);
    m_policyDeliverQuietly = readBoolPolicy(settings, QStringLiteral("settings/notifications/deliver_quietly"), false);
}

bool NotificationManager::shouldDropNotification(const NotificationEntry &entry) const
{
    NotificationPolicyEngine::Snapshot snapshot;
    snapshot.notificationsEnabled = m_notificationsEnabled;
    snapshot.allowCriticalAlerts = m_policyAllowCriticalAlerts;
    snapshot.silenceFullscreen = m_policySilenceFullscreen;
    snapshot.silenceScreenShare = m_policySilenceScreenShare;
    snapshot.focusModeIntegration = m_policyFocusModeIntegration;
    snapshot.deliverQuietly = m_policyDeliverQuietly;
    snapshot.doNotDisturb = m_doNotDisturb;
    return NotificationPolicyEngine::shouldDropNotification(entry, snapshot);
}

bool NotificationManager::shouldShowBanner(const NotificationEntry &entry) const
{
    NotificationPolicyEngine::Snapshot snapshot;
    snapshot.notificationsEnabled = m_notificationsEnabled;
    snapshot.allowCriticalAlerts = m_policyAllowCriticalAlerts;
    snapshot.silenceFullscreen = m_policySilenceFullscreen;
    snapshot.silenceScreenShare = m_policySilenceScreenShare;
    snapshot.focusModeIntegration = m_policyFocusModeIntegration;
    snapshot.deliverQuietly = m_policyDeliverQuietly;
    snapshot.doNotDisturb = m_doNotDisturb;
    return NotificationPolicyEngine::shouldShowBanner(entry, snapshot, m_runtimeContext);
}

uint NotificationManager::upsertNotification(const NotificationEntry &entry, bool suppressBanner)
{
    NotificationEntry effectiveEntry = entry;
    if (m_lifecycle) {
        m_lifecycle->onQueued(effectiveEntry.id);
        effectiveEntry.lifecycleState = NotificationLifecycleEngine::stateName(NotificationLifecycleEngine::State::Queued);
    }
    if (effectiveEntry.banner && shouldSuppressBannerForSpam(effectiveEntry)) {
        effectiveEntry.banner = false;
        suppressBanner = true;
    }

    const int before = count();
    const int beforeUnread = unreadCount();
    if (m_repository->notificationsModel()) {
        m_repository->notificationsModel()->upsert(effectiveEntry);
    }
    if (m_repository->bannerModel()) {
        if (effectiveEntry.banner) {
            if (m_lifecycle) {
                const NotificationLifecycleEngine::State state = effectiveEntry.groupId.isEmpty()
                        ? NotificationLifecycleEngine::State::Visible
                        : NotificationLifecycleEngine::State::Grouped;
                m_lifecycle->onDisplayed(effectiveEntry.id, !effectiveEntry.groupId.isEmpty());
                effectiveEntry.lifecycleState = NotificationLifecycleEngine::stateName(state);
            }
            m_repository->bannerModel()->upsert(effectiveEntry);
        } else {
            if (m_lifecycle) {
                m_lifecycle->onArchived(effectiveEntry.id);
                effectiveEntry.lifecycleState = NotificationLifecycleEngine::stateName(NotificationLifecycleEngine::State::Archived);
            }
            m_repository->bannerModel()->removeById(effectiveEntry.id);
        }
    }
    applyLifecycleToModels(m_repository, effectiveEntry.id, effectiveEntry.lifecycleState);
    emitCountIfChanged(before);
    emitUnreadCountIfChanged(beforeUnread);
    emit NotificationAdded(effectiveEntry.id);

    if (!suppressBanner) {
        updateLatestNotification(effectiveEntry);
    }

    // Schedule a debounced history save so disk writes are batched during bursts.
    m_saveTimer.start();

    return effectiveEntry.id;
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
        {QStringLiteral("banner"), entry.banner},
        {QStringLiteral("lifecycle_state"), entry.lifecycleState}
    };
}

void NotificationManager::updateLatestNotification(const NotificationEntry &entry)
{
    if (m_repository) {
        m_repository->updateLatestNotification(entry);
    }
    emit latestNotificationChanged();
}

bool NotificationManager::shouldSuppressBannerForSpam(const NotificationEntry &entry)
{
    if (!m_repository->bannerModel()) {
        return false;
    }

    const QString appKey = entry.appId.trimmed().isEmpty()
            ? QStringLiteral("unknown.app")
            : entry.appId.trimmed();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    // Sliding-window limiter: suppress banner bursts for the same app.
    if (m_bannerRateLimitWindowMs > 0 && m_maxBannerAttemptsPerWindow > 0) {
        QVector<qint64> &history = m_bannerAttemptHistoryByApp[appKey];
        const qint64 minAllowed = nowMs - static_cast<qint64>(m_bannerRateLimitWindowMs);
        while (!history.isEmpty() && history.front() < minAllowed) {
            history.removeFirst();
        }
        history.push_back(nowMs);
        if (history.size() > m_maxBannerAttemptsPerWindow) {
            return true;
        }
    }

    // Active banner cap: keep at most N banners visible per app.
    if (m_maxActiveBannersPerApp > 0
        && m_repository->bannerModel()->countForAppId(entry.appId, entry.id) >= m_maxActiveBannersPerApp) {
        return true;
    }

    return false;
}

void NotificationManager::saveHistory()
{
    if (m_historyFilePath.isEmpty() || !m_repository) {
        return;
    }
    QVector<NotificationEntry> entries = m_repository->entries();
    if (entries.size() > kMaxHistoryEntries) {
        entries.resize(kMaxHistoryEntries);
    }
    if (!NotificationStore::saveHistory(m_historyFilePath, m_repository->currentNextId(), entries)) {
        qWarning() << "NotificationManager: failed to save history to" << m_historyFilePath
                   << "error: store-save-failed";
    }
}

void NotificationManager::loadHistory()
{
    if (m_historyFilePath.isEmpty()) {
        return;
    }

    const NotificationStore::LoadedState loaded = NotificationStore::loadHistory(m_historyFilePath,
                                                                                  kMaxHistoryEntries);
    if (!loaded.ok) {
        return; // Normal on first run — no history yet.
    }
    m_repository->ensureNextIdAtLeast(loaded.nextId);

    int restoredCount = 0;
    for (NotificationEntry entry : loaded.entries) {
        // Restored notifications never show as banners — the user already saw them.
        entry.banner   = false;
        entry.urgency  = NotificationPolicyEngine::urgencyForPriority(entry.priority);
        entry.lifecycleState = NotificationLifecycleEngine::stateName(NotificationLifecycleEngine::State::Archived);
        if (entry.id > 0 && m_repository->notificationsModel()) {
            m_repository->notificationsModel()->upsert(entry);
            m_repository->ensureNextIdAtLeast(entry.id + 1);
            ++restoredCount;
        }
    }

    if (restoredCount > 0) {
        emit countChanged();
        emit unreadCountChanged();
        qInfo() << "NotificationManager: restored" << restoredCount << "notifications from history";
    }
}
