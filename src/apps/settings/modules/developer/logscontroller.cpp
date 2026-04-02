#include "logscontroller.h"

LogsController::LogsController(LogServiceClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &LogServiceClient::serviceAvailableChanged,
            this, &LogsController::onServiceAvailableChanged);
    connect(m_client, &LogServiceClient::logEntry,
            this, &LogsController::onLogEntry);

    if (m_client->serviceAvailable())
        refresh();
}

LogsController::~LogsController()
{
    stopStream();
}

bool LogsController::serviceAvailable() const
{
    return m_client->serviceAvailable();
}

void LogsController::setFilterSource(const QString &source)
{
    if (m_filterSource == source) return;
    m_filterSource = source;
    emit filterSourceChanged();
    refresh();
}

void LogsController::setFilterLevel(const QString &level)
{
    if (m_filterLevel == level) return;
    m_filterLevel = level;
    emit filterLevelChanged();
    refresh();
}

void LogsController::refresh()
{
    if (!m_client->serviceAvailable()) {
        m_entries.clear();
        emit entriesChanged();
        return;
    }

    QVariantMap filter;
    if (!m_filterSource.isEmpty()) filter[QStringLiteral("source")] = m_filterSource;
    if (!m_filterLevel.isEmpty())  filter[QStringLiteral("level")]  = m_filterLevel;
    filter[QStringLiteral("limit")] = 300;

    m_entries = m_client->getLogs(filter);
    emit entriesChanged();

    reloadSources();
}

void LogsController::reloadSources()
{
    QStringList s = m_client->getSources();
    if (s != m_sources) {
        m_sources = s;
        emit sourcesChanged();
    }
}

void LogsController::startStream()
{
    if (m_subId != 0) return;
    m_subId = m_client->subscribe(m_filterSource, m_filterLevel);
    emit streamingChanged();
}

void LogsController::stopStream()
{
    if (m_subId == 0) return;
    m_client->unsubscribe(m_subId);
    m_subId = 0;
    emit streamingChanged();
}

QString LogsController::exportBundle()
{
    QVariantMap filter;
    if (!m_filterSource.isEmpty()) filter[QStringLiteral("source")] = m_filterSource;
    if (!m_filterLevel.isEmpty())  filter[QStringLiteral("level")]  = m_filterLevel;
    return m_client->exportBundle(filter);
}

void LogsController::onServiceAvailableChanged()
{
    emit serviceAvailableChanged();
    if (m_client->serviceAvailable()) {
        refresh();
        if (m_subId != 0) {
            // Re-subscribe after service restart.
            m_subId = 0;
            startStream();
        }
    } else {
        m_entries.clear();
        emit entriesChanged();
        m_subId = 0;
        emit streamingChanged();
    }
}

void LogsController::onLogEntry(quint32 subId, const QVariantMap &entry)
{
    if (subId != m_subId) return;
    // Append and trim to 300.
    m_entries.append(entry);
    if (m_entries.size() > 300)
        m_entries.removeFirst();
    emit entriesChanged();
    reloadSources();
}
