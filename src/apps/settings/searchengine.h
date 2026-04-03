#pragma once

#include <QObject>
#include <QVariantList>

class ModuleLoader;

class SearchEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList groupedResults READ groupedResults NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList sidebarModules READ sidebarModules NOTIFY resultsChanged)
    Q_PROPERTY(qint64 lastSearchLatencyMs READ lastSearchLatencyMs NOTIFY searchTelemetryChanged)
    Q_PROPERTY(int lastSearchResultCount READ lastSearchResultCount NOTIFY searchTelemetryChanged)

public:
    explicit SearchEngine(ModuleLoader *loader, QObject *parent = nullptr);

    QString searchQuery() const { return m_searchQuery; }
    void setSearchQuery(const QString &query);

    QVariantList results() const { return m_results; }
    QVariantList groupedResults() const { return m_groupedResults; }
    QVariantList sidebarModules() const { return m_sidebarModules; }
    qint64 lastSearchLatencyMs() const { return m_lastSearchLatencyMs; }
    int lastSearchResultCount() const { return m_lastSearchResultCount; }

    Q_INVOKABLE QVariantList query(const QString &text, int limit = 25) const;

signals:
    void searchQueryChanged();
    void resultsChanged();
    void searchTelemetryChanged();

private:
    QVariantList queryInternal(const QString &text, int limit) const;
    static int scoreMatch(const QString &query, const QString &text);
    static QString norm(const QString &v);
    void performSearch();

    ModuleLoader *m_loader;
    QString m_searchQuery;
    QVariantList m_results;
    QVariantList m_groupedResults;
    QVariantList m_sidebarModules;
    qint64 m_lastSearchLatencyMs = 0;
    int m_lastSearchResultCount = 0;
};
