#include "storagepolicystore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

namespace Slm::Storage {

namespace {

constexpr int kStoragePolicySchemaVersion = 3;
constexpr const char kStoragePolicySchemaVersionKey[] = "schemaVersion";
constexpr const char kStoragePolicyEntriesKey[] = "entries";
constexpr const char kStoragePolicyUpdatedAtKey[] = "updatedAt";

QString canonicalizePolicyKey(const QString &in)
{
    const QString key = in.trimmed();
    if (key.isEmpty()) {
        return QString();
    }
    const int colon = key.indexOf(QLatin1Char(':'));
    if (colon <= 0 || colon >= key.size() - 1) {
        return key;
    }
    const QString prefix = key.left(colon).trimmed().toLower();
    const QString value = key.mid(colon + 1).trimmed();
    if (value.isEmpty()) {
        return QString();
    }

    if (prefix == QStringLiteral("uuid")
            || prefix == QStringLiteral("partuuid")
            || prefix == QStringLiteral("serial-index")) {
        return prefix + QStringLiteral(":") + value.toLower();
    }
    if (prefix == QStringLiteral("device") || prefix == QStringLiteral("group")) {
        return prefix + QStringLiteral(":") + value;
    }
    return prefix + QStringLiteral(":") + value;
}

QVariantMap normalizedPolicyOverride(const QVariantMap &in)
{
    QVariantMap out;
    if (in.contains(QStringLiteral("action"))) {
        const QString action = in.value(QStringLiteral("action")).toString().trimmed().toLower();
        if (action == QStringLiteral("mount")
                || action == QStringLiteral("ask")
                || action == QStringLiteral("ignore")) {
            out.insert(QStringLiteral("action"), action);
        }
    }
    if (in.contains(QStringLiteral("automount"))) {
        out.insert(QStringLiteral("automount"), in.value(QStringLiteral("automount")).toBool());
    }
    if (in.contains(QStringLiteral("auto_open"))) {
        out.insert(QStringLiteral("auto_open"), in.value(QStringLiteral("auto_open")).toBool());
    }
    if (in.contains(QStringLiteral("visible"))) {
        out.insert(QStringLiteral("visible"), in.value(QStringLiteral("visible")).toBool());
    }
    if (in.contains(QStringLiteral("read_only"))) {
        out.insert(QStringLiteral("read_only"), in.value(QStringLiteral("read_only")).toBool());
    }
    if (in.contains(QStringLiteral("exec"))) {
        out.insert(QStringLiteral("exec"), in.value(QStringLiteral("exec")).toBool());
    }
    return out;
}

QVariantMap normalizedPolicyEntries(const QVariantMap &in)
{
    QVariantMap out;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        const QString key = canonicalizePolicyKey(it.key());
        if (key.isEmpty()) {
            continue;
        }
        const QVariantMap normalized = normalizedPolicyOverride(it.value().toMap());
        if (normalized.isEmpty()) {
            continue;
        }
        out.insert(key, normalized);
    }
    return out;
}

QVariantMap normalizePolicyPayloadKeepingKeys(const QVariantMap &in)
{
    QVariantMap out;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        const QString key = it.key().trimmed();
        if (key.isEmpty()) {
            continue;
        }
        const QVariantMap normalized = normalizedPolicyOverride(it.value().toMap());
        if (normalized.isEmpty()) {
            continue;
        }
        out.insert(key, normalized);
    }
    return out;
}

QVariantMap migratePolicyEntriesForSchema(const QVariantMap &in, int sourceSchema)
{
    QVariantMap out = in;
    if (sourceSchema <= 0) {
        out = normalizePolicyPayloadKeepingKeys(out);
    }
    if (sourceSchema < 2) {
        out = normalizePolicyPayloadKeepingKeys(out);
    }
    if (sourceSchema < 3) {
        out = normalizedPolicyEntries(out);
    }
    return out;
}

struct PolicyMigrationTelemetry {
    int sourceSchema = 0;
    int loadedEntries = 0;
    int normalizedEntries = 0;
    int canonicalizedKeyCount = 0;
    int duplicateCanonicalKeyCount = 0;
    int droppedEntryCount = 0;
    bool shouldMigrate = false;
    bool schemaAhead = false;
    bool saveAttempted = false;
    bool saveOk = false;
};

PolicyMigrationTelemetry buildPolicyMigrationTelemetry(const QVariantMap &loadedMap,
                                                       const QVariantMap &normalized,
                                                       int sourceSchema,
                                                       bool shouldMigrate,
                                                       bool schemaAhead)
{
    PolicyMigrationTelemetry t;
    t.sourceSchema = sourceSchema;
    t.loadedEntries = loadedMap.size();
    t.normalizedEntries = normalized.size();
    t.shouldMigrate = shouldMigrate;
    t.schemaAhead = schemaAhead;

    QHash<QString, int> canonicalKeyCounter;
    for (auto it = loadedMap.constBegin(); it != loadedMap.constEnd(); ++it) {
        const QString originalKey = it.key().trimmed();
        if (originalKey.isEmpty()) {
            continue;
        }
        const QString canonicalKey = canonicalizePolicyKey(originalKey);
        if (canonicalKey.isEmpty()) {
            continue;
        }
        if (canonicalKey != originalKey) {
            t.canonicalizedKeyCount++;
        }
        canonicalKeyCounter.insert(canonicalKey, canonicalKeyCounter.value(canonicalKey) + 1);
    }
    for (auto it = canonicalKeyCounter.constBegin(); it != canonicalKeyCounter.constEnd(); ++it) {
        if (it.value() > 1) {
            t.duplicateCanonicalKeyCount += (it.value() - 1);
        }
    }

    if (t.loadedEntries > t.normalizedEntries) {
        t.droppedEntryCount = t.loadedEntries - t.normalizedEntries;
    }
    return t;
}

bool writePolicyDocument(const QString &path, const QVariantMap &policyMap, QString *errorOut)
{
    const QFileInfo fi(path);
    if (!QDir().mkpath(fi.absolutePath())) {
        if (errorOut) {
            *errorOut = QStringLiteral("policy-dir-mkpath-failed");
        }
        return false;
    }
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        if (errorOut) {
            *errorOut = QStringLiteral("policy-open-failed");
        }
        return false;
    }
    QJsonObject root;
    root.insert(QString::fromLatin1(kStoragePolicySchemaVersionKey), kStoragePolicySchemaVersion);
    root.insert(QString::fromLatin1(kStoragePolicyUpdatedAtKey),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QString::fromLatin1(kStoragePolicyEntriesKey), QJsonObject::fromVariantMap(policyMap));
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        if (errorOut) {
            *errorOut = QStringLiteral("policy-commit-failed");
        }
        return false;
    }
    return true;
}

} // namespace

QString StoragePolicyStore::policyFilePath()
{
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configHome.isEmpty()) {
        return QString();
    }
    return QDir(configHome).filePath(QStringLiteral("storage-policies.json"));
}

QVariantMap StoragePolicyStore::loadPolicyMap()
{
    const QString path = policyFilePath();
    if (path.isEmpty()) {
        return QVariantMap();
    }
    QFile f(path);
    if (!f.exists()) {
        return QVariantMap();
    }
    if (!f.open(QIODevice::ReadOnly)) {
        return QVariantMap();
    }
    QJsonParseError parseError{};
    const QByteArray raw = f.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (!doc.isObject()) {
        const QString backupPath = path + QStringLiteral(".corrupt");
        QFile::remove(backupPath);
        QFile::copy(path, backupPath);
        qWarning() << "storage policy json invalid, backup created at" << backupPath
                   << "parseError" << parseError.errorString();
        return QVariantMap();
    }
    const QJsonObject root = doc.object();
    const int schemaVersion = root.value(QString::fromLatin1(kStoragePolicySchemaVersionKey)).toInt(0);
    bool schemaAhead = false;
    const QJsonValue entriesValue = root.value(QString::fromLatin1(kStoragePolicyEntriesKey));
    QVariantMap loadedMap;
    bool shouldMigrate = false;
    int sourceSchema = 0;

    if (entriesValue.isObject()) {
        sourceSchema = schemaVersion;
        if (schemaVersion > kStoragePolicySchemaVersion) {
            schemaAhead = true;
            qWarning() << "storage policy schema newer than supported:"
                       << schemaVersion << "supported" << kStoragePolicySchemaVersion
                       << "- attempting to read entries payload";
        }
        loadedMap = entriesValue.toObject().toVariantMap();
        if (schemaVersion <= 0 || schemaVersion < kStoragePolicySchemaVersion) {
            shouldMigrate = true;
            sourceSchema = schemaVersion <= 0 ? 1 : schemaVersion;
        }
    } else {
        loadedMap = root.toVariantMap();
        shouldMigrate = true;
        sourceSchema = 0;
    }

    const QVariantMap migrated = migratePolicyEntriesForSchema(loadedMap, sourceSchema);
    const QVariantMap normalized = normalizedPolicyEntries(migrated);
    if (!shouldMigrate && normalized != loadedMap) {
        shouldMigrate = true;
    }
    PolicyMigrationTelemetry telemetry = buildPolicyMigrationTelemetry(loadedMap,
                                                                       normalized,
                                                                       sourceSchema,
                                                                       shouldMigrate,
                                                                       schemaAhead);
    if (shouldMigrate) {
        telemetry.saveAttempted = true;
        QString migrateErr;
        if (!writePolicyDocument(path, normalized, &migrateErr)) {
            telemetry.saveOk = false;
            qWarning() << "storage policy migration failed from schema" << sourceSchema
                       << "error" << migrateErr;
        } else {
            telemetry.saveOk = true;
            qInfo() << "storage policy migrated from schema" << sourceSchema
                    << "to" << kStoragePolicySchemaVersion;
        }
    }
    qDebug().noquote()
            << QStringLiteral("storage policy migration telemetry")
            << QStringLiteral("sourceSchema=%1").arg(telemetry.sourceSchema)
            << QStringLiteral("loaded=%1").arg(telemetry.loadedEntries)
            << QStringLiteral("normalized=%1").arg(telemetry.normalizedEntries)
            << QStringLiteral("canonicalized=%1").arg(telemetry.canonicalizedKeyCount)
            << QStringLiteral("canonicalCollisions=%1").arg(telemetry.duplicateCanonicalKeyCount)
            << QStringLiteral("dropped=%1").arg(telemetry.droppedEntryCount)
            << QStringLiteral("shouldMigrate=%1").arg(telemetry.shouldMigrate ? 1 : 0)
            << QStringLiteral("schemaAhead=%1").arg(telemetry.schemaAhead ? 1 : 0)
            << QStringLiteral("saveAttempted=%1").arg(telemetry.saveAttempted ? 1 : 0)
            << QStringLiteral("saveOk=%1").arg(telemetry.saveOk ? 1 : 0);
    return normalized;
}

bool StoragePolicyStore::savePolicyMap(const QVariantMap &policyMap, QString *errorOut)
{
    const QString path = policyFilePath();
    if (path.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("policy-path-unavailable");
        }
        return false;
    }
    return writePolicyDocument(path, normalizedPolicyEntries(policyMap), errorOut);
}

} // namespace Slm::Storage
