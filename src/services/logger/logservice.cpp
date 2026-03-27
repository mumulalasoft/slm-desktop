#include "logservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>
#include <QTemporaryFile>
#include <QDebug>

// journalctl arguments: follow all slm-* user units, output JSON, include 200 boot entries.
static const QStringList kJournalArgs = {
    QStringLiteral("--follow"),
    QStringLiteral("--output=json"),
    QStringLiteral("--user"),
    QStringLiteral("-n"), QStringLiteral("200"),
    QStringLiteral("_SYSTEMD_USER_UNIT=slm-envd.service"),
    QStringLiteral("+"),
    QStringLiteral("_SYSTEMD_USER_UNIT=slm-loggerd.service"),
    QStringLiteral("+"),
    QStringLiteral("_SYSTEMD_USER_UNIT=slm-svcmgrd.service"),
    QStringLiteral("+"),
    QStringLiteral("_SYSTEMD_USER_UNIT=slm-portald.service"),
    QStringLiteral("+"),
    QStringLiteral("SYSLOG_IDENTIFIER=slm-envd"),
    QStringLiteral("+"),
    QStringLiteral("SYSLOG_IDENTIFIER=slm-loggerd"),
    QStringLiteral("+"),
    QStringLiteral("SYSLOG_IDENTIFIER=slm-svcmgrd"),
    QStringLiteral("+"),
    QStringLiteral("SYSLOG_IDENTIFIER=slm-portald"),
};

static QString sourceFromJournalObject(const QJsonObject &obj)
{
    // Prefer SYSLOG_IDENTIFIER, fall back to _SYSTEMD_USER_UNIT.
    const QString sid = obj.value(QStringLiteral("SYSLOG_IDENTIFIER")).toString();
    if (!sid.isEmpty())
        return sid;
    const QString unit = obj.value(QStringLiteral("_SYSTEMD_USER_UNIT")).toString();
    // Strip .service suffix.
    if (unit.endsWith(QStringLiteral(".service")))
        return unit.chopped(8);
    return unit.isEmpty() ? QStringLiteral("unknown") : unit;
}

static QString isoTimestampFromJournal(const QJsonObject &obj)
{
    // __REALTIME_TIMESTAMP is microseconds since epoch.
    const QString us = obj.value(QStringLiteral("__REALTIME_TIMESTAMP")).toString();
    if (us.isEmpty())
        return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const qint64 msec = us.toLongLong() / 1000;
    return QDateTime::fromMSecsSinceEpoch(msec).toUTC().toString(Qt::ISODate);
}

LogService::LogService(QObject *parent)
    : QObject(parent)
{}

LogService::~LogService()
{
    stop();
}

bool LogService::start()
{
    m_journal = new QProcess(this);
    connect(m_journal, &QProcess::readyReadStandardOutput,
            this, &LogService::onJournalReadyRead);
    connect(m_journal, &QProcess::errorOccurred,
            this, &LogService::onJournalError);

    m_journal->start(QStringLiteral("journalctl"), kJournalArgs);
    if (!m_journal->waitForStarted(3000)) {
        m_lastError = QStringLiteral("journalctl did not start: ") + m_journal->errorString();
        qWarning() << "[slm-loggerd]" << m_lastError;
        // Not fatal — the service still works for direct submissions.
        return false;
    }

    qInfo() << "[slm-loggerd] journalctl follow started (pid" << m_journal->processId() << ")";
    return true;
}

void LogService::stop()
{
    if (m_journal) {
        m_journal->terminate();
        m_journal->waitForFinished(2000);
        m_journal = nullptr;
    }
}

void LogService::onJournalReadyRead()
{
    m_lineBuffer += m_journal->readAllStandardOutput();
    int nl;
    while ((nl = m_lineBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_lineBuffer.left(nl).trimmed();
        m_lineBuffer = m_lineBuffer.mid(nl + 1);
        if (line.isEmpty())
            continue;

        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject obj = doc.object();
        const QString message  = obj.value(QStringLiteral("MESSAGE")).toString();
        if (message.isEmpty())
            continue;

        const int priority = obj.value(QStringLiteral("PRIORITY")).toString(QStringLiteral("6")).toInt();

        LogEntry e;
        e.id        = m_nextId++;
        e.timestamp = isoTimestampFromJournal(obj);
        e.source    = sourceFromJournalObject(obj);
        e.level     = logLevelFromPriority(priority);
        e.message   = message;

        appendEntry(std::move(e));
    }
}

void LogService::onJournalError()
{
    qWarning() << "[slm-loggerd] journalctl error:" << m_journal->errorString();
}

void LogService::appendEntry(LogEntry entry)
{
    if (m_buffer.size() >= kMaxEntries)
        m_buffer.removeFirst();
    emit newEntry(entry);
    dispatchToSubscribers(entry);
    m_buffer.append(std::move(entry));
}

void LogService::dispatchToSubscribers(const LogEntry &entry)
{
    for (const Subscription &sub : std::as_const(m_subs)) {
        if (!sub.source.isEmpty() && sub.source != entry.source)
            continue;
        // Level filter: only pass entries at or above the requested severity.
        if (!sub.level.isEmpty()) {
            const QString entryLevel = logLevelName(entry.level);
            // Simple ordering: critical < error < warning < notice < info < debug
            static const QStringList kOrder = {
                QStringLiteral("debug"), QStringLiteral("info"), QStringLiteral("notice"),
                QStringLiteral("warning"), QStringLiteral("error"), QStringLiteral("critical")
            };
            const int entryIdx  = kOrder.indexOf(entryLevel);
            const int filterIdx = kOrder.indexOf(sub.level);
            if (entryIdx < filterIdx)
                continue;
        }
        emit logEntryForSubscription(sub.id, entry.toVariantMap());
    }
}

// ── Query ────────────────────────────────────────────────────────────────────

bool LogService::matchesFilter(const LogEntry &entry, const QVariantMap &filter) const
{
    const QString source = filter.value(QStringLiteral("source")).toString();
    if (!source.isEmpty() && entry.source != source)
        return false;

    const QString level = filter.value(QStringLiteral("level")).toString();
    if (!level.isEmpty()) {
        static const QStringList kOrder = {
            QStringLiteral("debug"), QStringLiteral("info"), QStringLiteral("notice"),
            QStringLiteral("warning"), QStringLiteral("error"), QStringLiteral("critical")
        };
        const int entryIdx  = kOrder.indexOf(logLevelName(entry.level));
        const int filterIdx = kOrder.indexOf(level);
        if (entryIdx < filterIdx)
            return false;
    }

    const quint64 sinceId = filter.value(QStringLiteral("sinceId"), 0ULL).toULongLong();
    if (sinceId > 0 && entry.id <= sinceId)
        return false;

    return true;
}

QVariantList LogService::getLogs(const QVariantMap &filter) const
{
    const int limit = filter.value(QStringLiteral("limit"), 200).toInt();
    QVariantList result;
    // Walk backwards to get most recent first, then reverse.
    for (int i = m_buffer.size() - 1; i >= 0 && result.size() < limit; --i) {
        if (matchesFilter(m_buffer[i], filter))
            result.prepend(m_buffer[i].toVariantMap());
    }
    return result;
}

QStringList LogService::getSources() const
{
    QStringList sources;
    for (const LogEntry &e : m_buffer) {
        if (!sources.contains(e.source))
            sources.append(e.source);
    }
    sources.sort();
    return sources;
}

// ── Subscriptions ─────────────────────────────────────────────────────────────

quint32 LogService::subscribe(const QString &source, const QString &level)
{
    const quint32 id = m_nextSubId++;
    m_subs.append({id, source, level});
    return id;
}

void LogService::unsubscribe(quint32 subscriptionId)
{
    m_subs.removeIf([subscriptionId](const Subscription &s) {
        return s.id == subscriptionId;
    });
}

// ── Direct submission ─────────────────────────────────────────────────────────

void LogService::submit(const QString &source, const QString &level,
                         const QString &message, const QVariantMap &fields)
{
    static const QHash<QString, LogLevel> kLevelMap = {
        { QStringLiteral("debug"),    LogLevel::Debug    },
        { QStringLiteral("info"),     LogLevel::Info     },
        { QStringLiteral("notice"),   LogLevel::Notice   },
        { QStringLiteral("warning"),  LogLevel::Warning  },
        { QStringLiteral("error"),    LogLevel::Error    },
        { QStringLiteral("critical"), LogLevel::Critical },
    };

    LogEntry e;
    e.id        = m_nextId++;
    e.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    e.source    = source.isEmpty() ? QStringLiteral("unknown") : source;
    e.level     = kLevelMap.value(level.toLower(), LogLevel::Info);
    e.message   = message;
    e.fields    = fields;
    appendEntry(std::move(e));
}

// ── Export ────────────────────────────────────────────────────────────────────

QString LogService::exportBundle(const QVariantMap &filter) const
{
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/slm-logs-XXXXXX.jsonl"));
    if (!tmp.open()) {
        qWarning() << "[slm-loggerd] exportBundle: cannot create temp file";
        return {};
    }

    QTextStream out(&tmp);
    const QVariantList entries = getLogs(filter);
    for (const QVariant &v : entries) {
        out << QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact) << '\n';
    }
    tmp.close();
    return tmp.fileName();
}
