#include "lifecycleengine.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>

namespace Slm::Appd {

namespace {
qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

qint64 parentPidOf(qint64 pid)
{
    const QString statusPath = QStringLiteral("/proc/%1/status").arg(pid);
    QFile f(statusPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (line.startsWith("PPid:")) {
            return line.mid(5).trimmed().toLongLong();
        }
    }
    return -1;
}

bool pidAlive(qint64 pid)
{
    return QFile::exists(QStringLiteral("/proc/%1").arg(pid));
}
} // namespace

LifecycleEngine::LifecycleEngine(AppRegistry *registry,
                                  ClassificationEngine *classifier,
                                  QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_classifier(classifier)
{
    Q_ASSERT(registry);
    Q_ASSERT(classifier);
}

void LifecycleEngine::onLaunch(const LaunchRequest &req)
{
    const QString &appId = req.appId;
    if (appId.isEmpty()) {
        return;
    }

    AppEntry *existing = m_registry->find(appId);
    if (existing) {
        // App already tracked — attach new PID if provided.
        if (req.pid > 0) {
            m_registry->attachPid(appId, req.pid);
            m_pidToAppId.insert(req.pid, appId);
        }
        if (existing->state == AppState::Terminated
                || existing->state == AppState::Crashed) {
            transitionState(existing, AppState::Launching);
        }
        return;
    }

    AppEntry entry;
    entry.appId          = appId;
    entry.state          = AppState::Launching;
    entry.launchTs       = req.timestampMs > 0 ? req.timestampMs : nowMs();
    entry.executableHint = req.executableHint;
    entry.desktopFile    = req.desktopFile;
    if (req.pid > 0) {
        entry.pids.append(req.pid);
        m_pidToAppId.insert(req.pid, appId);
    }

    classifyAndExpose(&entry, req.desktopFile);
    m_registry->upsert(std::move(entry));

    emit appAdded(appId);
    emit appStateChanged(appId, appStateString(AppState::Launching));
    qDebug() << "[appd-lifecycle] onLaunch" << appId << "pid=" << req.pid;
}

void LifecycleEngine::onWindowCreated(const QString &appId,
                                      qint64 pid,
                                      const WindowInfo &window,
                                      const QStringList &desktopCategories)
{
    if (appId.isEmpty()) {
        return;
    }

    AppEntry *entry = m_registry->find(appId);
    if (!entry) {
        // Window arrived before launch notification — create entry now.
        AppEntry newEntry;
        newEntry.appId    = appId;
        newEntry.state    = AppState::Launching;
        newEntry.launchTs = nowMs();
        if (pid > 0) {
            newEntry.pids.append(pid);
            m_pidToAppId.insert(pid, appId);
        }
        classifyAndExpose(&newEntry, {}, desktopCategories);
        entry = m_registry->upsert(std::move(newEntry));
        emit appAdded(appId);
    } else {
        if (pid > 0 && !entry->pids.contains(pid)) {
            m_registry->attachPid(appId, pid);
            m_pidToAppId.insert(pid, appId);
        }
    }

    m_registry->attachWindow(appId, window);
    entry->isVisible = true;

    if (entry->state == AppState::Launching || entry->state == AppState::Created) {
        transitionState(entry, AppState::Running);
    }

    emit appWindowsChanged(appId);
}

void LifecycleEngine::onWindowClosed(const QString &windowId)
{
    AppEntry *entry = m_registry->findByWindowId(windowId);
    if (!entry) {
        return;
    }
    const QString appId = entry->appId;
    m_registry->detachWindow(appId, windowId);

    entry = m_registry->find(appId);
    if (!entry) {
        return;
    }

    emit appWindowsChanged(appId);

    if (entry->windows.isEmpty()) {
        entry->isVisible = false;
        // If all PIDs are still alive, move to IDLE.
        bool anyAlive = false;
        for (qint64 pid : std::as_const(entry->pids)) {
            if (pidAlive(pid)) {
                anyAlive = true;
                break;
            }
        }
        if (anyAlive) {
            transitionState(entry, AppState::Idle);
        }
    }
}

void LifecycleEngine::onFocusChanged(const QString &windowId, bool focused)
{
    AppEntry *entry = m_registry->findByWindowId(windowId);
    if (!entry) {
        return;
    }

    // Update window focus state.
    for (auto &w : entry->windows) {
        if (w.windowId == windowId) {
            w.focused = focused;
        } else if (focused) {
            w.focused = false; // only one window focused at a time per app
        }
    }

    entry->isFocused    = focused;
    entry->lastActiveTs = focused ? nowMs() : entry->lastActiveTs;

    emit appFocusChanged(entry->appId, focused);
    emit appUpdated(entry->appId);
}

void LifecycleEngine::onProcessExited(qint64 pid, int exitCode)
{
    AppEntry *entry = m_registry->findByPid(pid);
    if (!entry) {
        m_pidToAppId.remove(pid);
        return;
    }

    const QString appId = entry->appId;
    m_registry->detachPid(pid);
    m_pidToAppId.remove(pid);

    entry = m_registry->find(appId);
    if (!entry) {
        return;
    }

    if (entry->pids.isEmpty()) {
        // Last PID exited.
        const AppState next = (exitCode != 0) ? AppState::Crashed : AppState::Terminated;
        transitionState(entry, next);
        emit appRemoved(appId);
        m_registry->remove(appId);
    } else {
        emit appUpdated(appId);
    }
}

void LifecycleEngine::onUnresponsive(const QString &appId)
{
    AppEntry *entry = m_registry->find(appId);
    if (!entry) {
        return;
    }
    transitionState(entry, AppState::Unresponsive);
}

void LifecycleEngine::setLowPowerMode(bool lowPower)
{
    if (m_lowPower == lowPower) {
        return;
    }
    m_lowPower = lowPower;

    if (!lowPower) {
        return;
    }

    // On low power: move background apps to BACKGROUND (UI cue, no actual suspend).
    for (auto *entry : m_registry->all()) {
        if (entry->state == AppState::Idle) {
            transitionState(entry, AppState::Background);
        }
    }
}

void LifecycleEngine::pollProcessStates()
{
    const auto entries = m_registry->all();
    for (auto *entry : entries) {
        if (entry->state == AppState::Terminated
                || entry->state == AppState::Crashed) {
            continue;
        }

        QList<qint64> deadPids;
        for (qint64 pid : std::as_const(entry->pids)) {
            if (!pidAlive(pid)) {
                deadPids.append(pid);
            }
        }

        for (qint64 dead : std::as_const(deadPids)) {
            onProcessExited(dead, 0);
        }
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void LifecycleEngine::transitionState(AppEntry *entry, AppState next)
{
    if (!entry || entry->state == next) {
        return;
    }
    entry->state = next;
    emit appStateChanged(entry->appId, appStateString(next));
    emit appUpdated(entry->appId);
    qDebug() << "[appd-lifecycle] state" << entry->appId << "->" << appStateString(next);
}

void LifecycleEngine::classifyAndExpose(AppEntry *entry,
                                         const QString &desktopFile,
                                         const QStringList &desktopCategories)
{
    const bool hasWindow = !entry->windows.isEmpty();
    qint64 parentPid = -1;
    if (!entry->pids.isEmpty()) {
        parentPid = parentPidOf(entry->pids.first());
    }

    entry->category = m_classifier->classify(
        entry->appId,
        desktopFile.isEmpty() ? entry->desktopFile : desktopFile,
        entry->executableHint,
        desktopCategories,
        hasWindow,
        parentPid);

    entry->uiExposure = UIExposurePolicy::compute(entry->category);
}

} // namespace Slm::Appd
