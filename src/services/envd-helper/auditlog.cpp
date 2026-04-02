#include "auditlog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

// static
QString AuditLog::logPath()
{
    return QStringLiteral("/var/log/slm/envd-audit.log");
}

// static
void AuditLog::record(const QString &operation,
                      const QString &key,
                      quint32        uid,
                      bool           success,
                      const QString &detail)
{
    const QString path = logPath();

    // Ensure the log directory exists (best-effort; we run as root).
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject obj;
    obj[QStringLiteral("ts")]  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    obj[QStringLiteral("op")]  = operation;
    obj[QStringLiteral("key")] = key;
    obj[QStringLiteral("uid")] = static_cast<qint64>(uid);
    obj[QStringLiteral("result")] = success ? QStringLiteral("ok") : QStringLiteral("denied");
    if (!detail.isEmpty())
        obj[QStringLiteral("detail")] = detail;

    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return; // silent failure — audit log must not block the operation

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    f.write("\n");
}
