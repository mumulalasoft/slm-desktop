#include "appmodel.h"
#include "src/core/execution/appexecutiongate.h"
#include "src/core/prefs/uipreferences.h"

#ifdef QT_DBUS_LIB
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#endif

#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <QtCore/qobjectdefs.h>
#include <QVariantMap>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QUrl>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QLoggingCategory>
#include <QTimeZone>
#include <QRegularExpression>
#include <QProcess>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QMutex>

#ifdef signals
#undef signals
#define DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif
extern "C" {
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
}
#ifdef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#define signals Q_SIGNALS
#undef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif

namespace {
Q_LOGGING_CATEGORY(lcAppModel, "slm.appmodel")
#ifdef QT_DBUS_LIB
constexpr const char kCapabilityService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kCapabilityPath[] = "/org/freedesktop/SLMCapabilities";
constexpr const char kCapabilityIface[] = "org.freedesktop.SLMCapabilities";
#endif

QString fromUtf8(const char *value)
{
    return value ? QString::fromUtf8(value) : QString();
}

QString normalizedDesktopFileBase(const QString &desktopFile)
{
    QString base = QFileInfo(desktopFile.trimmed()).fileName().toLower();
    if (base.endsWith(QStringLiteral(".desktop"))) {
        base.chop(8);
    }
    return base;
}

QString scopeToken(const QString &scope)
{
    return scope.trimmed().toLower();
}

bool listContainsToken(const QString &rawList, const QString &token)
{
    const QString t = token.trimmed().toLower();
    if (t.isEmpty()) {
        return false;
    }
    const QStringList parts = rawList.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.trimmed().toLower() == t) {
            return true;
        }
    }
    return false;
}

QString resolveDesktopFilePath(const QVariantMap &entry)
{
    const QString directPath = entry.value(QStringLiteral("desktopFile")).toString().trimmed();
    if (!directPath.isEmpty() && QFileInfo::exists(directPath)) {
        return QFileInfo(directPath).absoluteFilePath();
    }
    QString desktopId = entry.value(QStringLiteral("desktopId")).toString().trimmed();
    if (desktopId.isEmpty()) {
        return QString();
    }
    if (!desktopId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        desktopId += QStringLiteral(".desktop");
    }
    const QString localPath = QDir::home().filePath(QStringLiteral(".local/share/applications/%1").arg(desktopId));
    if (QFileInfo::exists(localPath)) {
        return QFileInfo(localPath).absoluteFilePath();
    }
    const QString sysPath = QStringLiteral("/usr/share/applications/%1").arg(desktopId);
    if (QFileInfo::exists(sysPath)) {
        return QFileInfo(sysPath).absoluteFilePath();
    }
    return QString();
}

QString localQuickActionId(const QString &desktopFilePath, const QString &actionGroupSuffix)
{
    return QStringLiteral("local-qa:%1|%2")
            .arg(QUrl::toPercentEncoding(desktopFilePath),
                 QUrl::toPercentEncoding(actionGroupSuffix));
}

bool parseLocalQuickActionId(const QString &actionId,
                             QString *desktopFilePathOut,
                             QString *groupSuffixOut)
{
    const QString prefix = QStringLiteral("local-qa:");
    if (!actionId.startsWith(prefix)) {
        return false;
    }
    const QString encoded = actionId.mid(prefix.size());
    const int sep = encoded.indexOf(QLatin1Char('|'));
    if (sep <= 0) {
        return false;
    }
    const QString left = QUrl::fromPercentEncoding(encoded.left(sep).toUtf8());
    const QString right = QUrl::fromPercentEncoding(encoded.mid(sep + 1).toUtf8());
    if (left.trimmed().isEmpty() || right.trimmed().isEmpty()) {
        return false;
    }
    if (desktopFilePathOut) {
        *desktopFilePathOut = left.trimmed();
    }
    if (groupSuffixOut) {
        *groupSuffixOut = right.trimmed();
    }
    return true;
}

QString fillExecNoUris(const QString &execTemplate)
{
    QString out = execTemplate;
    out.replace(QStringLiteral("%F"), QString());
    out.replace(QStringLiteral("%f"), QString());
    out.replace(QStringLiteral("%U"), QString());
    out.replace(QStringLiteral("%u"), QString());
    out.replace(QStringLiteral("%%"), QStringLiteral("%"));
    return out.trimmed();
}

QVariantList localQuickActionsFromDesktopEntry(const QVariantMap &entry, const QString &scope)
{
    const QString desktopFilePath = resolveDesktopFilePath(entry);
    if (desktopFilePath.isEmpty()) {
        return {};
    }

    GKeyFile *kf = g_key_file_new();
    GError *error = nullptr;
    const gboolean loaded = g_key_file_load_from_file(
        kf, QFile::encodeName(desktopFilePath).constData(), G_KEY_FILE_NONE, &error);
    if (error) {
        g_error_free(error);
    }
    if (!loaded) {
        g_key_file_free(kf);
        return {};
    }

    gsize groupCount = 0;
    gchar **groups = g_key_file_get_groups(kf, &groupCount);
    QVariantList rows;
    const QString scopeValue = scopeToken(scope);
    for (gsize i = 0; i < groupCount; ++i) {
        const QString groupName = fromUtf8(groups[i]);
        const QString prefix = QStringLiteral("Desktop Action ");
        if (!groupName.startsWith(prefix)) {
            continue;
        }
        const QString suffix = groupName.mid(prefix.size()).trimmed();
        if (suffix.isEmpty()) {
            continue;
        }
        const QByteArray groupUtf8 = groupName.toUtf8();
        const QString caps = fromUtf8(g_key_file_get_string(kf,
                                                            groupUtf8.constData(),
                                                            "X-SLM-Capabilities",
                                                            nullptr));
        if (!listContainsToken(caps, QStringLiteral("quickaction"))) {
            continue;
        }
        const QString scopes = fromUtf8(g_key_file_get_string(kf,
                                                              groupUtf8.constData(),
                                                              "X-SLM-QuickAction-Scopes",
                                                              nullptr));
        if (!scopeValue.isEmpty() && !scopes.trimmed().isEmpty()
            && !listContainsToken(scopes, scopeValue)) {
            continue;
        }
        const QString name = fromUtf8(g_key_file_get_string(kf,
                                                            groupUtf8.constData(),
                                                            "Name",
                                                            nullptr)).trimmed();
        const QString exec = fromUtf8(g_key_file_get_string(kf,
                                                            groupUtf8.constData(),
                                                            "Exec",
                                                            nullptr)).trimmed();
        if (name.isEmpty() || exec.isEmpty()) {
            continue;
        }
        const QString icon = fromUtf8(g_key_file_get_string(kf,
                                                            groupUtf8.constData(),
                                                            "Icon",
                                                            nullptr)).trimmed();
        QVariantMap row;
        row.insert(QStringLiteral("id"), localQuickActionId(desktopFilePath, suffix));
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("exec"), exec);
        row.insert(QStringLiteral("icon"), icon);
        row.insert(QStringLiteral("desktopFilePath"), desktopFilePath);
        row.insert(QStringLiteral("desktopId"), entry.value(QStringLiteral("desktopId")).toString());
        rows.push_back(row);
    }

    if (groups) {
        g_strfreev(groups);
    }
    g_key_file_free(kf);
    return rows;
}

bool quickActionMatchesEntry(const QVariantMap &action, const QVariantMap &entry)
{
    const QString actionDesktopId = action.value(QStringLiteral("desktopId")).toString().trimmed().toLower();
    const QString actionDesktopPath = action.value(QStringLiteral("desktopFilePath")).toString().trimmed();
    const QString actionDesktopBase = normalizedDesktopFileBase(actionDesktopPath);

    const QString entryDesktopId = entry.value(QStringLiteral("desktopId")).toString().trimmed().toLower();
    const QString entryDesktopPath = entry.value(QStringLiteral("desktopFile")).toString().trimmed();
    const QString entryDesktopBase = normalizedDesktopFileBase(entryDesktopPath);

    if (!actionDesktopId.isEmpty() && !entryDesktopId.isEmpty() && actionDesktopId == entryDesktopId) {
        return true;
    }
    if (!actionDesktopBase.isEmpty() && !entryDesktopBase.isEmpty() && actionDesktopBase == entryDesktopBase) {
        return true;
    }
    if (!actionDesktopId.isEmpty() && !entryDesktopBase.isEmpty()
        && (actionDesktopId == entryDesktopBase
            || actionDesktopId == (entryDesktopBase + QStringLiteral(".desktop")))) {
        return true;
    }
    if (!entryDesktopId.isEmpty() && !actionDesktopBase.isEmpty()
        && (entryDesktopId == actionDesktopBase
            || entryDesktopId == (actionDesktopBase + QStringLiteral(".desktop")))) {
        return true;
    }
    return false;
}

QString toQmlFileSource(const QString &path)
{
    if (path.isEmpty()) {
        return QString();
    }

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return QString();
    }
    return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
}

QString iconFilePathFromGIcon(GIcon *icon)
{
    if (!icon) {
        return QString();
    }

    if (G_IS_FILE_ICON(icon)) {
        GFile *file = g_file_icon_get_file(G_FILE_ICON(icon));
        if (!file) {
            return QString();
        }
        gchar *path = g_file_get_path(file);
        const QString result = fromUtf8(path);
        g_free(path);
        return toQmlFileSource(result);
    }

    const gchar *serialized = g_icon_to_string(icon);
    if (!serialized) {
        return QString();
    }
    const QString maybePath = fromUtf8(serialized);
    if (QFileInfo::exists(maybePath)) {
        return toQmlFileSource(maybePath);
    }
    return QString();
}

QString iconNameFromGIcon(GIcon *icon)
{
    if (!icon) {
        return QString();
    }

    if (G_IS_THEMED_ICON(icon)) {
        const gchar *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) {
            return fromUtf8(names[0]);
        }
    }

    return QString();
}

// Thread-safe icon path resolver.
// Heavy filesystem scanning runs without holding the lock; only the cache
// lookup and insert are protected so multiple threads can call concurrently.
QString resolveThemedIconPath(const QString &rawIconName)
{
    static QHash<QString, QString> cache;
    static QMutex cacheMutex;

    const QString iconName = rawIconName.trimmed();
    if (iconName.isEmpty()) {
        return QString();
    }

    // Fast path: already cached.
    {
        QMutexLocker lk(&cacheMutex);
        const auto it = cache.constFind(iconName);
        if (it != cache.cend()) {
            return it.value();
        }
    }

    // Slow path: scan filesystem (no lock held — pure reads).
    QString resolved;

    QFileInfo iconInfo(iconName);
    if (iconInfo.exists() && iconInfo.isFile()) {
        resolved = toQmlFileSource(iconInfo.absoluteFilePath());
    } else {
        const QStringList exts = {QStringLiteral("png"), QStringLiteral("svg"),
                                  QStringLiteral("xpm"), QStringLiteral("svgz")};
        const QStringList contexts = {
            QStringLiteral("apps"),    QStringLiteral("actions"),
            QStringLiteral("categories"), QStringLiteral("devices"),
            QStringLiteral("mimetypes"), QStringLiteral("places"),
            QStringLiteral("status"),  QStringLiteral("emblems")
        };
        const QStringList sizeDirs = {
            QStringLiteral("symbolic"), QStringLiteral("scalable"),
            QStringLiteral("512x512"),  QStringLiteral("256x256"),
            QStringLiteral("192x192"),  QStringLiteral("128x128"),
            QStringLiteral("96x96"),    QStringLiteral("72x72"),
            QStringLiteral("64x64"),    QStringLiteral("48x48"),
            QStringLiteral("36x36"),    QStringLiteral("32x32"),
            QStringLiteral("24x24"),    QStringLiteral("22x22"),
            QStringLiteral("16x16")
        };

        QStringList searchRoots = QStandardPaths::standardLocations(
            QStandardPaths::GenericDataLocation);
        searchRoots << QStringLiteral("/usr/share") << QStringLiteral("/usr/local/share");
        searchRoots.removeDuplicates();

        QStringList iconThemeBases;
        for (const QString &root : searchRoots) {
            iconThemeBases << QDir(root).filePath(QStringLiteral("icons"));
        }
        iconThemeBases << QDir::home().filePath(QStringLiteral(".icons"));

        QStringList pixmapBases;
        for (const QString &root : searchRoots) {
            pixmapBases << QDir(root).filePath(QStringLiteral("pixmaps"));
        }

        QStringList candidateNames;
        candidateNames << iconName;
        if (iconName.contains('.')) {
            const QString maybeBase = QFileInfo(iconName).completeBaseName();
            if (!maybeBase.isEmpty() && maybeBase != iconName) {
                candidateNames << maybeBase;
            }
        }

        auto tryFile = [&](const QString &path) -> bool {
            const QFileInfo fi(path);
            if (fi.exists() && fi.isFile()) {
                resolved = toQmlFileSource(fi.absoluteFilePath());
                return true;
            }
            return false;
        };

        bool found = false;
        for (const QString &base : iconThemeBases) {
            if (found) break;
            QDir baseDir(base);
            if (!baseDir.exists()) continue;
            QStringList themes = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            if (!themes.contains(QStringLiteral("hicolor"))) {
                themes.prepend(QStringLiteral("hicolor"));
            }
            for (const QString &theme : themes) {
                if (found) break;
                const QString themePath = baseDir.filePath(theme);
                for (const QString &sizeDir : sizeDirs) {
                    if (found) break;
                    for (const QString &context : contexts) {
                        if (found) break;
                        const QString bucket = QDir(themePath).filePath(
                            sizeDir + QLatin1Char('/') + context);
                        for (const QString &name : candidateNames) {
                            if (found) break;
                            if (tryFile(bucket + QLatin1Char('/') + name)) { found = true; break; }
                            for (const QString &ext : exts) {
                                if (tryFile(bucket + QLatin1Char('/') + name
                                            + QLatin1Char('.') + ext)) { found = true; break; }
                            }
                        }
                    }
                }
            }
        }

        if (!found) {
            for (const QString &pixmapBase : pixmapBases) {
                if (found) break;
                QDir pixmapDir(pixmapBase);
                if (!pixmapDir.exists()) continue;
                for (const QString &name : candidateNames) {
                    if (found) break;
                    if (tryFile(pixmapDir.filePath(name))) { found = true; break; }
                    for (const QString &ext : exts) {
                        if (tryFile(pixmapDir.filePath(name + QLatin1Char('.') + ext))) {
                            found = true; break;
                        }
                    }
                }
            }
        }
    }

    // Store result (hit or miss) under lock.
    {
        QMutexLocker lk(&cacheMutex);
        cache.insert(iconName, resolved);
    }
    return resolved;
}

qint64 nowMsUtc()
{
    return QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
}

QString monitorEventToString(GFileMonitorEvent type)
{
    switch (type) {
    case G_FILE_MONITOR_EVENT_CREATED: return QStringLiteral("created");
    case G_FILE_MONITOR_EVENT_DELETED: return QStringLiteral("deleted");
    case G_FILE_MONITOR_EVENT_CHANGED: return QStringLiteral("changed");
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: return QStringLiteral("changes_done");
    case G_FILE_MONITOR_EVENT_MOVED: return QStringLiteral("moved");
    case G_FILE_MONITOR_EVENT_RENAMED: return QStringLiteral("renamed");
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: return QStringLiteral("attribute_changed");
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT: return QStringLiteral("pre_unmount");
    case G_FILE_MONITOR_EVENT_UNMOUNTED: return QStringLiteral("unmounted");
    default: return QStringLiteral("other");
    }
}

qint64 sevenDaysMs()
{
    return 7LL * 24LL * 60LL * 60LL * 1000LL;
}

QString normalizedExecKey(const QString &exec)
{
    const QString trimmed = exec.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    QString firstToken = trimmed;
    const int spacePos = firstToken.indexOf(QLatin1Char(' '));
    if (spacePos > 0) {
        firstToken = firstToken.left(spacePos);
    }
    if (firstToken.startsWith(QLatin1Char('"')) && firstToken.endsWith(QLatin1Char('"')) && firstToken.size() > 1) {
        firstToken = firstToken.mid(1, firstToken.size() - 2);
    }
    if (firstToken.isEmpty()) {
        return QString();
    }
    return QFileInfo(firstToken).fileName().toLower();
}

QString launchHistoryPath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/SLM/DesktopShell");
    }
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(QStringLiteral("app-launch-history.json"));
}

QString xbelUsageCachePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/SLM/DesktopShell");
    }
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(QStringLiteral("app-xbel-usage-cache.json"));
}

QVariantMap defaultUsageMap()
{
    QVariantMap usage;
    usage.insert(QStringLiteral("launchCount7d"), 0);
    usage.insert(QStringLiteral("fileOpenCount7d"), 0);
    usage.insert(QStringLiteral("lastLaunchMs"), qint64(0));
    usage.insert(QStringLiteral("score"), 0);
    return usage;
}

void accumulateUsage(QHash<QString, QVariantMap> &byKey,
                     const QStringList &keys,
                     int launchDelta,
                     int fileOpenDelta,
                     qint64 lastLaunchMs)
{
    for (const QString &rawKey : keys) {
        const QString key = rawKey.trimmed().toLower();
        if (key.isEmpty()) {
            continue;
        }
        QVariantMap usage = byKey.value(key, defaultUsageMap());
        usage.insert(QStringLiteral("launchCount7d"), usage.value(QStringLiteral("launchCount7d")).toInt() + launchDelta);
        usage.insert(QStringLiteral("fileOpenCount7d"), usage.value(QStringLiteral("fileOpenCount7d")).toInt() + fileOpenDelta);
        usage.insert(QStringLiteral("lastLaunchMs"),
                     qMax(usage.value(QStringLiteral("lastLaunchMs")).toLongLong(), lastLaunchMs));
        byKey.insert(key, usage);
    }
}

int recencyBonus(qint64 lastLaunchMs)
{
    if (lastLaunchMs <= 0) {
        return 0;
    }
    const qint64 age = nowMsUtc() - lastLaunchMs;
    if (age <= 0) {
        return 80;
    }
    const qint64 dayMs = 24LL * 60LL * 60LL * 1000LL;
    if (age <= dayMs) {
        return 80;
    }
    if (age <= 3 * dayMs) {
        return 45;
    }
    if (age <= 7 * dayMs) {
        return 18;
    }
    return 0;
}

void mergeUsageMap(QHash<QString, QVariantMap> &target,
                   const QHash<QString, QVariantMap> &source)
{
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        const QString key = it.key().trimmed().toLower();
        if (key.isEmpty()) {
            continue;
        }
        const QVariantMap src = it.value();
        QVariantMap dst = target.value(key, defaultUsageMap());
        dst.insert(QStringLiteral("launchCount7d"),
                   dst.value(QStringLiteral("launchCount7d")).toInt()
                   + src.value(QStringLiteral("launchCount7d")).toInt());
        dst.insert(QStringLiteral("fileOpenCount7d"),
                   dst.value(QStringLiteral("fileOpenCount7d")).toInt()
                   + src.value(QStringLiteral("fileOpenCount7d")).toInt());
        dst.insert(QStringLiteral("lastLaunchMs"),
                   qMax(dst.value(QStringLiteral("lastLaunchMs")).toLongLong(),
                        src.value(QStringLiteral("lastLaunchMs")).toLongLong()));
        target.insert(key, dst);
    }
}

QHash<QString, QVariantMap> loadXbelUsageCache()
{
    QHash<QString, QVariantMap> out;
    QFile file(xbelUsageCachePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return out;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return out;
    }
    const QJsonObject root = doc.object();
    const QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
    for (auto it = usage.begin(); it != usage.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        const QString key = it.key().trimmed().toLower();
        if (key.isEmpty()) {
            continue;
        }
        const QJsonObject row = it.value().toObject();
        QVariantMap map = defaultUsageMap();
        map.insert(QStringLiteral("launchCount7d"), row.value(QStringLiteral("launchCount7d")).toInt());
        map.insert(QStringLiteral("fileOpenCount7d"), row.value(QStringLiteral("fileOpenCount7d")).toInt());
        map.insert(QStringLiteral("lastLaunchMs"), static_cast<qint64>(row.value(QStringLiteral("lastLaunchMs")).toDouble()));
        out.insert(key, map);
    }
    return out;
}

void saveXbelUsageCache(const QHash<QString, QVariantMap> &usage)
{
    QJsonObject usageObj;
    for (auto it = usage.constBegin(); it != usage.constEnd(); ++it) {
        const QVariantMap row = it.value();
        QJsonObject item;
        item.insert(QStringLiteral("launchCount7d"), row.value(QStringLiteral("launchCount7d")).toInt());
        item.insert(QStringLiteral("fileOpenCount7d"), row.value(QStringLiteral("fileOpenCount7d")).toInt());
        item.insert(QStringLiteral("lastLaunchMs"),
                    static_cast<double>(row.value(QStringLiteral("lastLaunchMs")).toLongLong()));
        usageObj.insert(it.key(), item);
    }
    QJsonObject root;
    root.insert(QStringLiteral("usage"), usageObj);
    root.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    QFile file(xbelUsageCachePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
} // namespace

DesktopAppModel::DesktopAppModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_refreshDebounceTimer = new QTimer(this);
    m_refreshDebounceTimer->setSingleShot(true);
    m_refreshDebounceTimer->setInterval(250);
    connect(m_refreshDebounceTimer, &QTimer::timeout, this, &DesktopAppModel::refresh);

    GAppInfoMonitor *monitor = g_app_info_monitor_get();
    if (monitor) {
        m_appInfoMonitor = g_object_ref(monitor);
        g_signal_connect(monitor, "changed", G_CALLBACK(&DesktopAppModel::onGAppInfoChanged), this);
    }
    setupDesktopDirMonitors();

    loadLaunchHistory();
    QTimer::singleShot(0, this, &DesktopAppModel::refresh);
}

DesktopAppModel::~DesktopAppModel()
{
    clearDesktopDirMonitors();
    if (m_appInfoMonitor) {
        g_signal_handlers_disconnect_by_data(m_appInfoMonitor, this);
        g_object_unref(m_appInfoMonitor);
        m_appInfoMonitor = nullptr;
    }
}

void DesktopAppModel::onGAppInfoChanged(GAppInfoMonitor *monitor, void *userData)
{
    Q_UNUSED(monitor);
    auto *self = static_cast<DesktopAppModel *>(userData);
    if (!self) {
        return;
    }
    qCInfo(lcAppModel) << "GAppInfoMonitor changed: scheduling DesktopAppModel refresh";
    QMetaObject::invokeMethod(self, [self]() {
        if (self->m_refreshDebounceTimer) {
            self->m_refreshDebounceTimer->start();
        } else {
            self->refresh();
        }
    }, Qt::QueuedConnection);
}

void DesktopAppModel::onDesktopDirChanged(GFileMonitor *monitor,
                                          GFile *file,
                                          GFile *otherFile,
                                          int eventType,
                                          void *userData)
{
    Q_UNUSED(monitor);
    Q_UNUSED(otherFile);
    auto *self = static_cast<DesktopAppModel *>(userData);
    if (!self) {
        return;
    }
    gchar *path = file ? g_file_get_path(file) : nullptr;
    const QString changedPath = fromUtf8(path);
    g_free(path);
    if (changedPath.isEmpty() || !changedPath.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        return;
    }
    qCInfo(lcAppModel) << "Desktop file monitor event"
                       << monitorEventToString(static_cast<GFileMonitorEvent>(eventType))
                       << changedPath;
    QMetaObject::invokeMethod(self, [self]() {
        if (self->m_refreshDebounceTimer) {
            self->m_refreshDebounceTimer->start();
        } else {
            self->refresh();
        }
    }, Qt::QueuedConnection);
}

void DesktopAppModel::setupDesktopDirMonitors()
{
    clearDesktopDirMonitors();

    QStringList roots;
    roots << QStringLiteral("/usr/share/applications");
    roots << QDir::home().filePath(QStringLiteral(".local/share/applications"));
    roots.removeDuplicates();

    for (const QString &root : roots) {
        if (!QFileInfo(root).isDir()) {
            continue;
        }
        GError *error = nullptr;
        GFile *dir = g_file_new_for_path(QFile::encodeName(root).constData());
        if (!dir) {
            continue;
        }
        GFileMonitor *monitor = g_file_monitor_directory(dir, G_FILE_MONITOR_NONE, nullptr, &error);
        g_object_unref(dir);
        if (error) {
            qCWarning(lcAppModel) << "Failed to monitor desktop directory" << root << ":" << fromUtf8(error->message);
            g_error_free(error);
            continue;
        }
        if (!monitor) {
            continue;
        }
        g_signal_connect(monitor, "changed", G_CALLBACK(&DesktopAppModel::onDesktopDirChanged), this);
        m_appDirMonitors.push_back(monitor);
        qCInfo(lcAppModel) << "Monitoring desktop directory:" << root;
    }
}

void DesktopAppModel::clearDesktopDirMonitors()
{
    for (void *ptr : m_appDirMonitors) {
        auto *monitor = static_cast<GFileMonitor *>(ptr);
        if (!monitor) {
            continue;
        }
        g_signal_handlers_disconnect_by_data(monitor, this);
        g_file_monitor_cancel(monitor);
        g_object_unref(monitor);
    }
    m_appDirMonitors.clear();
}

void DesktopAppModel::setExecutionGate(AppExecutionGate *gate)
{
    if (m_gate == gate) {
        return;
    }
    QObject *oldGateObj = reinterpret_cast<QObject *>(m_gate);
    if (oldGateObj) {
        disconnect(oldGateObj, nullptr, this, nullptr);
    }
    m_gate = gate;
    QObject *gateObj = reinterpret_cast<QObject *>(m_gate);
    if (gateObj) {
        connect(gateObj,
                SIGNAL(appExecutionRecorded(QString,QString,QString,QString,bool)),
                this,
                SLOT(onAppExecutionRecorded(QString,QString,QString,QString,bool)));
    }
}

void DesktopAppModel::setUIPreferences(UIPreferences *preferences)
{
    if (m_preferences == preferences) {
        return;
    }
    if (m_preferences) {
        disconnect(m_preferences, nullptr, this, nullptr);
    }
    m_preferences = preferences;
    if (m_preferences) {
        connect(m_preferences, &UIPreferences::preferenceChanged,
                this, &DesktopAppModel::onPreferenceChanged);
    }
    reloadScoringWeights();
    rebuildUsageStats();
}

void DesktopAppModel::setDesktopSettings(QObject *desktopSettings)
{
    if (m_desktopSettings == desktopSettings) {
        return;
    }
    if (m_desktopSettings) {
        disconnect(m_desktopSettings, nullptr, this, nullptr);
    }
    m_desktopSettings = desktopSettings;
    if (m_desktopSettings) {
        connect(m_desktopSettings, SIGNAL(settingChanged(QString)),
                this, SLOT(onDesktopSettingChanged(QString)));
    }
    reloadScoringWeights();
    rebuildUsageStats();
}

void DesktopAppModel::reloadScoringWeights()
{
    auto readIntSetting = [this](const QStringList &paths, int fallback) -> int {
        if (m_desktopSettings) {
            for (const QString &path : paths) {
                QVariant v;
                const bool okInvoke = QMetaObject::invokeMethod(m_desktopSettings, "settingValue",
                                                                 Q_RETURN_ARG(QVariant, v),
                                                                 Q_ARG(QString, path),
                                                                 Q_ARG(QVariant, QVariant()));
                if (!okInvoke) {
                    continue;
                }
                if (v.isValid() && !v.isNull()) {
                    bool ok = false;
                    const int out = v.toInt(&ok);
                    if (ok) {
                        return out;
                    }
                }
            }
        }
        if (m_preferences) {
            for (const QString &path : paths) {
                const QVariant v = m_preferences->getPreference(path, QVariant());
                if (v.isValid() && !v.isNull()) {
                    bool ok = false;
                    const int out = v.toInt(&ok);
                    if (ok) {
                        return out;
                    }
                }
            }
        }
        return fallback;
    };

    m_launchWeight = qBound(1, readIntSetting({QStringLiteral("app.score.launchWeight"),
                                               QStringLiteral("app.scoreLaunchWeight"),
                                               QStringLiteral("app/scoreLaunchWeight")}, 10), 50);
    m_fileOpenWeight = qBound(0, readIntSetting({QStringLiteral("app.score.fileOpenWeight"),
                                                 QStringLiteral("app.scoreFileOpenWeight"),
                                                 QStringLiteral("app/scoreFileOpenWeight")}, 3), 50);
    m_recencyWeight = qBound(0, readIntSetting({QStringLiteral("app.score.recencyWeight"),
                                                QStringLiteral("app.scoreRecencyWeight"),
                                                QStringLiteral("app/scoreRecencyWeight")}, 1), 20);
}

int DesktopAppModel::effectiveScore(int launchCount, int fileOpenCount, qint64 lastLaunchMs) const
{
    return (launchCount * m_launchWeight) +
           (fileOpenCount * m_fileOpenWeight) +
           (recencyBonus(lastLaunchMs) * m_recencyWeight);
}

void DesktopAppModel::onAppExecutionRecorded(QString source, QString name, QString desktopFile, QString executable, bool success)
{
    Q_UNUSED(source);
    if (!success) {
        return;
    }
    noteLaunchEvent(name, desktopFile, executable);
}

void DesktopAppModel::onPreferenceChanged(QString key, QVariant value)
{
    Q_UNUSED(value);
    const QString k = key.trimmed().toLower();
    if (k != QStringLiteral("app/scorelaunchweight") &&
        k != QStringLiteral("app/scorefileopenweight") &&
        k != QStringLiteral("app/scorerecencyweight")) {
        return;
    }
    reloadScoringWeights();
    rebuildUsageStats();
}

void DesktopAppModel::onDesktopSettingChanged(QString path)
{
    const QString k = path.trimmed().toLower();
    if (k != QStringLiteral("app.scorelaunchweight") &&
        k != QStringLiteral("app.score.launchweight") &&
        k != QStringLiteral("app/scorelaunchweight") &&
        k != QStringLiteral("app.scorefileopenweight") &&
        k != QStringLiteral("app.score.fileopenweight") &&
        k != QStringLiteral("app/scorefileopenweight") &&
        k != QStringLiteral("app.scorerecencyweight") &&
        k != QStringLiteral("app.score.recencyweight") &&
        k != QStringLiteral("app/scorerecencyweight")) {
        return;
    }
    reloadScoringWeights();
    rebuildUsageStats();
}

int DesktopAppModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_apps.size();
}

QVariant DesktopAppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_apps.size()) {
        return QVariant();
    }

    const DesktopAppEntry &app = m_apps.at(index.row());
    switch (role) {
    case NameRole:
        return app.name;
    case IconSourceRole:
        return app.iconSource;
    case IconNameRole:
        return app.iconName;
    case DesktopIdRole:
        return app.desktopId;
    case DesktopFileRole:
        return app.desktopFile;
    case ExecutableRole:
        return app.executable;
    case ScoreRole:
        return app.score;
    case LaunchCount7dRole:
        return app.launchCount7d;
    case FileOpenCount7dRole:
        return app.fileOpenCount7d;
    case LastLaunchRole:
        return app.lastLaunchIso;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> DesktopAppModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[IconSourceRole] = "iconSource";
    roles[IconNameRole] = "iconName";
    roles[DesktopIdRole] = "desktopId";
    roles[DesktopFileRole] = "desktopFile";
    roles[ExecutableRole] = "executable";
    roles[ScoreRole] = "score";
    roles[LaunchCount7dRole] = "launchCount7d";
    roles[FileOpenCount7dRole] = "fileOpenCount7d";
    roles[LastLaunchRole] = "lastLaunch";
    return roles;
}

void DesktopAppModel::refresh()
{
    qCDebug(lcAppModel) << "DesktopAppModel refresh begin";
    QVector<DesktopAppEntry> newApps;
    QSet<QString> seenKeys;

    const auto appendFromAppInfo = [&](GAppInfo *info, bool enforceShouldShow) {
        if (!info) {
            return;
        }
        if (enforceShouldShow && !g_app_info_should_show(info)) {
            return;
        }
        if (!enforceShouldShow && G_IS_DESKTOP_APP_INFO(info)) {
            GDesktopAppInfo *desktopInfo = G_DESKTOP_APP_INFO(info);
            if (g_desktop_app_info_get_is_hidden(desktopInfo)
                || g_desktop_app_info_get_nodisplay(desktopInfo)) {
                return;
            }
        }

        DesktopAppEntry app;
        app.name = fromUtf8(g_app_info_get_display_name(info));
        if (app.name.trimmed().isEmpty()) {
            return;
        }

        app.desktopId = fromUtf8(g_app_info_get_id(info));
        app.executable = fromUtf8(g_app_info_get_executable(info));

        if (G_IS_DESKTOP_APP_INFO(info)) {
            app.desktopFile = fromUtf8(g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(info)));
            if (app.iconName.isEmpty()) {
                app.iconName = fromUtf8(g_desktop_app_info_get_string(G_DESKTOP_APP_INFO(info), "Icon"));
            }
        }

        GIcon *icon = g_app_info_get_icon(info);
        app.iconSource = iconFilePathFromGIcon(icon);
        if (app.iconName.isEmpty()) {
            app.iconName = iconNameFromGIcon(icon);
        }
        if (app.iconSource.isEmpty()) {
            app.iconSource = resolveThemedIconPath(app.iconName);
        }

        const QString dedupeKey = !app.desktopId.isEmpty()
                                      ? app.desktopId.toLower()
                                      : app.name.toLower();
        if (seenKeys.contains(dedupeKey)) {
            return;
        }
        seenKeys.insert(dedupeKey);
        newApps.push_back(app);
    };

    GList *infos = g_app_info_get_all();
    for (GList *it = infos; it != nullptr; it = it->next) {
        appendFromAppInfo(G_APP_INFO(it->data), true);
    }
    g_list_free_full(infos, g_object_unref);

    // Fallback scan for local desktop entries to avoid delayed registry propagation.
    const QString localAppsDir = QDir::home().filePath(QStringLiteral(".local/share/applications"));
    QDirIterator localIt(localAppsDir, QStringList{QStringLiteral("*.desktop")}, QDir::Files);
    while (localIt.hasNext()) {
        const QString desktopPath = localIt.next();
        GDesktopAppInfo *desktopInfo = g_desktop_app_info_new_from_filename(
            QFile::encodeName(desktopPath).constData());
        if (!desktopInfo) {
            // Some test desktop entries may not be accepted by GDesktopAppInfo
            // (e.g. Exec points to a command not currently resolvable in PATH).
            // Keep them discoverable in Launchpad via a permissive keyfile parse.
            GKeyFile *kf = g_key_file_new();
            GError *loadError = nullptr;
            const gboolean loaded = g_key_file_load_from_file(
                kf, QFile::encodeName(desktopPath).constData(), G_KEY_FILE_NONE, &loadError);
            if (loadError) {
                g_error_free(loadError);
                g_key_file_free(kf);
                continue;
            }
            if (!loaded) {
                g_key_file_free(kf);
                continue;
            }

            const QString type = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Type", nullptr)).trimmed();
            const bool hidden = g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", nullptr);
            const bool noDisplay = g_key_file_get_boolean(kf, "Desktop Entry", "NoDisplay", nullptr);
            const QString name = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Name", nullptr)).trimmed();
            const QString execRaw = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Exec", nullptr)).trimmed();
            const QString iconName = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Icon", nullptr)).trimmed();
            g_key_file_free(kf);

            if (type.compare(QStringLiteral("Application"), Qt::CaseInsensitive) != 0
                || hidden || noDisplay || name.isEmpty()) {
                continue;
            }

            DesktopAppEntry app;
            app.name = name;
            app.desktopId = QFileInfo(desktopPath).fileName();
            app.desktopFile = desktopPath;
            app.executable = execRaw;
            app.iconName = iconName;
            app.iconSource = resolveThemedIconPath(iconName);
            const QString dedupeKey = !app.desktopId.isEmpty()
                                          ? app.desktopId.toLower()
                                          : app.name.toLower();
            if (!seenKeys.contains(dedupeKey)) {
                seenKeys.insert(dedupeKey);
                newApps.push_back(app);
            }
            continue;
        }
        appendFromAppInfo(G_APP_INFO(desktopInfo), false);
        g_object_unref(desktopInfo);
    }

    beginResetModel();
    m_apps = std::move(newApps);
    endResetModel();

    qCInfo(lcAppModel) << "DesktopAppModel refresh done, app count =" << m_apps.size();
    rebuildUsageStats();
}

// Static: all heavy GIO + filesystem work. Safe to call from any thread.
// Does NOT touch any DesktopAppModel members.
QVector<DesktopAppEntry> DesktopAppModel::computeAppsFromSystem()
{
    QVector<DesktopAppEntry> newApps;
    QSet<QString> seenKeys;

    const auto appendFromAppInfo = [&](GAppInfo *info, bool enforceShouldShow) {
        if (!info) return;
        if (enforceShouldShow && !g_app_info_should_show(info)) return;
        if (!enforceShouldShow && G_IS_DESKTOP_APP_INFO(info)) {
            GDesktopAppInfo *di = G_DESKTOP_APP_INFO(info);
            if (g_desktop_app_info_get_is_hidden(di) || g_desktop_app_info_get_nodisplay(di))
                return;
        }

        DesktopAppEntry app;
        app.name = fromUtf8(g_app_info_get_display_name(info));
        if (app.name.trimmed().isEmpty()) return;

        app.desktopId  = fromUtf8(g_app_info_get_id(info));
        app.executable = fromUtf8(g_app_info_get_executable(info));

        if (G_IS_DESKTOP_APP_INFO(info)) {
            app.desktopFile = fromUtf8(g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(info)));
            if (app.iconName.isEmpty())
                app.iconName = fromUtf8(g_desktop_app_info_get_string(G_DESKTOP_APP_INFO(info), "Icon"));
        }

        GIcon *icon = g_app_info_get_icon(info);
        app.iconSource = iconFilePathFromGIcon(icon);
        if (app.iconName.isEmpty())  app.iconName   = iconNameFromGIcon(icon);
        if (app.iconSource.isEmpty()) app.iconSource = resolveThemedIconPath(app.iconName);

        const QString dedupeKey = !app.desktopId.isEmpty()
                                      ? app.desktopId.toLower() : app.name.toLower();
        if (seenKeys.contains(dedupeKey)) return;
        seenKeys.insert(dedupeKey);
        newApps.push_back(app);
    };

    GList *infos = g_app_info_get_all();
    for (GList *it = infos; it != nullptr; it = it->next)
        appendFromAppInfo(G_APP_INFO(it->data), true);
    g_list_free_full(infos, g_object_unref);

    // Fallback: local desktop entries.
    const QString localAppsDir = QDir::home().filePath(QStringLiteral(".local/share/applications"));
    QDirIterator localIt(localAppsDir, QStringList{QStringLiteral("*.desktop")}, QDir::Files);
    while (localIt.hasNext()) {
        const QString desktopPath = localIt.next();
        GDesktopAppInfo *desktopInfo = g_desktop_app_info_new_from_filename(
            QFile::encodeName(desktopPath).constData());
        if (!desktopInfo) {
            GKeyFile *kf = g_key_file_new();
            GError *loadError = nullptr;
            const gboolean loaded = g_key_file_load_from_file(
                kf, QFile::encodeName(desktopPath).constData(), G_KEY_FILE_NONE, &loadError);
            if (loadError) g_error_free(loadError);
            if (!loaded) { g_key_file_free(kf); continue; }

            const QString type     = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Type",      nullptr)).trimmed();
            const bool hidden      = g_key_file_get_boolean(kf, "Desktop Entry", "Hidden",    nullptr);
            const bool noDisplay   = g_key_file_get_boolean(kf, "Desktop Entry", "NoDisplay", nullptr);
            const QString name     = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Name",      nullptr)).trimmed();
            const QString execRaw  = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Exec",      nullptr)).trimmed();
            const QString iconName = fromUtf8(g_key_file_get_string(kf, "Desktop Entry", "Icon",      nullptr)).trimmed();
            g_key_file_free(kf);

            if (type.compare(QStringLiteral("Application"), Qt::CaseInsensitive) != 0
                || hidden || noDisplay || name.isEmpty())
                continue;

            DesktopAppEntry app;
            app.name       = name;
            app.desktopId  = QFileInfo(desktopPath).fileName();
            app.desktopFile = desktopPath;
            app.executable = execRaw;
            app.iconName   = iconName;
            app.iconSource = resolveThemedIconPath(iconName);
            const QString dedupeKey = !app.desktopId.isEmpty()
                                          ? app.desktopId.toLower() : app.name.toLower();
            if (!seenKeys.contains(dedupeKey)) {
                seenKeys.insert(dedupeKey);
                newApps.push_back(app);
            }
            continue;
        }
        appendFromAppInfo(G_APP_INFO(desktopInfo), false);
        g_object_unref(desktopInfo);
    }

    return newApps;
}

void DesktopAppModel::refreshAsync()
{
    if (m_refreshRunning) {
        qCDebug(lcAppModel) << "refreshAsync: already running, skipped";
        return;
    }
    m_refreshRunning = true;
    qCDebug(lcAppModel) << "refreshAsync: starting background scan";

    auto *watcher = new QFutureWatcher<QVector<DesktopAppEntry>>(this);
    connect(watcher, &QFutureWatcher<QVector<DesktopAppEntry>>::finished, this,
            [this, watcher]() {
                QVector<DesktopAppEntry> newApps = watcher->result();
                watcher->deleteLater();
                m_refreshRunning = false;
                qCInfo(lcAppModel) << "refreshAsync: scan done, app count =" << newApps.size();
                beginResetModel();
                m_apps = std::move(newApps);
                endResetModel();
                rebuildUsageStats();
            });
    watcher->setFuture(QtConcurrent::run(&DesktopAppModel::computeAppsFromSystem));
}

QString DesktopAppModel::primaryKey(const QString &desktopId,
                                    const QString &desktopFile,
                                    const QString &executable,
                                    const QString &name)
{
    const QString id = desktopId.trimmed().toLower();
    if (!id.isEmpty()) {
        return id;
    }
    const QString desktopBase = QFileInfo(desktopFile.trimmed()).fileName().toLower();
    if (!desktopBase.isEmpty()) {
        return desktopBase;
    }
    const QString execKey = normalizedExecKey(executable);
    if (!execKey.isEmpty()) {
        return execKey;
    }
    return name.trimmed().toLower();
}

QStringList DesktopAppModel::keysFromApp(const DesktopAppEntry &app)
{
    QStringList keys;
    const QString id = app.desktopId.trimmed().toLower();
    if (!id.isEmpty()) {
        keys << id;
    }
    const QString desktopBase = QFileInfo(app.desktopFile.trimmed()).fileName().toLower();
    if (!desktopBase.isEmpty() && !keys.contains(desktopBase)) {
        keys << desktopBase;
    }
    const QString execKey = normalizedExecKey(app.executable);
    if (!execKey.isEmpty() && !keys.contains(execKey)) {
        keys << execKey;
    }
    const QString appName = app.name.trimmed().toLower();
    if (!appName.isEmpty() && !keys.contains(appName)) {
        keys << appName;
    }
    return keys;
}

QStringList DesktopAppModel::keysFromUsageRecord(const QString &appName, const QString &appExec)
{
    QStringList keys;
    const QString nameKey = appName.trimmed().toLower();
    if (!nameKey.isEmpty()) {
        keys << nameKey;
    }
    const QString execRaw = appExec.trimmed().toLower();
    if (!execRaw.isEmpty() && !keys.contains(execRaw)) {
        keys << execRaw;
    }
    const QString execFile = normalizedExecKey(appExec);
    if (!execFile.isEmpty() && !keys.contains(execFile)) {
        keys << execFile;
    }
    return keys;
}

void DesktopAppModel::pruneOldLaunches()
{
    const qint64 cutoff = nowMsUtc() - sevenDaysMs();
    for (auto it = m_launchHistoryByKey.begin(); it != m_launchHistoryByKey.end();) {
        QVariantList kept;
        const QVariantList existing = it.value();
        for (const QVariant &v : existing) {
            const qint64 ts = v.toLongLong();
            if (ts >= cutoff) {
                kept.push_back(ts);
            }
        }
        if (kept.isEmpty()) {
            it = m_launchHistoryByKey.erase(it);
            continue;
        }
        it.value() = kept;
        ++it;
    }
}

void DesktopAppModel::loadLaunchHistory()
{
    m_launchHistoryByKey.clear();
    QFile file(launchHistoryPath());
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonObject launches = root.value(QStringLiteral("launches")).toObject();
    for (auto it = launches.begin(); it != launches.end(); ++it) {
        const QString key = it.key().trimmed().toLower();
        if (key.isEmpty() || !it.value().isArray()) {
            continue;
        }
        QVariantList values;
        const QJsonArray arr = it.value().toArray();
        for (const QJsonValue &v : arr) {
            const qint64 ts = static_cast<qint64>(v.toDouble());
            if (ts > 0) {
                values.push_back(ts);
            }
        }
        if (!values.isEmpty()) {
            m_launchHistoryByKey.insert(key, values);
        }
    }
    pruneOldLaunches();
}

void DesktopAppModel::saveLaunchHistory() const
{
    QJsonObject launches;
    for (auto it = m_launchHistoryByKey.constBegin(); it != m_launchHistoryByKey.constEnd(); ++it) {
        QJsonArray arr;
        const QVariantList list = it.value();
        for (const QVariant &v : list) {
            const qint64 ts = v.toLongLong();
            if (ts > 0) {
                arr.push_back(static_cast<double>(ts));
            }
        }
        if (!arr.isEmpty()) {
            launches.insert(it.key(), arr);
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("launches"), launches);
    root.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QFile file(launchHistoryPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void DesktopAppModel::noteLaunchEvent(const QString &name, const QString &desktopFile, const QString &executable)
{
    const QString key = primaryKey(QString(), desktopFile, executable, name);
    if (key.isEmpty()) {
        return;
    }
    pruneOldLaunches();
    QVariantList list = m_launchHistoryByKey.value(key);
    list.push_back(nowMsUtc());
    m_launchHistoryByKey.insert(key, list);
    saveLaunchHistory();
    rebuildUsageStats();
}

void DesktopAppModel::applyUsageToApps()
{
    for (DesktopAppEntry &app : m_apps) {
        app.score = 0;
        app.launchCount7d = 0;
        app.fileOpenCount7d = 0;
        app.lastLaunchIso.clear();

        int launchCount = 0;
        int fileOpenCount = 0;
        qint64 lastLaunch = 0;

        const QStringList keys = keysFromApp(app);
        for (const QString &key : keys) {
            const QVariantMap usage = m_usageStatsByKey.value(key.trimmed().toLower());
            if (usage.isEmpty()) {
                continue;
            }
            launchCount = qMax(launchCount, usage.value(QStringLiteral("launchCount7d")).toInt());
            fileOpenCount = qMax(fileOpenCount, usage.value(QStringLiteral("fileOpenCount7d")).toInt());
            lastLaunch = qMax(lastLaunch, usage.value(QStringLiteral("lastLaunchMs")).toLongLong());
        }

        const int score = effectiveScore(launchCount, fileOpenCount, lastLaunch);
        app.launchCount7d = launchCount;
        app.fileOpenCount7d = fileOpenCount;
        app.score = score;
        if (lastLaunch > 0) {
            app.lastLaunchIso = QDateTime::fromMSecsSinceEpoch(lastLaunch, QTimeZone::utc())
                                    .toString(Qt::ISODateWithMs);
        }
    }

    std::sort(m_apps.begin(), m_apps.end(), [](const DesktopAppEntry &a, const DesktopAppEntry &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        const qint64 aMs = QDateTime::fromString(a.lastLaunchIso, Qt::ISODate).toMSecsSinceEpoch();
        const qint64 bMs = QDateTime::fromString(b.lastLaunchIso, Qt::ISODate).toMSecsSinceEpoch();
        if (aMs != bMs) {
            return aMs > bMs;
        }
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
}

void DesktopAppModel::rebuildUsageStats()
{
    pruneOldLaunches();
    m_usageStatsByKey.clear();

    const qint64 cutoffMs = nowMsUtc() - sevenDaysMs();

    for (auto it = m_launchHistoryByKey.constBegin(); it != m_launchHistoryByKey.constEnd(); ++it) {
        int launchCount = 0;
        qint64 lastLaunch = 0;
        const QVariantList tsList = it.value();
        for (const QVariant &v : tsList) {
            const qint64 ts = v.toLongLong();
            if (ts < cutoffMs) {
                continue;
            }
            ++launchCount;
            lastLaunch = qMax(lastLaunch, ts);
        }
        if (launchCount <= 0) {
            continue;
        }
        QVariantMap usage = m_usageStatsByKey.value(it.key(), defaultUsageMap());
        usage.insert(QStringLiteral("launchCount7d"),
                     usage.value(QStringLiteral("launchCount7d")).toInt() + launchCount);
        usage.insert(QStringLiteral("lastLaunchMs"),
                     qMax(usage.value(QStringLiteral("lastLaunchMs")).toLongLong(), lastLaunch));
        m_usageStatsByKey.insert(it.key(), usage);
    }

    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString xbelPath = dataHome.isEmpty()
            ? (QDir::homePath() + QStringLiteral("/.local/share/recently-used.xbel"))
            : QDir(dataHome).filePath(QStringLiteral("recently-used.xbel"));

    bool xbelLoadOk = false;
    QHash<QString, QVariantMap> xbelUsageCurrent;
    GBookmarkFile *bookmark = g_bookmark_file_new();
    if (bookmark) {
        GError *loadError = nullptr;
        const QByteArray nativePath = QFile::encodeName(xbelPath);
        const gboolean loaded = g_bookmark_file_load_from_file(bookmark, nativePath.constData(), &loadError);
        if (loadError) {
            g_error_free(loadError);
        }
        if (loaded) {
            xbelLoadOk = true;
            gsize uriCount = 0;
            gchar **uris = g_bookmark_file_get_uris(bookmark, &uriCount);
            if (uris) {
                for (gsize i = 0; i < uriCount; ++i) {
                    const gchar *uri = uris[i];
                    if (!uri || !*uri) {
                        continue;
                    }

                    GError *uriErr = nullptr;
                    GDateTime *modifiedDt = g_bookmark_file_get_modified_date_time(bookmark, uri, &uriErr);
                    if (uriErr) {
                        g_error_free(uriErr);
                        uriErr = nullptr;
                    }
                    const qint64 modifiedMs = modifiedDt
                            ? (static_cast<qint64>(g_date_time_to_unix(modifiedDt)) * 1000LL)
                            : 0;
                    if (modifiedMs > 0 && modifiedMs < cutoffMs) {
                        continue;
                    }

                    gsize appCount = 0;
                    GError *appsErr = nullptr;
                    gchar **apps = g_bookmark_file_get_applications(bookmark, uri, &appCount, &appsErr);
                    if (appsErr) {
                        g_error_free(appsErr);
                        appsErr = nullptr;
                    }
                    if (!apps) {
                        continue;
                    }

                    for (gsize j = 0; j < appCount; ++j) {
                        const gchar *appNameRaw = apps[j];
                        if (!appNameRaw || !*appNameRaw) {
                            continue;
                        }
                        gchar *appExec = nullptr;
                        guint appCountValue = 0;
                        GDateTime *appStampDt = nullptr;
                        GError *appInfoErr = nullptr;
                        const gboolean hasInfo = g_bookmark_file_get_application_info(
                            bookmark, uri, appNameRaw, &appExec, &appCountValue, &appStampDt, &appInfoErr);
                        if (appInfoErr) {
                            g_error_free(appInfoErr);
                            appInfoErr = nullptr;
                        }
                        if (!hasInfo) {
                            g_free(appExec);
                            continue;
                        }
                        const qint64 appStampMs = appStampDt
                                ? (static_cast<qint64>(g_date_time_to_unix(appStampDt)) * 1000LL)
                                : modifiedMs;
                        if (appStampMs > 0 && appStampMs < cutoffMs) {
                            g_free(appExec);
                            continue;
                        }
                        const QString appName = fromUtf8(appNameRaw);
                        const QString appExecStr = fromUtf8(appExec);
                        const QStringList keys = keysFromUsageRecord(appName, appExecStr);
                        const int launches = qMax(1, static_cast<int>(appCountValue));
                        accumulateUsage(m_usageStatsByKey, keys, launches, launches, appStampMs);
                        accumulateUsage(xbelUsageCurrent, keys, launches, launches, appStampMs);
                        g_free(appExec);
                    }
                    g_strfreev(apps);
                }
                g_strfreev(uris);
            }
        }
        g_bookmark_file_free(bookmark);
    }

    if (xbelLoadOk) {
        saveXbelUsageCache(xbelUsageCurrent);
    } else {
        // Fallback to last good 7-day snapshot to avoid score collapse when XBEL is malformed/unreadable.
        mergeUsageMap(m_usageStatsByKey, loadXbelUsageCache());
    }

    beginResetModel();
    applyUsageToApps();
    endResetModel();
    emit appScoresChanged();
}

int DesktopAppModel::countMatching(const QString &searchText) const
{
    const QString needle = searchText.trimmed();
    if (needle.isEmpty()) {
        return m_apps.size();
    }

    int count = 0;
    for (const DesktopAppEntry &app : m_apps) {
        if (app.name.contains(needle, Qt::CaseInsensitive)) {
            ++count;
        }
    }
    return count;
}

QVariantList DesktopAppModel::page(int pageIndex, int pageSize, const QString &searchText) const
{
    QVariantList result;
    if (pageIndex < 0 || pageSize <= 0) {
        return result;
    }

    const QString needle = searchText.trimmed();
    QVector<const DesktopAppEntry *> filtered;
    filtered.reserve(m_apps.size());
    for (const DesktopAppEntry &app : m_apps) {
        if (needle.isEmpty() || app.name.contains(needle, Qt::CaseInsensitive)) {
            filtered.push_back(&app);
        }
    }

    const int start = pageIndex * pageSize;
    if (start >= filtered.size()) {
        return result;
    }

    const int end = std::min(start + pageSize, static_cast<int>(filtered.size()));
    for (int i = start; i < end; ++i) {
        const DesktopAppEntry &app = *filtered.at(i);
        QVariantMap item;
        item.insert(QStringLiteral("name"), app.name);
        item.insert(QStringLiteral("iconSource"), app.iconSource);
        item.insert(QStringLiteral("iconName"), app.iconName);
        item.insert(QStringLiteral("desktopId"), app.desktopId);
        item.insert(QStringLiteral("desktopFile"), app.desktopFile);
        item.insert(QStringLiteral("executable"), app.executable);
        item.insert(QStringLiteral("score"), app.score);
        item.insert(QStringLiteral("launchCount7d"), app.launchCount7d);
        item.insert(QStringLiteral("fileOpenCount7d"), app.fileOpenCount7d);
        item.insert(QStringLiteral("lastLaunch"), app.lastLaunchIso);
        result.push_back(item);
    }
    return result;
}

QVariantMap DesktopAppModel::appUsage(const QString &desktopId,
                                      const QString &desktopFile,
                                      const QString &executable) const
{
    const QString key = primaryKey(desktopId, desktopFile, executable, QString());
    const QVariantMap usage = m_usageStatsByKey.value(key, defaultUsageMap());
    QVariantMap out;
    out.insert(QStringLiteral("key"), key);
    const int launchCount = usage.value(QStringLiteral("launchCount7d")).toInt();
    const int fileOpenCount = usage.value(QStringLiteral("fileOpenCount7d")).toInt();
    out.insert(QStringLiteral("launchCount7d"), launchCount);
    out.insert(QStringLiteral("fileOpenCount7d"), fileOpenCount);
    const qint64 lastLaunchMs = usage.value(QStringLiteral("lastLaunchMs")).toLongLong();
    out.insert(QStringLiteral("score"), effectiveScore(launchCount, fileOpenCount, lastLaunchMs));
    out.insert(QStringLiteral("lastLaunch"), lastLaunchMs > 0
               ? QDateTime::fromMSecsSinceEpoch(lastLaunchMs, QTimeZone::utc())
                     .toString(Qt::ISODateWithMs)
               : QString());
    return out;
}

QVariantList DesktopAppModel::frequentApps(int limit) const
{
    QVariantList out;
    const int maxItems = (limit > 0) ? limit : 24;
    for (int i = 0; i < m_apps.size() && i < maxItems; ++i) {
        const DesktopAppEntry &app = m_apps.at(i);
        QVariantMap item;
        item.insert(QStringLiteral("name"), app.name);
        item.insert(QStringLiteral("iconSource"), app.iconSource);
        item.insert(QStringLiteral("iconName"), app.iconName);
        item.insert(QStringLiteral("desktopId"), app.desktopId);
        item.insert(QStringLiteral("desktopFile"), app.desktopFile);
        item.insert(QStringLiteral("executable"), app.executable);
        item.insert(QStringLiteral("score"), app.score);
        item.insert(QStringLiteral("launchCount7d"), app.launchCount7d);
        item.insert(QStringLiteral("fileOpenCount7d"), app.fileOpenCount7d);
        item.insert(QStringLiteral("lastLaunch"), app.lastLaunchIso);
        out.push_back(item);
    }
    return out;
}

QVariantList DesktopAppModel::topApps(int limit) const
{
    return frequentApps(limit);
}

QVariantList DesktopAppModel::slmQuickActions(const QString &scope) const
{
    return slmQuickActionsForEntry(scope, {});
}

QVariantList DesktopAppModel::slmQuickActionsForEntry(const QString &scope,
                                                      const QVariantMap &entry) const
{
    const auto localFallback = [&]() -> QVariantList {
        if (entry.isEmpty()) {
            return {};
        }
        return localQuickActionsFromDesktopEntry(entry, scope);
    };
#ifdef QT_DBUS_LIB
    QVariantMap context{
        {QStringLiteral("scope"), scope.trimmed().toLower()},
        {QStringLiteral("selection_count"), 0},
        {QStringLiteral("source_app"), QStringLiteral("org.slm.desktop")},
    };
    const QString desktopId = entry.value(QStringLiteral("desktopId")).toString().trimmed();
    const QString desktopFile = entry.value(QStringLiteral("desktopFile")).toString().trimmed();
    const QString executable = entry.value(QStringLiteral("executable")).toString().trimmed();
    if (!desktopId.isEmpty()) {
        context.insert(QStringLiteral("desktop_id"), desktopId);
    }
    if (!desktopFile.isEmpty()) {
        context.insert(QStringLiteral("desktop_file"), desktopFile);
    }
    if (!executable.isEmpty()) {
        context.insert(QStringLiteral("executable"), executable);
    }

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return localFallback();
    }
    QDBusReply<QVariantList> reply = iface.call(QStringLiteral("ListActionsWithContext"),
                                                QStringLiteral("QuickAction"),
                                                context);
    if (!reply.isValid()) {
        reply = iface.call(QStringLiteral("ListActions"),
                           QStringLiteral("QuickAction"),
                           context);
    }
    if (!reply.isValid()) {
        return localFallback();
    }

    const QVariantList source = reply.value();
    auto collectRows = [&](bool strictMatch) {
        QVariantList out;
        out.reserve(source.size());
        QSet<QString> seenIds;
        for (const QVariant &rowVar : source) {
            QVariantMap row = rowVar.toMap();
            const QString actionId = row.value(QStringLiteral("id")).toString().trimmed();
            if (actionId.isEmpty() || seenIds.contains(actionId)) {
                continue;
            }
            if (strictMatch && !entry.isEmpty() && !quickActionMatchesEntry(row, entry)) {
                continue;
            }
            if (row.value(QStringLiteral("icon")).toString().trimmed().isEmpty()) {
                const QString fallbackIconName = entry.value(QStringLiteral("iconName")).toString().trimmed();
                const QString fallbackIconSource = entry.value(QStringLiteral("iconSource")).toString().trimmed();
                if (!fallbackIconName.isEmpty()) {
                    row.insert(QStringLiteral("icon"), fallbackIconName);
                }
                if (!fallbackIconSource.isEmpty()) {
                    row.insert(QStringLiteral("iconSource"), fallbackIconSource);
                }
            }
            out.push_back(row);
            seenIds.insert(actionId);
        }
        return out;
    };

    QVariantList strict = collectRows(true);
    if (!strict.isEmpty() || entry.isEmpty()) {
        return strict;
    }
    // Fallback: do not hide quick actions entirely when provider metadata is incomplete.
    QVariantList relaxed = collectRows(false);
    if (!relaxed.isEmpty()) {
        return relaxed;
    }
    return localFallback();
#else
    return localFallback();
#endif
}

QVariantMap DesktopAppModel::invokeSlmQuickAction(const QString &actionId,
                                                  const QVariantMap &context)
{
    const QString id = actionId.trimmed();
    if (id.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-action-id")},
        };
    }

    QString localDesktopFile;
    QString localGroupSuffix;
    if (parseLocalQuickActionId(id, &localDesktopFile, &localGroupSuffix)) {
        GKeyFile *kf = g_key_file_new();
        GError *err = nullptr;
        const gboolean loaded = g_key_file_load_from_file(
            kf, QFile::encodeName(localDesktopFile).constData(), G_KEY_FILE_NONE, &err);
        if (err) {
            g_error_free(err);
        }
        if (!loaded) {
            g_key_file_free(kf);
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("desktop-entry-not-found")},
            };
        }

        const QString groupName = QStringLiteral("Desktop Action %1").arg(localGroupSuffix);
        const QByteArray groupUtf8 = groupName.toUtf8();
        const QString execTemplate = fromUtf8(g_key_file_get_string(
            kf, groupUtf8.constData(), "Exec", nullptr)).trimmed();
        g_key_file_free(kf);

        if (execTemplate.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("missing-exec")},
            };
        }

        const QString cmd = fillExecNoUris(execTemplate);
        if (cmd.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("invalid-exec")},
            };
        }

        qint64 pid = -1;
        const bool ok = QProcess::startDetached(QStringLiteral("/bin/bash"),
                                                {QStringLiteral("-lc"), cmd},
                                                QString(),
                                                &pid);
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("id"), id},
            {QStringLiteral("mode"), QStringLiteral("local")},
            {QStringLiteral("pid"), pid},
        };
    }
#ifdef QT_DBUS_LIB
    QVariantMap invokeContext = context;
    if (!invokeContext.contains(QStringLiteral("scope"))) {
        invokeContext.insert(QStringLiteral("scope"), QStringLiteral("launcher"));
    }
    if (!invokeContext.contains(QStringLiteral("selection_count"))) {
        invokeContext.insert(QStringLiteral("selection_count"), 0);
    }

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("capability-service-unavailable")},
        };
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("InvokeActionWithContext"),
                                               id,
                                               invokeContext);
    if (!reply.isValid()) {
        reply = iface.call(QStringLiteral("InvokeAction"), id, invokeContext);
    }
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("dbus-error")},
        };
    }
    return reply.value();
#else
    Q_UNUSED(context);
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("dbus-unavailable")},
    };
#endif
}
