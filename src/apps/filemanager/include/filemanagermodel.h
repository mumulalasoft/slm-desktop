#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <atomic>

class FileManagerApi;
class MetadataIndexServer;

struct FileEntry
{
    QString name;
    QString path;
    QString thumbnailPath;
    QString suffix;
    QString mimeType;
    QString iconName;
    QString dateAdded;
    QString lastModified;
    qlonglong size = 0;
    bool dir = false;
    bool hidden = false;
    bool networkShared = false;
};

class FileManagerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath WRITE setCurrentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool includeHidden READ includeHidden WRITE setIncludeHidden NOTIFY includeHiddenChanged)
    Q_PROPERTY(bool directoriesFirst READ directoriesFirst WRITE setDirectoriesFirst NOTIFY directoriesFirstChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY sortKeyChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortDescendingChanged)
    Q_PROPERTY(bool deepSearchRunning READ deepSearchRunning NOTIFY deepSearchRunningChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        ThumbnailPathRole,
        SuffixRole,
        MimeTypeRole,
        IconNameRole,
        DateAddedRole,
        LastModifiedRole,
        SizeRole,
        IsDirRole,
        HiddenRole,
        NetworkSharedRole
    };

    explicit FileManagerModel(FileManagerApi *api,
                              MetadataIndexServer *indexServer = nullptr,
                              QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentPath() const;
    void setCurrentPath(const QString &path);
    bool includeHidden() const;
    void setIncludeHidden(bool include);
    bool directoriesFirst() const;
    void setDirectoriesFirst(bool enabled);
    QString searchText() const;
    void setSearchText(const QString &value);
    QString sortKey() const;
    void setSortKey(const QString &value);
    bool sortDescending() const;
    void setSortDescending(bool value);
    bool deepSearchRunning() const;
    int count() const;
    QString lastError() const;

    Q_INVOKABLE QVariantMap refresh();
    Q_INVOKABLE void setSort(const QString &key, bool descending);
    Q_INVOKABLE QVariantMap goUp();
    Q_INVOKABLE QVariantMap activate(int index);
    Q_INVOKABLE QVariantMap entryAt(int index) const;
    Q_INVOKABLE QVariantMap createDirectory(const QString &name);
    Q_INVOKABLE QVariantMap createFile(const QString &name);
    Q_INVOKABLE QVariantMap removeAt(int index, bool recursive = false);
    Q_INVOKABLE QVariantMap renameAt(int index, const QString &newName);

signals:
    void currentPathChanged();
    void includeHiddenChanged();
    void directoriesFirstChanged();
    void searchTextChanged();
    void sortKeyChanged();
    void sortDescendingChanged();
    void deepSearchRunningChanged();
    void countChanged();
    void lastErrorChanged();

private:
    void setLastError(const QString &error);
    void setDeepSearchRunning(bool running);
    QString resolvePath(const QString &path) const;
    void applyEntries(QVector<FileEntry> entries);
    void cacheStore(const QString &path, const QVector<FileEntry> &entries);
    bool cacheLoad(const QString &path, QVector<FileEntry> *out) const;
    void scheduleRefresh();
    void prewarmCommonDirectories();

    FileManagerApi *m_api = nullptr;
    MetadataIndexServer *m_indexServer = nullptr;
    QVector<FileEntry> m_entries;
    QString m_currentPath;
    QString m_lastError;
    bool m_includeHidden = false;
    bool m_directoriesFirst = true;
    QString m_searchText;
    QString m_sortKey = QStringLiteral("dateModified");
    bool m_sortDescending = true;
    bool m_deepSearchRunning = false;
    std::atomic<quint64> m_refreshGeneration{0};
    QHash<QString, QVector<FileEntry>> m_dirCache;
    QStringList m_dirCacheOrder;
    bool m_refreshDeferredByBatch = false;
    QTimer m_refreshDebounce;
    QString m_lastSearchQuery;
    QVector<FileEntry> m_lastSearchEntries;
    bool m_indexWarmupTriggered = false;
};
