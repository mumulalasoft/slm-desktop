#pragma once

#include <QDateTime>
#include <QVariantMap>
#include <QDebug>

#include <atomic>

namespace SlmDbusLog {

inline QString nextRequestId()
{
    static std::atomic<qulonglong> seq{1};
    const qulonglong n = seq.fetch_add(1, std::memory_order_relaxed);
    return QStringLiteral("req-%1-%2")
            .arg(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())
            .arg(n);
}

inline QString normalizeCaller(const QString &caller)
{
    const QString s = caller.trimmed();
    return s.isEmpty() ? QStringLiteral("local") : s;
}

inline QString normalizeJobId(const QString &jobId)
{
    const QString s = jobId.trimmed();
    return s.isEmpty() ? QStringLiteral("-") : s;
}

inline void logEvent(const QString &service,
                 const QString &method,
                 const QString &requestId,
                 const QString &caller,
                 const QString &jobId,
                 const QString &phase,
                 const QVariantMap &fields = {})
{
    QString msg = QStringLiteral("[slm][dbus] service=%1 method=%2 request_id=%3 caller=%4 job_id=%5 phase=%6")
                          .arg(service,
                               method,
                               requestId,
                               normalizeCaller(caller),
                               normalizeJobId(jobId),
                               phase);
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        msg += QStringLiteral(" %1=%2").arg(it.key(), it.value().toString());
    }
    qInfo().noquote() << msg;
}

} // namespace SlmDbusLog

