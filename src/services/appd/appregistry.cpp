#include "appregistry.h"

#include <QDebug>

namespace Slm::Appd {

AppRegistry::AppRegistry(QObject *parent)
    : QObject(parent)
{
}

bool AppRegistry::contains(const QString &appId) const
{
    return m_entries.contains(appId);
}

AppEntry *AppRegistry::find(const QString &appId)
{
    auto it = m_entries.find(appId);
    return it != m_entries.end() ? &it.value() : nullptr;
}

const AppEntry *AppRegistry::find(const QString &appId) const
{
    auto it = m_entries.constFind(appId);
    return it != m_entries.constEnd() ? &it.value() : nullptr;
}

AppEntry *AppRegistry::findByPid(qint64 pid)
{
    for (auto &entry : m_entries) {
        if (entry.pids.contains(pid)) {
            return &entry;
        }
    }
    return nullptr;
}

AppEntry *AppRegistry::findByWindowId(const QString &windowId)
{
    for (auto &entry : m_entries) {
        for (const auto &w : std::as_const(entry.windows)) {
            if (w.windowId == windowId) {
                return &entry;
            }
        }
    }
    return nullptr;
}

QList<AppEntry *> AppRegistry::all()
{
    QList<AppEntry *> result;
    result.reserve(m_entries.size());
    for (auto &entry : m_entries) {
        result.append(&entry);
    }
    return result;
}

QList<const AppEntry *> AppRegistry::all() const
{
    QList<const AppEntry *> result;
    result.reserve(m_entries.size());
    for (const auto &entry : m_entries) {
        result.append(&entry);
    }
    return result;
}

AppEntry *AppRegistry::upsert(AppEntry entry)
{
    const QString appId = entry.appId;
    const bool existed  = m_entries.contains(appId);
    m_entries.insert(appId, std::move(entry));
    if (!existed) {
        emit entryAdded(appId);
    } else {
        emit entryUpdated(appId);
    }
    return &m_entries[appId];
}

void AppRegistry::remove(const QString &appId)
{
    if (m_entries.remove(appId) > 0) {
        emit entryRemoved(appId);
    }
}

bool AppRegistry::attachPid(const QString &appId, qint64 pid)
{
    auto *entry = find(appId);
    if (!entry) {
        return false;
    }
    if (!entry->pids.contains(pid)) {
        entry->pids.append(pid);
    }
    return true;
}

bool AppRegistry::detachPid(qint64 pid)
{
    for (auto &entry : m_entries) {
        const int removed = entry.pids.removeAll(pid);
        if (removed > 0) {
            return !entry.pids.isEmpty();
        }
    }
    return false;
}

bool AppRegistry::attachWindow(const QString &appId, const WindowInfo &window)
{
    auto *entry = find(appId);
    if (!entry) {
        return false;
    }
    // Replace if already present.
    for (auto &w : entry->windows) {
        if (w.windowId == window.windowId) {
            w = window;
            emit entryWindowsChanged(appId);
            return true;
        }
    }
    entry->windows.append(window);
    emit entryWindowsChanged(appId);
    return true;
}

bool AppRegistry::detachWindow(const QString &appId, const QString &windowId)
{
    auto *entry = find(appId);
    if (!entry) {
        return false;
    }
    const int sizeBefore = entry->windows.size();
    entry->windows.erase(
        std::remove_if(entry->windows.begin(), entry->windows.end(),
                       [&](const WindowInfo &w) { return w.windowId == windowId; }),
        entry->windows.end());
    const int removed = sizeBefore - entry->windows.size();
    if (removed > 0) {
        emit entryWindowsChanged(appId);
    }
    return removed > 0;
}

bool AppRegistry::updateWindow(const QString &appId, const WindowInfo &window)
{
    auto *entry = find(appId);
    if (!entry) {
        return false;
    }
    for (auto &w : entry->windows) {
        if (w.windowId == window.windowId) {
            w = window;
            emit entryWindowsChanged(appId);
            return true;
        }
    }
    return false;
}

} // namespace Slm::Appd
