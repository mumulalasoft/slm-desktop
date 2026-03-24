#pragma once

#include <QObject>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>

class FileManagerApi;

class SlmContextMenuController : public QObject
{
    Q_OBJECT
public:
    explicit SlmContextMenuController(FileManagerApi *api, QObject *parent = nullptr);

    Q_INVOKABLE QVariantList menuRootItems() const;
    Q_INVOKABLE QVariantList menuChildren(const QString &nodeId) const;
    Q_INVOKABLE QVariantList rootItems() const;
    Q_INVOKABLE QVariantList children(const QString &nodeId) const;
    Q_INVOKABLE void invoke(const QString &actionId);

    void setMenuTree(const QVariantMap &nodes, const QStringList &contextUris, const QString &target);
    void clear();

private:
    FileManagerApi *m_api = nullptr;
    QVariantMap m_nodes;
    QStringList m_contextUris;
    QString m_target;
};

class FileManagerApi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *slmContextMenuController READ slmContextMenuController CONSTANT)
    Q_PROPERTY(bool batchOperationActive READ batchOperationActive NOTIFY batchOperationStateChanged)
    Q_PROPERTY(QString batchOperationState READ batchOperationState NOTIFY batchOperationStateChanged)
    Q_PROPERTY(QString batchOperationType READ batchOperationType NOTIFY batchOperationStateChanged)
    Q_PROPERTY(QString batchOperationId READ batchOperationId NOTIFY batchOperationStateChanged)
    Q_PROPERTY(qlonglong batchOperationCurrent READ batchOperationCurrent NOTIFY batchOperationStateChanged)
    Q_PROPERTY(qlonglong batchOperationTotal READ batchOperationTotal NOTIFY batchOperationStateChanged)
    Q_PROPERTY(double batchOperationProgress READ batchOperationProgress NOTIFY batchOperationStateChanged)
    Q_PROPERTY(bool batchOperationPaused READ batchOperationPaused NOTIFY batchOperationStateChanged)
    Q_PROPERTY(int failedBatchCount READ failedBatchCount NOTIFY failedBatchItemsChanged)

public:
    explicit FileManagerApi(QObject *parent = nullptr);
    ~FileManagerApi() override;

    Q_INVOKABLE QVariantMap listDirectory(const QString &path,
                                          bool includeHidden = false,
                                          bool directoriesFirst = true) const;
    Q_INVOKABLE QVariantMap searchDirectory(const QString &rootPath,
                                            const QString &query,
                                            bool includeHidden = false,
                                            bool directoriesFirst = true,
                                            int maxResults = 500,
                                            qulonglong searchSession = 0) const;
    Q_INVOKABLE QVariantMap statPath(const QString &path) const;
    Q_INVOKABLE QVariantMap startStatPaths(const QVariantList &paths,
                                           const QString &requestId = QString());
    Q_INVOKABLE QVariantMap readTextFile(const QString &path, int maxBytes = 1024 * 1024) const;
    Q_INVOKABLE QVariantMap writeTextFile(const QString &path,
                                          const QString &content,
                                          bool append = false);
    Q_INVOKABLE bool isProtectedPath(const QString &path) const;
    Q_INVOKABLE QVariantMap createDirectory(const QString &path, bool recursive = true);
    Q_INVOKABLE QVariantMap createEmptyFile(const QString &path);
    Q_INVOKABLE QVariantMap renamePath(const QString &fromPath, const QString &toPath);
    Q_INVOKABLE QVariantMap removePath(const QString &path, bool recursive = false);
    Q_INVOKABLE QVariantMap copyPath(const QString &sourcePath,
                                     const QString &targetPath,
                                     bool overwrite = false);
    Q_INVOKABLE QVariantMap copyPaths(const QVariantList &sourcePaths,
                                      const QString &targetDirectory,
                                      bool overwrite = false);
    Q_INVOKABLE QVariantMap startCopyPaths(const QVariantList &sourcePaths,
                                           const QString &targetDirectory,
                                           bool overwrite = false);
    Q_INVOKABLE QVariantMap startMovePaths(const QVariantList &sourcePaths,
                                           const QString &targetDirectory,
                                           bool overwrite = false);
    Q_INVOKABLE QVariantMap movePath(const QString &sourcePath,
                                     const QString &targetPath,
                                     bool overwrite = false);
    Q_INVOKABLE QVariantMap removePaths(const QVariantList &paths, bool recursive = true);
    Q_INVOKABLE QVariantMap startRemovePaths(const QVariantList &paths, bool recursive = true);
    Q_INVOKABLE QVariantMap startEmptyTrash(const QString &trashFilesPath = QStringLiteral("~/.local/share/Trash/files"));
    Q_INVOKABLE QVariantMap startTrashPaths(const QVariantList &paths);
    Q_INVOKABLE QVariantMap startRestoreTrashPaths(const QVariantList &paths,
                                                   const QString &fallbackDirectory = QStringLiteral("~"));
    Q_INVOKABLE QVariantMap cancelActiveBatchOperation(const QString &mode);
    Q_INVOKABLE QVariantMap pauseActiveBatchOperation();
    Q_INVOKABLE QVariantMap resumeActiveBatchOperation();
    Q_INVOKABLE QVariantList failedBatchItems() const;
    Q_INVOKABLE QVariantMap retryFailedBatchItem(int index);
    Q_INVOKABLE QVariantMap retryFailedBatchItems();
    Q_INVOKABLE QVariantMap clearRecentFiles();
    Q_INVOKABLE QVariantList recentFiles(int limit = 50) const;
    Q_INVOKABLE QVariantMap setFavoriteFile(const QString &path,
                                            bool favorite = true,
                                            const QString &thumbnailPath = QString());
    Q_INVOKABLE QVariantList favoriteFiles(int limit = 200) const;
    Q_INVOKABLE bool isFavoriteFile(const QString &path) const;
    Q_INVOKABLE QVariantMap addBookmark(const QString &path, const QString &label = QString());
    Q_INVOKABLE QVariantMap removeBookmark(const QString &path);
    Q_INVOKABLE QVariantList bookmarks(int limit = 500) const;
    QVariantMap extractArchive(const QString &archivePath,
                               const QString &destinationDir = QString()) const;
    QVariantMap compressArchive(const QVariantList &sourcePaths,
                                const QString &archivePath,
                                const QString &format = QStringLiteral("zip")) const;
    Q_INVOKABLE QVariantMap startExtractArchive(const QString &archivePath,
                                                const QString &destinationDir = QString());
    Q_INVOKABLE QVariantMap startCompressArchive(const QVariantList &sourcePaths,
                                                 const QString &archivePath,
                                                 const QString &format = QStringLiteral("zip"));
    Q_INVOKABLE QVariantMap requestThumbnailAsync(const QString &path, int size = 256);
    Q_INVOKABLE QString ensureThumbnail(const QString &path, int size = 256) const;
    Q_INVOKABLE QString ensureVideoThumbnail(const QString &path, int size = 256) const;
    Q_INVOKABLE QVariantList storageLocations() const;
    Q_INVOKABLE void refreshStorageLocationsAsync();
    Q_INVOKABLE QVariantMap startMountStorageDevice(const QString &devicePath);
    Q_INVOKABLE QVariantMap startUnmountStorageDevice(const QString &devicePath);
    Q_INVOKABLE QVariantMap startConnectServer(const QString &serverUri);
    Q_INVOKABLE QString startPortalFileChooser(const QVariantMap &options,
                                               const QString &requestId = QString());
    QVariantMap mountStorageDevice(const QString &devicePath) const;
    QVariantMap unmountStorageDevice(const QString &devicePath) const;
    QVariantMap openPath(const QString &path) const;
    QVariantMap openPathWithApp(const QString &path, const QString &appId) const;
    QVariantMap setDefaultOpenWithApp(const QString &path, const QString &appId) const;
    QVariantMap openPathInFileManager(const QString &path) const;
    Q_INVOKABLE QVariantMap startOpenPath(const QString &path,
                                          const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startOpenPathWithApp(const QString &path,
                                                 const QString &appId,
                                                 const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startSetDefaultOpenWithApp(const QString &path,
                                                       const QString &appId,
                                                       const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startOpenPathInFileManager(const QString &path,
                                                       const QString &requestId = QString());
    QVariantList openWithApplications(const QString &path, int limit = 24) const;
    Q_INVOKABLE QVariantMap startOpenWithApplications(const QString &path,
                                                      int limit = 24,
                                                      const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startSlmContextActions(const QVariantList &uris,
                                                   const QString &target = QString(),
                                                   const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startSlmCapabilityActions(const QString &capability,
                                                      const QVariantList &uris,
                                                      const QString &target = QString(),
                                                      const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startInvokeSlmContextAction(const QString &actionId,
                                                        const QVariantList &uris,
                                                        const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startInvokeSlmCapabilityAction(const QString &capability,
                                                           const QString &actionId,
                                                           const QVariantList &uris,
                                                           const QString &requestId = QString());
    Q_INVOKABLE QVariantMap startSlmContextMenuTreeDebug(const QVariantList &uris,
                                                         const QString &target = QString());
    Q_INVOKABLE QVariantMap startResolveSlmDragDropAction(const QVariantList &sourceUris,
                                                          const QString &targetUri,
                                                          const QString &requestId = QString());
    QObject *slmContextMenuController() const;
    Q_INVOKABLE QString slmContextMenuRootId() const;
    Q_INVOKABLE QVariantList slmContextMenuChildren(const QString &nodeId = QStringLiteral("root")) const;
    Q_INVOKABLE QVariantMap copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE bool watchPath(const QString &path);
    Q_INVOKABLE bool watchDirectory(const QString &path);
    Q_INVOKABLE bool watchFile(const QString &path);
    Q_INVOKABLE bool unwatchPath(const QString &path);
    Q_INVOKABLE bool unwatchDirectory(const QString &path);
    Q_INVOKABLE bool unwatchFile(const QString &path);
    Q_INVOKABLE QVariantMap watchedPaths() const;
    Q_INVOKABLE QVariantList watchedDirectories() const;
    Q_INVOKABLE QVariantList watchedFiles() const;
    qulonglong beginSearchSession();
    bool batchOperationActive() const;
    QString batchOperationState() const;
    QString batchOperationType() const;
    QString batchOperationId() const;
    qlonglong batchOperationCurrent() const;
    qlonglong batchOperationTotal() const;
    double batchOperationProgress() const;
    bool batchOperationPaused() const;
    int failedBatchCount() const;

signals:
    void fileOperationProgress(const QString &operation,
                               const QString &sourcePath,
                               const QString &targetPath,
                               qlonglong current,
                               qlonglong total);
    void fileBatchOperationFinished(const QString &operation,
                                    bool ok,
                                    qlonglong processed,
                                    qlonglong total,
                                    const QString &error);
    void batchOperationTaskError(const QString &operation,
                                 const QString &path,
                                 const QString &error);
    void failedBatchItemsChanged();
    void batchOperationStateChanged();
    void directoryChanged(const QString &path);
    void fileChanged(const QString &path);
    void pathChanged(const QString &path, const QString &kind);
    void storageLocationsUpdated(const QVariantList &locations);
    void storageMountFinished(const QString &devicePath,
                              bool ok,
                              const QString &mountedPath,
                              const QString &error);
    void storageUnmountFinished(const QString &devicePath,
                                bool ok,
                                const QString &error);
    void connectServerFinished(const QString &serverUri,
                               bool ok,
                               const QString &mountedPath,
                               const QString &error);
    void portalFileChooserFinished(const QString &requestId,
                                   const QVariantMap &result);
    void statPathsFinished(const QString &requestId, const QVariantList &results);
    void openWithApplicationsReady(const QString &requestId,
                                   const QString &path,
                                   const QVariantList &rows,
                                   const QString &error);
    void slmContextActionsReady(const QString &requestId,
                                const QVariantList &actions,
                                const QString &error);
    void slmCapabilityActionsReady(const QString &requestId,
                                   const QString &capability,
                                   const QVariantList &actions,
                                   const QString &error);
    void slmContextActionInvoked(const QString &requestId,
                                 const QString &actionId,
                                 const QVariantMap &result);
    void slmCapabilityActionInvoked(const QString &requestId,
                                    const QString &capability,
                                    const QString &actionId,
                                    const QVariantMap &result);
    void slmDragDropActionResolved(const QString &requestId,
                                   const QVariantMap &action,
                                   const QString &error);
    void openActionFinished(const QString &requestId,
                            const QString &action,
                            const QVariantMap &result);
    void archiveExtractFinished(const QString &archivePath,
                                bool ok,
                                const QString &destinationDir,
                                const QString &error,
                                const QVariantMap &result);
    void archiveCompressFinished(const QString &archivePath,
                                 bool ok,
                                 const QString &error,
                                 const QVariantMap &result);
    void thumbnailReady(const QString &path,
                        int size,
                        const QString &thumbnailPath,
                        bool ok,
                        const QString &error);

private:
    struct ThumbnailDbusRequest {
        QString path;
        QString uri;
        int size = 256;
    };

    enum class BatchKind {
        None,
        Copy,
        Move,
        Delete,
        Trash,
        Restore
    };
    enum class BatchState {
        Idle,
        Preparing,
        Running,
        Paused,
        Cancelling
    };
    struct BatchTask {
        QString sourcePath;
        QString targetPath;
        QString auxiliaryPath;
        bool isDir = false;
        bool existedBefore = false;
    };
    struct FailedBatchItem {
        BatchTask task;
        QString error;
        BatchKind kind = BatchKind::None;
    };
    enum class CancelMode {
        KeepCompleted,
        RemoveCreated,
        MoveCreatedToTrash
    };

    static QVariantMap makeResult(bool ok,
                                  const QString &error = QString(),
                                  const QVariantMap &extra = QVariantMap());
    static QString expandPath(const QString &path);
    static QVariantMap fileInfoMap(const QFileInfo &info);

    bool ensureDatabase(QString *error = nullptr) const;
    QString databasePath() const;
    void startBatchEngine(BatchKind kind, const QString &id, const QList<BatchTask> &tasks);
    void processBatchStep();
    void finishBatchEngine(bool ok, const QString &error = QString());
    QList<BatchTask> buildCopyTasks(const QStringList &sources,
                                    const QString &targetDirectory,
                                    bool *ok,
                                    QString *error) const;
    QList<BatchTask> buildDeleteTasks(const QStringList &targets,
                                      bool recursive,
                                      bool *ok,
                                      QString *error) const;
    bool rollbackCreatedPaths(CancelMode mode);
    QString uniqueTrashPath(const QString &path) const;
    CancelMode parseCancelMode(const QString &mode) const;
    QString batchKindToString(BatchKind kind) const;
    QString batchStateToString(BatchState state) const;
    void startDaemonBatchJob(BatchKind kind,
                             const QString &jobId,
                             qlonglong total,
                             bool overwrite);
    void startPreparingBatchJob(BatchKind kind,
                                const QString &id,
                                bool overwrite);
    QVariantList queryStorageLocationsSync(int lsblkTimeoutMs) const;
    bool queueFreedesktopThumbnailer(const QString &path, int size);

private slots:
    void onBatchTaskProgress(qlonglong baseBytes, qlonglong currentBytes, qlonglong totalBytes);
    void onDaemonFileOpProgress(const QString &id, int percent);
    void onDaemonFileOpProgressDetail(const QString &id, int current, int total);
    void onDaemonFileOpFinished(const QString &id);
    void onDaemonFileOpError(const QString &id);
    void onDaemonFileOpErrorDetail(const QString &id,
                                   const QString &code,
                                   const QString &message);
    void onDaemonFileOpsJobsChanged();
    void onFreedesktopThumbnailReady(uint handle, const QStringList &uris);
    void onFreedesktopThumbnailError(uint handle, const QStringList &uris, int code, const QString &message);
    void onFreedesktopThumbnailFinished(uint handle);
    void recoverDaemonFileOpState();

private:
    void updateSlmContextMenuController(const QVariantMap &nodes,
                                        const QStringList &contextUris,
                                        const QString &target);

private:
    QFileSystemWatcher m_watcher;
    mutable std::atomic<qulonglong> m_activeSearchSession{0};
    mutable QString m_dbConnectionName;
    mutable bool m_dbReady = false;
    QTimer m_batchTimer;
    BatchKind m_batchKind = BatchKind::None;
    QString m_batchId;
    QList<BatchTask> m_batchTasks;
    qlonglong m_batchCurrent = 0;
    qlonglong m_batchTotal = 0;
    QString m_batchError;
    QStringList m_batchCreatedPaths;
    bool m_batchCancelRequested = false;
    CancelMode m_batchCancelMode = CancelMode::KeepCompleted;
    bool m_batchOverwrite = false;
    bool m_batchTotalIsBytes = false;
    bool m_batchTaskInFlight = false;
    bool m_batchPreparingInFlight = false;
    bool m_batchPaused = false;
    BatchState m_batchState = BatchState::Idle;
    QString m_daemonFileOpJobId;
    bool m_daemonFileOpErrored = false;
    QString m_daemonFileOpError;
    int m_daemonRecoverMisses = 0;
    QList<FailedBatchItem> m_failedBatchItems;
    mutable QVariantList m_storageLocationsCache;
    mutable qint64 m_storageLocationsCacheMs = 0;
    mutable QMutex m_storageCacheMutex;
    mutable bool m_storageRefreshPending = false;
    mutable QMutex m_thumbnailCacheMutex;
    mutable QHash<QString, QString> m_thumbnailMemoPath;
    mutable QHash<QString, qint64> m_thumbnailMemoExpiryMs;
    mutable QHash<QString, qint64> m_thumbnailMemoFailUntilMs;
    QSet<QString> m_thumbnailPending;
    QHash<uint, QList<ThumbnailDbusRequest>> m_thumbnailDbusRequests;
    QVariantMap m_slmContextMenuNodes;
    SlmContextMenuController *m_slmContextMenuController = nullptr;
};
