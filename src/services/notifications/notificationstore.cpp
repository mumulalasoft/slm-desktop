#include "notificationstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QJsonObject toJson(const NotificationEntry &entry)
{
    return {
        {QStringLiteral("id"), static_cast<int>(entry.id)},
        {QStringLiteral("app_id"), entry.appId},
        {QStringLiteral("app_name"), entry.appName},
        {QStringLiteral("title"), entry.summary},
        {QStringLiteral("body"), entry.body},
        {QStringLiteral("icon"), entry.appIcon},
        {QStringLiteral("timestamp"), entry.timestamp.toString(Qt::ISODate)},
        {QStringLiteral("actions"), QJsonArray::fromStringList(entry.actions)},
        {QStringLiteral("priority"), entry.priority},
        {QStringLiteral("group_id"), entry.groupId},
        {QStringLiteral("read"), entry.read},
        {QStringLiteral("banner"), entry.banner},
        {QStringLiteral("lifecycle_state"), entry.lifecycleState}
    };
}

NotificationEntry fromJson(const QJsonObject &obj)
{
    NotificationEntry entry;
    entry.id = static_cast<uint>(obj.value(QStringLiteral("id")).toInt());
    entry.appId = obj.value(QStringLiteral("app_id")).toString();
    entry.appName = obj.value(QStringLiteral("app_name")).toString();
    entry.summary = obj.value(QStringLiteral("title")).toString();
    entry.body = obj.value(QStringLiteral("body")).toString();
    entry.appIcon = obj.value(QStringLiteral("icon")).toString();
    entry.priority = obj.value(QStringLiteral("priority")).toString(QStringLiteral("normal"));
    entry.groupId = obj.value(QStringLiteral("group_id")).toString();
    entry.read = obj.value(QStringLiteral("read")).toBool(true);
    entry.banner = obj.value(QStringLiteral("banner")).toBool(false);
    entry.lifecycleState = obj.value(QStringLiteral("lifecycle_state")).toString(QStringLiteral("Archived"));
    entry.urgency = 1;
    const QJsonArray actions = obj.value(QStringLiteral("actions")).toArray();
    for (const QJsonValue &action : actions) {
        entry.actions.push_back(action.toString());
    }
    const QString ts = obj.value(QStringLiteral("timestamp")).toString();
    if (!ts.isEmpty()) {
        entry.timestamp = QDateTime::fromString(ts, Qt::ISODate);
    }
    return entry;
}
}

bool NotificationStore::saveHistory(const QString &path, uint nextId, const QVector<NotificationEntry> &entries)
{
    if (path.isEmpty()) {
        return false;
    }

    QJsonArray arr;
    for (const NotificationEntry &entry : entries) {
        arr.append(toJson(entry));
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("nextId")] = static_cast<qint64>(nextId);
    root[QStringLiteral("entries")] = arr;

    const QFileInfo info(path);
    const QDir dir = info.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

NotificationStore::LoadedState NotificationStore::loadHistory(const QString &path, int maxEntries)
{
    LoadedState out;
    if (path.isEmpty()) {
        return out;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return out;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (doc.isNull() || !doc.isObject()) {
        Q_UNUSED(parseError)
        return out;
    }

    const QJsonObject root = doc.object();
    out.nextId = static_cast<uint>(root.value(QStringLiteral("nextId")).toInt(1));
    const QJsonArray arr = root.value(QStringLiteral("entries")).toArray();
    out.entries.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        if (maxEntries > 0 && out.entries.size() >= maxEntries) {
            break;
        }
        const NotificationEntry entry = fromJson(val.toObject());
        if (entry.id > 0) {
            out.entries.push_back(entry);
        }
    }
    out.ok = true;
    return out;
}
