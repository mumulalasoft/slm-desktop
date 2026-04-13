#include "appservice.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace Slm::Appd {

namespace {
constexpr const char kService[] = "org.desktop.Apps";
constexpr const char kPath[]    = "/org/desktop/Apps";
// PID poll interval: every 5 s for detecting orphaned processes.
constexpr int kPollIntervalMs = 5000;
} // namespace

AppService::AppService(QObject *parent)
    : QObject(parent)
    , m_registry(this)
    , m_lifecycle(&m_registry, &m_classifier, this)
{
    connectRegistry();

    m_pollTimer.setInterval(kPollIntervalMs);
    m_pollTimer.setSingleShot(false);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() {
        m_lifecycle.pollProcessStates();
    });
}

AppService::~AppService()
{
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool AppService::start(QString *error)
{
    registerDbusService();
    if (!m_serviceRegistered) {
        if (error) {
            *error = QStringLiteral("failed to register D-Bus service %1").arg(
                QString::fromLatin1(kService));
        }
        return false;
    }
    m_pollTimer.start();
    return true;
}

bool AppService::serviceRegistered() const
{
    return m_serviceRegistered;
}

// ── D-Bus Methods ─────────────────────────────────────────────────────────────

QVariantList AppService::ListRunningApps() const
{
    QVariantList result;
    for (const auto *entry : m_registry.all()) {
        if (entry->state == AppState::Terminated || entry->state == AppState::Crashed) {
            continue;
        }
        result.append(entry->toVariantMap());
    }
    return result;
}

QVariantMap AppService::GetApp(const QString &appId) const
{
    const auto *entry = m_registry.find(appId);
    if (!entry) {
        return {};
    }
    return entry->toVariantMap();
}

QVariantList AppService::GetAppWindows(const QString &appId) const
{
    const auto *entry = m_registry.find(appId);
    if (!entry) {
        return {};
    }
    QVariantList result;
    for (const auto &w : entry->windows) {
        result.append(w.toVariantMap());
    }
    return result;
}

QString AppService::GetAppState(const QString &appId) const
{
    const auto *entry = m_registry.find(appId);
    if (!entry) {
        return QStringLiteral("unknown");
    }
    return appStateString(entry->state);
}

void AppService::LaunchApp(const QString &appId)
{
    // Resolve the .desktop file for this appId and launch.
    const auto *entry = m_registry.find(appId);
    if (entry && !entry->desktopFile.isEmpty()) {
        QProcess::startDetached(QStringLiteral("gtk-launch"),
                                { QFileInfo(entry->desktopFile).baseName() });
        return;
    }
    // Fallback: launch by appId as desktop entry basename.
    QProcess::startDetached(QStringLiteral("gtk-launch"), { appId });
}

void AppService::ActivateApp(const QString &appId)
{
    const auto *entry = m_registry.find(appId);
    if (!entry || entry->windows.isEmpty()) {
        LaunchApp(appId);
        return;
    }
    // Use KWin or wmctrl to raise the first window.
    const QString windowId = entry->windows.first().windowId;
    if (!windowId.isEmpty()) {
        QProcess::startDetached(QStringLiteral("wmctrl"),
                                { QStringLiteral("-ia"), windowId });
    }
}

void AppService::MinimizeApp(const QString &appId)
{
    const auto *entry = m_registry.find(appId);
    if (!entry) {
        return;
    }
    for (const auto &w : entry->windows) {
        if (!w.windowId.isEmpty()) {
            QProcess::startDetached(QStringLiteral("wmctrl"),
                                    { QStringLiteral("-ir"), w.windowId,
                                      QStringLiteral("-b"), QStringLiteral("add,hidden") });
        }
    }
}

void AppService::CloseApp(const QString &appId)
{
    const auto *entry = m_registry.find(appId);
    if (!entry) {
        return;
    }
    for (const auto &w : entry->windows) {
        if (!w.windowId.isEmpty()) {
            QProcess::startDetached(QStringLiteral("wmctrl"),
                                    { QStringLiteral("-ic"), w.windowId });
        }
    }
}

void AppService::RestartApp(const QString &appId)
{
    CloseApp(appId);
    // Brief delay to allow window close before re-launch.
    QTimer::singleShot(800, this, [this, appId]() {
        LaunchApp(appId);
    });
}

void AppService::NoteLaunch(const QString &appId,
                             const QString &desktopFile,
                             const QString &executable,
                             qint64 pid)
{
    LaunchRequest req;
    req.appId           = appId;
    req.desktopFile     = desktopFile;
    req.executableHint  = executable;
    req.pid             = pid;
    req.timestampMs     = QDateTime::currentMSecsSinceEpoch();
    m_lifecycle.onLaunch(req);
}

void AppService::NoteWindowCreated(const QString &appId,
                                   qint64 pid,
                                   const QString &windowId,
                                   const QString &title,
                                   int workspace)
{
    WindowInfo w;
    w.windowId  = windowId;
    w.title     = title;
    w.workspace = workspace;
    m_lifecycle.onWindowCreated(appId, pid, w);
}

void AppService::NoteWindowClosed(const QString &windowId)
{
    m_lifecycle.onWindowClosed(windowId);
}

void AppService::NoteFocusChanged(const QString &windowId, bool focused)
{
    m_lifecycle.onFocusChanged(windowId, focused);
}

void AppService::NoteProcessExited(qint64 pid, int exitCode)
{
    m_lifecycle.onProcessExited(pid, exitCode);
}

// ── Private ───────────────────────────────────────────────────────────────────

void AppService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        qWarning() << "[appd] D-Bus session bus not connected";
        return;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        qWarning() << "[appd] service already registered by another instance";
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        qWarning() << "[appd] failed to register service:" << bus.lastError().message();
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerObject(QString::fromLatin1(kPath), this,
                            QDBusConnection::ExportAllSlots
                                | QDBusConnection::ExportAllSignals
                                | QDBusConnection::ExportAllProperties)) {
        qWarning() << "[appd] failed to register object:" << bus.lastError().message();
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
    qInfo() << "[appd] D-Bus service registered:" << kService;
}

void AppService::connectRegistry()
{
    connect(&m_lifecycle, &LifecycleEngine::appAdded,
            this, [this](const QString &appId) {
        emit AppAdded(appId);
    });
    connect(&m_lifecycle, &LifecycleEngine::appRemoved,
            this, [this](const QString &appId) {
        emit AppRemoved(appId);
    });
    connect(&m_lifecycle, &LifecycleEngine::appStateChanged,
            this, [this](const QString &appId, const QString &state) {
        emit AppStateChanged(appId, state);
    });
    connect(&m_lifecycle, &LifecycleEngine::appWindowsChanged,
            this, [this](const QString &appId) {
        emit AppWindowsChanged(appId);
    });
    connect(&m_lifecycle, &LifecycleEngine::appFocusChanged,
            this, [this](const QString &appId, bool focused) {
        emit AppFocusChanged(appId, focused);
    });
    connect(&m_lifecycle, &LifecycleEngine::appUpdated,
            this, [this](const QString &appId) {
        emit AppUpdated(appId);
    });
}

} // namespace Slm::Appd
