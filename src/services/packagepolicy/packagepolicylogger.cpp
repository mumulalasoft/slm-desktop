#include "packagepolicylogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>

namespace Slm::PackagePolicy {

QString PackagePolicyLogger::logPath()
{
    const QString envPath = qEnvironmentVariable("SLM_PACKAGE_POLICY_LOG_PATH").trimmed();
    if (!envPath.isEmpty()) {
        return envPath;
    }
    return QStringLiteral("/var/log/slm-package-policy.log");
}

void PackagePolicyLogger::writeDecision(const QString &tool,
                                        const QStringList &args,
                                        const QJsonObject &decision,
                                        const QJsonObject &transaction)
{
    QJsonObject entry;
    entry.insert(QStringLiteral("ts"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    entry.insert(QStringLiteral("tool"), tool);
    entry.insert(QStringLiteral("args"), args.join(QLatin1Char(' ')));
    entry.insert(QStringLiteral("decision"), decision);
    entry.insert(QStringLiteral("transaction"), transaction);

    const QByteArray line = QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';

    QString targetPath = logPath();
    QFile f(targetPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        const QString fallbackDir = QDir::homePath() + QStringLiteral("/.local/state/slm");
        QDir().mkpath(fallbackDir);
        targetPath = fallbackDir + QStringLiteral("/package-policy.log");
        f.setFileName(targetPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return;
        }
    }
    f.write(line);
}

} // namespace Slm::PackagePolicy
