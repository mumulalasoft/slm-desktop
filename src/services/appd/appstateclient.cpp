#include "appstateclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QFileInfo>

namespace Slm::Appd {

namespace {
constexpr auto kService = "org.desktop.Apps";
constexpr auto kPath    = "/org/desktop/Apps";
constexpr auto kIface   = "org.desktop.Apps";

// Active states that count as "running" for dock indicators.
bool isRunningState(const QString &state)
{
    return state == QStringLiteral("running")
           || state == QStringLiteral("idle")
           || state == QStringLiteral("background")
           || state == QStringLiteral("unresponsive");
}

bool isLaunchingState(const QString &state)
{
    return state == QStringLiteral("launching")
           || state == QStringLiteral("created");
}

QString desktopBase(const QString &path)
{
    return QFileInfo(path).completeBaseName().toLower();
}
} // namespace

AppStateClient::AppStateClient(QObject *parent)
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

AppStateClient::~AppStateClient() = default;

bool AppStateClient::serviceAvailable() const
{
    return m_serviceAvailable;
}

QStringList AppStateClient::runningAppIds() const
{
    QStringList result;
    for (auto it = m_appStates.constBegin(); it != m_appStates.constEnd(); ++it) {
        if (isRunningState(it.value())) {
            result.append(it.key());
        }
    }
    return result;
}

QStringList AppStateClient::launchingAppIds() const
{
    QStringList result;
    for (auto it = m_appStates.constBegin(); it != m_appStates.constEnd(); ++it) {
        if (isLaunchingState(it.value())) {
            result.append(it.key());
        }
    }
    return result;
}

QString AppStateClient::focusedAppId() const
{
    return m_focusedAppId;
}

int AppStateClient::rev() const
{
    return m_rev;
}

bool AppStateClient::isRunning(const QString &appId) const
{
    return isRunningState(m_appStates.value(appId));
}

bool AppStateClient::isLaunching(const QString &appId) const
{
    return isLaunchingState(m_appStates.value(appId));
}

bool AppStateClient::isFocused(const QString &appId) const
{
    return m_focusedAppId == appId;
}

QString AppStateClient::getAppState(const QString &appId) const
{
    return m_appStates.value(appId, QStringLiteral("unknown"));
}

QVariantMap AppStateClient::getApp(const QString &appId) const
{
    if (!m_iface || !m_iface->isValid()) {
        return {};
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetApp"), appId);
    if (!reply.isValid()) {
        return {};
    }
    return reply.value();
}

QVariantList AppStateClient::listRunningApps() const
{
    if (!m_iface || !m_iface->isValid()) {
        return {};
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListRunningApps"));
    if (!reply.isValid()) {
        return {};
    }
    return reply.value();
}

bool AppStateClient::matchesRunning(const QString &desktopFile,
                                     const QString &executable,
                                     const QString &name) const
{
    const QString dBase = desktopBase(desktopFile);
    const QString exeLower = executable.toLower();
    const QString nameLower = name.toLower();

    for (auto it = m_appStates.constBegin(); it != m_appStates.constEnd(); ++it) {
        if (!isRunningState(it.value()) && !isLaunchingState(it.value())) {
            continue;
        }
        const QString id = it.key().toLower();
        if (!dBase.isEmpty() && (id.contains(dBase) || dBase.contains(id))) {
            return true;
        }
        if (!exeLower.isEmpty() && id.contains(exeLower)) {
            return true;
        }
        if (!nameLower.isEmpty() && id.contains(nameLower)) {
            return true;
        }
    }
    return false;
}

// ── Private slots ─────────────────────────────────────────────────────────────

void AppStateClient::onAppAdded(const QString &appId)
{
    // New app: fetch its current state.
    if (!m_iface || !m_iface->isValid()) {
        return;
    }
    QDBusReply<QString> reply = m_iface->call(QStringLiteral("GetAppState"), appId);
    const QString state = reply.isValid() ? reply.value() : QStringLiteral("launching");
    m_appStates.insert(appId, state);
    m_rev++;
    emit runningAppsChanged();
}

void AppStateClient::onAppRemoved(const QString &appId)
{
    m_appStates.remove(appId);
    m_focusedIds.remove(appId);
    if (m_focusedAppId == appId) {
        m_focusedAppId.clear();
        emit focusedAppIdChanged();
    }
    m_rev++;
    emit runningAppsChanged();
}

void AppStateClient::onAppStateChanged(const QString &appId, const QString &state)
{
    m_appStates.insert(appId, state);
    m_rev++;
    emit runningAppsChanged();
}

void AppStateClient::onAppFocusChanged(const QString &appId, bool focused)
{
    if (focused) {
        m_focusedIds.insert(appId);
        if (m_focusedAppId != appId) {
            m_focusedAppId = appId;
            emit focusedAppIdChanged();
        }
    } else {
        m_focusedIds.remove(appId);
        if (m_focusedAppId == appId) {
            m_focusedAppId = m_focusedIds.isEmpty()
                             ? QString()
                             : *m_focusedIds.constBegin();
            emit focusedAppIdChanged();
        }
    }
    m_rev++;
    emit runningAppsChanged();
}

void AppStateClient::onNameOwnerChanged(const QString &name,
                                         const QString &oldOwner,
                                         const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (name != QString::fromLatin1(kService)) {
        return;
    }
    setServiceAvailable(!newOwner.isEmpty());
    if (!newOwner.isEmpty()) {
        refresh();
    } else {
        m_appStates.clear();
        m_focusedIds.clear();
        m_focusedAppId.clear();
        m_rev++;
        emit runningAppsChanged();
        emit focusedAppIdChanged();
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void AppStateClient::bindSignals()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kIface), QStringLiteral("AppAdded"),
                this, SLOT(onAppAdded(QString)));
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kIface), QStringLiteral("AppRemoved"),
                this, SLOT(onAppRemoved(QString)));
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kIface), QStringLiteral("AppStateChanged"),
                this, SLOT(onAppStateChanged(QString, QString)));
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kIface), QStringLiteral("AppFocusChanged"),
                this, SLOT(onAppFocusChanged(QString, bool)));

    m_watcher = new QDBusServiceWatcher(QString::fromLatin1(kService),
                                        bus,
                                        QDBusServiceWatcher::WatchForOwnerChange,
                                        this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &AppStateClient::onNameOwnerChanged);
}

void AppStateClient::setServiceAvailable(bool available)
{
    if (m_serviceAvailable == available) {
        return;
    }
    m_serviceAvailable = available;
    emit serviceAvailableChanged();
}

void AppStateClient::refresh()
{
    QDBusConnectionInterface *busIface = QDBusConnection::sessionBus().interface();
    const bool available = busIface
            ? busIface->isServiceRegistered(QString::fromLatin1(kService)).value()
            : false;
    setServiceAvailable(available);

    m_appStates.clear();
    if (!m_iface || !m_iface->isValid() || !available) {
        m_rev++;
        emit runningAppsChanged();
        return;
    }

    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListRunningApps"));
    if (!reply.isValid()) {
        m_rev++;
        emit runningAppsChanged();
        return;
    }

    for (const QVariant &v : reply.value()) {
        const QVariantMap entry = v.toMap();
        const QString appId = entry.value(QStringLiteral("appId")).toString();
        const QString state = entry.value(QStringLiteral("state")).toString();
        if (!appId.isEmpty()) {
            m_appStates.insert(appId, state);
        }
    }

    m_rev++;
    emit runningAppsChanged();
}

} // namespace Slm::Appd
