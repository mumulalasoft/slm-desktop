#include "slmactiondiscovery.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QDebug>
#include <QtGlobal>
#include <algorithm>

namespace Slm::Actions {

ActionDiscoveryScanner::ActionDiscoveryScanner(QObject *parent)
    : QObject(parent)
{
    const QString home = qEnvironmentVariable("HOME");
    m_scanDirectories = {
        QStringLiteral("/usr/share/applications"),
        home + QStringLiteral("/.local/share/applications"),
    };

    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.setInterval(250);
    connect(&m_rescanTimer, &QTimer::timeout, this, [this]() {
        scanNow();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        scheduleRescan();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        scheduleRescan();
    });
}

void ActionDiscoveryScanner::setScanDirectories(const QStringList &directories)
{
    m_scanDirectories.clear();
    for (const QString &path : directories) {
        const QString normalized = QDir::cleanPath(path.trimmed());
        if (!normalized.isEmpty()) {
            m_scanDirectories.push_back(normalized);
        }
    }
    if (m_autoWatchEnabled) {
        rebuildWatchList(discoverDesktopFiles());
    }
}

QStringList ActionDiscoveryScanner::scanDirectories() const
{
    return m_scanDirectories;
}

QStringList ActionDiscoveryScanner::discoverDesktopFiles() const
{
    QSet<QString> out;
    for (const QString &root : m_scanDirectories) {
        QDirIterator it(root,
                        QStringList{QStringLiteral("*.desktop")},
                        QDir::Files | QDir::Readable,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            out.insert(it.next());
        }
    }
    QStringList files = out.values();
    files.sort();
    return files;
}

void ActionDiscoveryScanner::setAutoWatchEnabled(bool enabled)
{
    if (m_autoWatchEnabled == enabled) {
        return;
    }
    m_autoWatchEnabled = enabled;
    if (!m_autoWatchEnabled) {
        if (!m_watcher.files().isEmpty()) {
            m_watcher.removePaths(m_watcher.files());
        }
        if (!m_watcher.directories().isEmpty()) {
            m_watcher.removePaths(m_watcher.directories());
        }
        return;
    }
    rebuildWatchList(discoverDesktopFiles());
}

bool ActionDiscoveryScanner::autoWatchEnabled() const
{
    return m_autoWatchEnabled;
}

void ActionDiscoveryScanner::scheduleRescan()
{
    if (!m_autoWatchEnabled) {
        return;
    }
    m_rescanTimer.start();
}

void ActionDiscoveryScanner::rebuildWatchList(const QStringList &files)
{
    if (!m_autoWatchEnabled) {
        return;
    }
    if (!m_watcher.files().isEmpty()) {
        m_watcher.removePaths(m_watcher.files());
    }
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }

    QStringList existingDirs;
    existingDirs.reserve(m_scanDirectories.size());
    for (const QString &dir : m_scanDirectories) {
        if (QFileInfo::exists(dir) && QFileInfo(dir).isDir()) {
            existingDirs.push_back(dir);
        }
    }
    if (!existingDirs.isEmpty()) {
        m_watcher.addPaths(existingDirs);
    }
    if (!files.isEmpty()) {
        m_watcher.addPaths(files);
    }
}

QVector<FileAction> ActionDiscoveryScanner::scanNow()
{
    emit scanStarted();
    QVector<FileAction> merged;
    const QStringList files = discoverDesktopFiles();
    if (m_autoWatchEnabled) {
        rebuildWatchList(files);
    }
    QSet<QString> present;
    present.reserve(files.size());

    for (const QString &file : files) {
        present.insert(file);
        const QFileInfo fi(file);
        const qint64 mtimeMs = fi.lastModified().toMSecsSinceEpoch();
        const qint64 size = fi.size();
        const auto it = m_cache.constFind(file);
        if (it != m_cache.constEnd() && it->mtimeMs == mtimeMs && it->size == size) {
            merged += it->actions;
            continue;
        }
        const DesktopParseResult parsed = m_parser.parseFile(file);
        CacheEntry entry;
        entry.mtimeMs = mtimeMs;
        entry.size = size;
        if (parsed.ok) {
            entry.actions = parsed.actions;
            merged += entry.actions;
        }
        m_cache.insert(file, entry);
    }

    QList<QString> removeKeys;
    removeKeys.reserve(m_cache.size());
    for (auto it = m_cache.cbegin(); it != m_cache.cend(); ++it) {
        if (!present.contains(it.key())) {
            removeKeys.push_back(it.key());
        }
    }
    for (const QString &key : removeKeys) {
        m_cache.remove(key);
    }

    std::sort(merged.begin(), merged.end(), [](const FileAction &a, const FileAction &b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;
        }
        const int byName = QString::localeAwareCompare(a.name, b.name);
        if (byName != 0) {
            return byName < 0;
        }
        return a.id < b.id;
    });

    m_actions = merged;
    emit cacheUpdated();
    emit scanFinished();
    return m_actions;
}

QVector<FileAction> ActionDiscoveryScanner::cachedActions() const
{
    return m_actions;
}

} // namespace Slm::Actions
