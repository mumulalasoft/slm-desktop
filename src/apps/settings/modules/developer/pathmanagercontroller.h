#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class EnvServiceClient;

class PathManagerController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(QString appId READ appId WRITE setAppId NOTIFY appIdChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList filteredEntries READ filteredEntries NOTIFY filteredEntriesChanged)
    Q_PROPERTY(QStringList finalPath READ finalPath NOTIFY finalPathChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedEntry READ selectedEntry NOTIFY selectedEntryChanged)
    Q_PROPERTY(QStringList appIds READ appIds NOTIFY appIdsChanged)

public:
    explicit PathManagerController(EnvServiceClient *client, QObject *parent = nullptr);

    QString scope() const;
    QString appId() const;
    QString filterText() const;
    QVariantList entries() const;
    QVariantList filteredEntries() const;
    QStringList finalPath() const;
    bool loading() const;
    QString statusText() const;
    QString lastError() const;
    int selectedIndex() const;
    QVariantMap selectedEntry() const;
    QStringList appIds() const;

    void setScope(const QString &scope);
    void setAppId(const QString &appId);
    void setFilterText(const QString &text);
    void setSelectedIndex(int index);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap validatePath(const QString &path) const;
    Q_INVOKABLE QVariantMap previewAddFolder(const QString &path) const;
    Q_INVOKABLE bool addFolder(const QString &path, const QString &position = QStringLiteral("bottom"));
    Q_INVOKABLE bool removeEntry(int index);
    Q_INVOKABLE bool moveEntry(int from, int to);
    Q_INVOKABLE bool setEntryEnabled(int index, bool enabled);
    Q_INVOKABLE bool resetScopePath();
    Q_INVOKABLE QVariantMap resolveCommand(const QString &command) const;
    Q_INVOKABLE QStringList detectMissingRecommendations() const;
    Q_INVOKABLE QVariantMap applyRecommendation(const QString &path);
    Q_INVOKABLE bool openPathInFileManager(const QString &path) const;
    Q_INVOKABLE bool copyPathToClipboard(const QString &path) const;

signals:
    void scopeChanged();
    void appIdChanged();
    void filterTextChanged();
    void entriesChanged();
    void filteredEntriesChanged();
    void finalPathChanged();
    void loadingChanged();
    void statusTextChanged();
    void lastErrorChanged();
    void selectedIndexChanged();
    void selectedEntryChanged();
    void appIdsChanged();

private:
    struct Entry {
        QString path;
        QString canonicalPath;
        QString source;
        bool enabled = true;
        bool editable = true;
        bool missing = false;
        bool duplicate = false;
        QString status;
        QString warning;
        QStringList executables;
    };

    QString normalizePathCandidate(const QString &path) const;
    QString canonicalizePath(const QString &path) const;
    QStringList splitPath(const QString &value) const;
    QString joinPath(const QVector<Entry> &entries, bool enabledOnly) const;
    QVariantList toVariantList(const QVector<Entry> &entries) const;
    void annotateEntries(QVector<Entry> &entries) const;
    QStringList scanExecutablesPreview(const QString &path, int maxItems = 3) const;
    bool writeScopeVar(const QString &key, const QString &value);
    QString readScopeVar(const QString &key) const;
    void setLastError(const QString &error);
    void rebuildFilteredEntries();
    QStringList currentPathForResolution() const;
    void reloadAppIds();

    EnvServiceClient *m_client = nullptr;
    QString m_scope = QStringLiteral("global");
    QString m_appId;
    QString m_filterText;
    QVector<Entry> m_entries;
    QVariantList m_entriesVariant;
    QVariantList m_filteredEntries;
    QStringList m_finalPath;
    bool m_loading = false;
    QString m_statusText;
    QString m_lastError;
    int m_selectedIndex = -1;
    QStringList m_appIds;
};
