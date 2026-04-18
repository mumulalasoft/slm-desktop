#include "cleanerpolicy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace Slm::Cleaner {

QString CleanerPolicyStore::policyPath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(base).filePath(QStringLiteral("slm-desktop/cleaner_policy.json"));
}

CleanerPolicy CleanerPolicyStore::load() const
{
    CleanerPolicy out;
    QFile file(policyPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return out;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return out;
    }
    const QJsonObject obj = doc.object();
    out.autoClean = obj.value(QStringLiteral("auto_clean")).toBool(out.autoClean);
    out.maxCacheSizeMb = qMax(128, obj.value(QStringLiteral("max_cache_size_mb")).toInt(out.maxCacheSizeMb));
    out.deleteAfterDays = qBound(1, obj.value(QStringLiteral("delete_after_days")).toInt(out.deleteAfterDays), 3650);
    return out;
}

bool CleanerPolicyStore::save(const CleanerPolicy &policy) const
{
    const QString path = policyPath();
    const QFileInfo info(path);
    QDir().mkpath(info.dir().absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("auto_clean"), policy.autoClean);
    obj.insert(QStringLiteral("max_cache_size_mb"), policy.maxCacheSizeMb);
    obj.insert(QStringLiteral("delete_after_days"), policy.deleteAfterDays);
    const QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

} // namespace Slm::Cleaner
