#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class FileManagerApi;

class MetadataIndexServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(int indexedCount READ indexedCount NOTIFY indexedCountChanged)
    Q_PROPERTY(bool indexing READ indexing NOTIFY indexingStateChanged)
    Q_PROPERTY(QString indexingPath READ indexingPath NOTIFY indexingStateChanged)
    Q_PROPERTY(int indexingCurrent READ indexingCurrent NOTIFY indexingStateChanged)
    Q_PROPERTY(int indexingTotal READ indexingTotal NOTIFY indexingStateChanged)

public:
    explicit MetadataIndexServer(FileManagerApi *fileApi, QObject *parent = nullptr);
    ~MetadataIndexServer() override;

    bool ready() const;
    int indexedCount() const;
    bool indexing() const;
    QString indexingPath() const;
    int indexingCurrent() const;
    int indexingTotal() const;

    Q_INVOKABLE QVariantMap ensureDefaultIndex();
    Q_INVOKABLE QVariantMap indexPath(const QString &path, bool recursive = true);
    Q_INVOKABLE QVariantMap removePath(const QString &path);
    Q_INVOKABLE QVariantList search(const QString &query,
                                    int limit = 100,
                                    const QString &underPath = QString());
    Q_INVOKABLE QVariantList searchAdvanced(const QString &query,
                                            const QVariantMap &options,
                                            int limit = 100);
    Q_INVOKABLE QVariantMap stats();
    Q_INVOKABLE bool addRoot(const QString &path);
    Q_INVOKABLE bool removeRoot(const QString &path);
    Q_INVOKABLE QVariantList roots() const;
    Q_INVOKABLE bool startDefaultIndexing();
    Q_INVOKABLE bool startIndexPath(const QString &path, bool recursive = true);
    Q_INVOKABLE void cancelIndexing();

public slots:
    void onPathChanged(const QString &path, const QString &kind);

signals:
    void readyChanged();
    void indexedCountChanged();
    void indexingStateChanged();
    void indexingStarted(const QString &path);
    void indexingProgress(const QString &path, int current, int total);
    void indexingFinished(const QString &path, int indexedItems);
    void indexChanged(const QString &path);

private:
    bool ensureDatabase(QString *error = nullptr);
    QString databasePath() const;
    QString normalizePath(const QString &path) const;
    QVariantMap upsertEntry(const QString &path);
    QVariantMap upsertDirectoryChildren(const QString &dirPath, bool recursive);
    void setIndexedCountInternal(int value);
    void finishIndexingJob(bool emitFinished = true);
    void processIndexBatch();
    void processPathChangeQueue();

    FileManagerApi *m_fileApi = nullptr;
    mutable QString m_dbConnectionName;
    mutable bool m_dbReady = false;
    mutable int m_indexedCount = 0;
    mutable bool m_defaultIndexed = false;
    QStringList m_roots;
    bool m_indexing = false;
    QString m_indexingPath;
    int m_indexingCurrent = 0;
    int m_indexingTotal = 0;
    bool m_indexingRecursive = true;
    QStringList m_pendingPaths;
    QStringList m_defaultIndexQueue;
    QStringList m_pendingPathChanges;
    bool m_pathChangeFlushScheduled = false;
};
