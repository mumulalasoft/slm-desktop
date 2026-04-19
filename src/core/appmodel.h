#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

typedef struct _GAppInfoMonitor GAppInfoMonitor;
typedef struct _GFile GFile;
typedef struct _GFileMonitor GFileMonitor;

class AppExecutionGate;
class QTimer;

struct DesktopAppEntry {
    QString name;
    QString iconSource;
    QString iconName;
    QString desktopId;
    QString desktopFile;
    QString executable;
    int score = 0;
    int launchCount7d = 0;
    int fileOpenCount7d = 0;
    QString lastLaunchIso;
};

class DesktopAppModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IconSourceRole,
        IconNameRole,
        DesktopIdRole,
        DesktopFileRole,
        ExecutableRole,
        ScoreRole,
        LaunchCount7dRole,
        FileOpenCount7dRole,
        LastLaunchRole
    };

    explicit DesktopAppModel(QObject *parent = nullptr);
    ~DesktopAppModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    // Non-blocking variant: heavy GIO + filesystem work runs on a thread-pool
    // thread. The model is updated on the main thread when the scan completes.
    Q_INVOKABLE void refreshAsync();
    Q_INVOKABLE int countMatching(const QString &searchText) const;
    Q_INVOKABLE QVariantList page(int pageIndex, int pageSize, const QString &searchText) const;
    Q_INVOKABLE QVariantMap appUsage(const QString &desktopId,
                                     const QString &desktopFile,
                                     const QString &executable) const;
    Q_INVOKABLE QString canonicalAppIdentity(const QString &desktopId,
                                             const QString &desktopFile,
                                             const QString &executable,
                                             const QString &name = QString()) const;
    Q_INVOKABLE QVariantList frequentApps(int limit = 24) const;
    Q_INVOKABLE QVariantList topApps(int limit = 24) const; // compatibility alias
    Q_INVOKABLE QVariantList slmQuickActions(const QString &scope) const;
    Q_INVOKABLE QVariantList slmQuickActionsForEntry(const QString &scope,
                                                     const QVariantMap &entry) const;
    Q_INVOKABLE QVariantMap invokeSlmQuickAction(const QString &actionId,
                                                 const QVariantMap &context = QVariantMap{});
    void setExecutionGate(AppExecutionGate *gate);
    void setDesktopSettings(QObject *desktopSettings);

signals:
    void appScoresChanged();

private:
    static void onGAppInfoChanged(GAppInfoMonitor *monitor, void *userData);
    static void onDesktopDirChanged(GFileMonitor *monitor,
                                    GFile *file,
                                    GFile *otherFile,
                                    int eventType,
                                    void *userData);
    void setupDesktopDirMonitors();
    void clearDesktopDirMonitors();
    void rebuildUsageStats();
    void loadLaunchHistory();
    void saveLaunchHistory() const;
    void noteLaunchEvent(const QString &name, const QString &desktopFile, const QString &executable);
    void applyUsageToApps();
    void pruneOldLaunches();
    static QString primaryKey(const QString &desktopId,
                              const QString &desktopFile,
                              const QString &executable,
                              const QString &name);
    static QStringList keysFromApp(const DesktopAppEntry &app);
    static QStringList keysFromUsageRecord(const QString &appName, const QString &appExec);
    void reloadScoringWeights();
    int effectiveScore(int launchCount, int fileOpenCount, qint64 lastLaunchMs) const;
    static QVector<DesktopAppEntry> computeAppsFromSystem();

private slots:
    void onAppExecutionRecorded(QString source, QString name, QString desktopFile, QString executable, bool success);
    void onDesktopSettingChanged(QString path);

private:
    bool m_refreshRunning = false;
    AppExecutionGate *m_gate = nullptr;
    QObject *m_desktopSettings = nullptr;
    QVector<DesktopAppEntry> m_apps;
    void *m_appInfoMonitor = nullptr;
    QVector<void *> m_appDirMonitors;
    QTimer *m_refreshDebounceTimer = nullptr;
    QHash<QString, QVariantList> m_launchHistoryByKey;
    QHash<QString, QVariantMap> m_usageStatsByKey;
    int m_launchWeight = 10;
    int m_fileOpenWeight = 3;
    int m_recencyWeight = 1;
};
