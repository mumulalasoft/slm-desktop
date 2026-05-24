#include "cleanerservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace Slm::Cleaner {
namespace {
constexpr const char kLogRelPath[] = "slm/logs/cleaner.log";
}

CleanerService::CleanerService(QObject *parent)
    : QObject(parent)
    , m_engine(this)
    , m_policy(m_policyStore.load())
{
    connect(&m_engine, &CleanerEngine::progressChanged,
            this, &CleanerService::ProgressChanged);
}

QVariantMap CleanerService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QStringLiteral("org.slm.Desktop.Cleaner")},
        {QStringLiteral("cacheHome"), CleanerScanner::resolveCacheHome()}
    };
}

QVariantMap CleanerService::Scan(const QVariantMap &)
{
    QString scanError;
    const QVector<CacheNodeStat> nodes = m_scanner.scanTopLevel(&scanError);
    QVariantMap result = m_analyzer.analyze(nodes);
    result.insert(QStringLiteral("ok"), scanError.isEmpty());
    if (!scanError.isEmpty()) {
        result.insert(QStringLiteral("error"), scanError);
    }
    result.insert(QStringLiteral("cacheHome"), CleanerScanner::resolveCacheHome());
    appendLog(QStringLiteral("scan ok=%1 nodes=%2").arg(scanError.isEmpty() ? 1 : 0).arg(nodes.size()));
    return result;
}

QVariantMap CleanerService::PreviewClean(const QVariantMap &options)
{
    const CleanRequest req = requestFromOptions(options, m_policy);
    QVariantMap result = m_engine.preview(req);
    result.insert(QStringLiteral("ok"), true);
    appendLog(QStringLiteral("preview mode=%1 days=%2").arg(req.mode).arg(req.deleteAfterDays));
    return result;
}

QVariantMap CleanerService::Clean(const QVariantMap &options)
{
    const CleanRequest req = requestFromOptions(options, m_policy);
    QVariantMap result = m_engine.clean(req);
    result.insert(QStringLiteral("mode"), req.mode);
    result.insert(QStringLiteral("days"), req.deleteAfterDays);
    appendLog(QStringLiteral("clean mode=%1 days=%2 deletedBytes=%3 deletedFiles=%4 failed=%5")
              .arg(req.mode)
              .arg(req.deleteAfterDays)
              .arg(result.value(QStringLiteral("deletedBytes")).toULongLong())
              .arg(result.value(QStringLiteral("deletedFiles")).toInt())
              .arg(result.value(QStringLiteral("failedFiles")).toInt()));
    return result;
}

QVariantMap CleanerService::GetPolicy() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("auto_clean"), m_policy.autoClean},
        {QStringLiteral("max_cache_size_mb"), m_policy.maxCacheSizeMb},
        {QStringLiteral("delete_after_days"), m_policy.deleteAfterDays}
    };
}

QVariantMap CleanerService::SetPolicy(const QVariantMap &policyPatch)
{
    CleanerPolicy next = m_policy;
    if (policyPatch.contains(QStringLiteral("auto_clean"))) {
        next.autoClean = policyPatch.value(QStringLiteral("auto_clean")).toBool();
    }
    if (policyPatch.contains(QStringLiteral("max_cache_size_mb"))) {
        const int value = policyPatch.value(QStringLiteral("max_cache_size_mb")).toInt();
        next.maxCacheSizeMb = qMax(128, value > 0 ? value : next.maxCacheSizeMb);
    }
    if (policyPatch.contains(QStringLiteral("delete_after_days"))) {
        const int value = policyPatch.value(QStringLiteral("delete_after_days")).toInt();
        next.deleteAfterDays = qBound(1, value > 0 ? value : next.deleteAfterDays, 3650);
    }
    if (!m_policyStore.save(next)) {
        return errorMap(QStringLiteral("policy-save-failed"),
                        QStringLiteral("Failed to save cleaner policy"));
    }
    m_policy = next;
    appendLog(QStringLiteral("policy updated auto=%1 maxMb=%2 days=%3")
              .arg(m_policy.autoClean ? 1 : 0)
              .arg(m_policy.maxCacheSizeMb)
              .arg(m_policy.deleteAfterDays));
    return GetPolicy();
}

CleanRequest CleanerService::requestFromOptions(const QVariantMap &options,
                                                const CleanerPolicy &policyDefaults)
{
    CleanRequest req;
    req.clearThumbnail = options.value(QStringLiteral("clearThumbnail"), true).toBool();
    req.clearFailedThumbnail = options.value(QStringLiteral("clearFailedThumbnail"), true).toBool();
    req.selectedAppPaths = options.value(QStringLiteral("selectedApps")).toStringList();
    req.mode = options.value(QStringLiteral("mode"), QStringLiteral("full")).toString().trimmed().toLower();
    if (!isValidMode(req.mode)) {
        req.mode = QStringLiteral("full");
    }
    req.deleteAfterDays = qBound(1,
                                 options.value(QStringLiteral("days"), policyDefaults.deleteAfterDays).toInt(),
                                 3650);
    req.previewOnly = options.value(QStringLiteral("preview"), false).toBool();
    return req;
}

QVariantMap CleanerService::errorMap(const QString &code, const QString &message)
{
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), code},
        {QStringLiteral("message"), message}
    };
}

void CleanerService::appendLog(const QString &line) const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString path = QDir(base).filePath(QString::fromLatin1(kLogRelPath));
    const QFileInfo info(path);
    QDir().mkpath(info.dir().absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    const QString stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    file.write(QStringLiteral("%1 %2\n").arg(stamp, line).toUtf8());
}

} // namespace Slm::Cleaner
