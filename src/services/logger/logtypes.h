#pragma once

#include <QString>
#include <QVariantMap>

// Log levels — match systemd journal priority values (lower = more severe).
enum class LogLevel {
    Debug    = 7,
    Info     = 6,
    Notice   = 5,
    Warning  = 4,
    Error    = 3,
    Critical = 2,
};

inline QString logLevelName(LogLevel l)
{
    switch (l) {
    case LogLevel::Debug:    return QStringLiteral("debug");
    case LogLevel::Info:     return QStringLiteral("info");
    case LogLevel::Notice:   return QStringLiteral("notice");
    case LogLevel::Warning:  return QStringLiteral("warning");
    case LogLevel::Error:    return QStringLiteral("error");
    case LogLevel::Critical: return QStringLiteral("critical");
    }
    return QStringLiteral("info");
}

inline LogLevel logLevelFromPriority(int priority)
{
    switch (priority) {
    case 7:  return LogLevel::Debug;
    case 6:  return LogLevel::Info;
    case 5:  return LogLevel::Notice;
    case 4:  return LogLevel::Warning;
    case 3:  return LogLevel::Error;
    case 2:  return LogLevel::Critical;
    default: return (priority <= 1) ? LogLevel::Critical : LogLevel::Info;
    }
}

struct LogEntry {
    quint64   id        = 0;
    QString   timestamp;     // ISO-8601
    QString   source;        // e.g. "slm-envd"
    LogLevel  level     = LogLevel::Info;
    QString   message;
    QVariantMap fields; // extra structured fields

    QVariantMap toVariantMap() const
    {
        return {
            { QStringLiteral("id"),        id        },
            { QStringLiteral("timestamp"), timestamp },
            { QStringLiteral("source"),    source    },
            { QStringLiteral("level"),     logLevelName(level) },
            { QStringLiteral("message"),   message   },
            { QStringLiteral("fields"),    fields    },
        };
    }
};
