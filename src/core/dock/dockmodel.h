#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QVector>

#include "src/core/execution/appruntimeregistry.h"

struct DockEntry {
    QString name;
    QString iconSource;
    QString iconName;
    QString desktopFile;
    QString executable;
    bool isRunning = false;
    bool isPinned = true;
};

class DockModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IconSourceRole,
        IconNameRole,
        DesktopFileRole,
        ExecutableRole,
        IsRunningRole,
        IsPinnedRole
    };

    explicit DockModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool addDesktopEntry(const QString &sourceDesktopFilePath);
    Q_INVOKABLE bool addDesktopEntryAt(const QString &sourceDesktopFilePath, int pinnedPosition);
    Q_INVOKABLE bool removeDesktopEntry(const QString &desktopFilePath);
    Q_INVOKABLE bool launchDesktopEntry(const QString &desktopFilePath, const QString &executable);
    Q_INVOKABLE bool activateExistingWindow(const QString &desktopFilePath, const QString &executable,
                                            const QString &displayName);
    Q_INVOKABLE bool activateOrLaunch(const QString &desktopFilePath, const QString &executable,
                                      const QString &displayName);
    Q_INVOKABLE void noteLaunchedEntry(const QString &desktopFilePath, const QString &executable,
                                       const QString &displayName, const QString &iconName = QString(),
                                       const QString &iconSource = QString());
    Q_INVOKABLE bool movePinnedEntry(int fromModelIndex, int toModelIndex);

private:
    QString dockDirectoryPath() const;
    QString orderFilePath() const;
    bool ensureDockDirectory();
    QStringList loadDockOrder() const;
    void saveDockOrder(const QVector<DockEntry> &entries) const;
    bool focusExistingWindow(const QString &desktopFilePath, const QString &executable,
                             const QString &displayName) const;
    void refreshRunningStates();
    void setupProcessWatcher();
    void scheduleRunningRefresh();
    bool hasWindowForEntry(const DockEntry &entry, const QStringList &wmctrlLines) const;
    bool isEntryRunning(const DockEntry &entry, const QSet<QString> &runningExecutables) const;

    QVector<DockEntry> m_entries;
    QTimer m_runningPollTimer;
    QFileSystemWatcher m_procWatcher;
    QTimer m_runningWatchDebounce;
    AppRuntimeRegistry m_runtimeRegistry;
    QHash<QString, qint64> m_desktopPathLaunchHints;
    QHash<QString, DockEntry> m_pendingTransientByDesktop;
    QHash<QString, qint64> m_pendingTransientExpiryByDesktop;
    QSettings m_localSettings;
    bool m_verboseLogging = false;
};
