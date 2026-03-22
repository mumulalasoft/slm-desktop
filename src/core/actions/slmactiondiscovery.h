#pragma once

#include "slmactiontypes.h"
#include "slmdesktopactionparser.h"

#include <QHash>
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace Slm::Actions {

class ActionDiscoveryScanner : public QObject
{
    Q_OBJECT

public:
    explicit ActionDiscoveryScanner(QObject *parent = nullptr);

    void setScanDirectories(const QStringList &directories);
    QStringList scanDirectories() const;

    QVector<FileAction> scanNow();
    QVector<FileAction> cachedActions() const;
    void setAutoWatchEnabled(bool enabled);
    bool autoWatchEnabled() const;

signals:
    void scanStarted();
    void scanFinished();
    void cacheUpdated();

private:
    void rebuildWatchList(const QStringList &files);
    void scheduleRescan();

    struct CacheEntry {
        qint64 mtimeMs = 0;
        qint64 size = 0;
        QVector<FileAction> actions;
    };

    QStringList discoverDesktopFiles() const;

    QStringList m_scanDirectories;
    DesktopActionParser m_parser;
    QFileSystemWatcher m_watcher;
    QTimer m_rescanTimer;
    bool m_autoWatchEnabled = true;
    QHash<QString, CacheEntry> m_cache;
    QVector<FileAction> m_actions;
};

} // namespace Slm::Actions
