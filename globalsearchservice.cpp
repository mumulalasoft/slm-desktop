#include "globalsearchservice.h"
#include "urlutils.h"

#include "appmodel.h"
#include "dbuslogutils.h"
#include "src/services/clipboard/ClipboardSearchProvider.h"
#include "src/core/prefs/uipreferences.h"
#include "src/core/workspace/workspacemanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QTimeZone>
#include <algorithm>

#ifdef signals
#undef signals
#define DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif
extern "C" {
#include <gio/gio.h>
}
#ifdef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#define signals Q_SIGNALS
#undef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif

namespace {
constexpr const char kService[] = "org.slm.Desktop.Search.v1";
constexpr const char kPath[] = "/org/slm/Desktop/Search";
constexpr const char kApiVersion[] = "1.0";
constexpr qint64 kTrackerCacheTtlMs = 2500;
constexpr const char kCapabilityService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kCapabilityPath[] = "/org/freedesktop/SLMCapabilities";
constexpr const char kCapabilityIface[] = "org.freedesktop.SLMCapabilities";

QString normalizedSearchProfile(const QVariantMap &options)
{
    const QString raw = options.value(QStringLiteral("searchProfile"),
                                      options.value(QStringLiteral("profile"))).toString().trimmed().toLower();
    if (raw == QStringLiteral("apps-first") ||
        raw == QStringLiteral("files-first") ||
        raw == QStringLiteral("balanced")) {
        return raw;
    }
    return QStringLiteral("balanced");
}

int keywordMatchBonus(const QStringList &keywords, const QString &query)
{
    const QString q = query.trimmed().toLower();
    if (q.isEmpty()) {
        return 0;
    }
    int bonus = 0;
    for (const QString &raw : keywords) {
        const QString k = raw.trimmed().toLower();
        if (k.isEmpty()) {
            continue;
        }
        if (k == q) {
            bonus = qMax(bonus, 220);
        } else if (k.startsWith(q)) {
            bonus = qMax(bonus, 140);
        } else if (k.contains(q)) {
            bonus = qMax(bonus, 80);
        }
    }
    return bonus;
}

QStringList splitSemicolonList(const QString &value)
{
    QStringList out;
    const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    out.reserve(parts.size());
    for (const QString &part : parts) {
        const QString token = part.trimmed();
        if (!token.isEmpty()) {
            out.push_back(token);
        }
    }
    return out;
}

bool tokenListContains(const QStringList &list, const QString &needle)
{
    const QString n = needle.trimmed().toLower();
    if (n.isEmpty()) {
        return false;
    }
    for (const QString &item : list) {
        if (item.trimmed().toLower() == n) {
            return true;
        }
    }
    return false;
}

Slm::Permissions::Capability activationCapabilityForResult(const QString &provider,
                                                           const QString &resultType)
{
    const QString p = provider.trimmed().toLower();
    const QString t = resultType.trimmed().toLower();
    if (p == QStringLiteral("clipboard")) {
        return Slm::Permissions::Capability::SearchResolveClipboardResult;
    }
    if (p == QStringLiteral("settings")) {
        return Slm::Permissions::Capability::SearchQueryApps;
    }
    if (t == QStringLiteral("app")) {
        return Slm::Permissions::Capability::SearchQueryApps;
    }
    return Slm::Permissions::Capability::SearchQueryFiles;
}

int scoreLocalSlmAction(const QString &name,
                        const QStringList &keywords,
                        const QString &intent,
                        int priority,
                        const QString &query)
{
    const QString q = query.trimmed().toLower();
    int score = 1000 - qMax(0, priority);
    if (q.isEmpty()) {
        return score;
    }
    const QString title = name.trimmed().toLower();
    if (title == q) {
        score += 1000;
    } else if (title.startsWith(q)) {
        score += 500;
    } else if (title.contains(q)) {
        score += 250;
    }
    score += keywordMatchBonus(keywords, query);
    const QString intentNorm = intent.trimmed().toLower();
    if (!intentNorm.isEmpty()) {
        if (intentNorm == q) {
            score += 400;
        } else if (intentNorm.startsWith(q)) {
            score += 180;
        } else if (intentNorm.contains(q)) {
            score += 90;
        }
    }
    return score;
}

int scoreSettingsMatch(const QString &query, const QString &text)
{
    const QString q = query.trimmed().toLower();
    const QString t = text.trimmed().toLower();
    if (q.isEmpty() || t.isEmpty()) {
        return 0;
    }
    if (t == q) {
        return 160;
    }
    if (t.startsWith(q)) {
        return 120;
    }
    if (t.contains(q)) {
        return 85;
    }
    int qi = 0;
    int chain = 0;
    int bonus = 0;
    for (int i = 0; i < t.size() && qi < q.size(); ++i) {
        if (t.at(i) == q.at(qi)) {
            ++qi;
            ++chain;
            bonus += 3 + chain;
        } else {
            chain = 0;
        }
    }
    if (qi == q.size()) {
        return 35 + bonus;
    }
    return 0;
}

QVariantList localSettingsRowsFromMetadata(const QString &query, int limit)
{
    QVariantList out;
    if (query.trimmed().isEmpty() || limit <= 0) {
        return out;
    }

    const QStringList roots{
        QDir::home().filePath(QStringLiteral(".local/lib/settings/modules")),
        QStringLiteral("/usr/lib/settings/modules"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/apps/settings/modules"),
        QDir::currentPath() + QStringLiteral("/src/apps/settings/modules"),
    };
    QSet<QString> seenIds;

    for (const QString &rootPath : roots) {
        QDir root(rootPath);
        if (!root.exists()) {
            continue;
        }
        const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            const QString metadataPath = root.absoluteFilePath(entry + QStringLiteral("/metadata.json"));
            QFile file(metadataPath);
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (!doc.isObject()) {
                continue;
            }
            const QJsonObject obj = doc.object();
            const QString moduleId = obj.value(QStringLiteral("id")).toString().trimmed();
            const QString moduleName = obj.value(QStringLiteral("name")).toString().trimmed();
            if (moduleId.isEmpty() || moduleName.isEmpty()) {
                continue;
            }
            const QString group = obj.value(QStringLiteral("group"))
                                      .toString(obj.value(QStringLiteral("category"))
                                                    .toString(QStringLiteral("General")))
                                      .trimmed();
            const QString icon = obj.value(QStringLiteral("icon")).toString().trimmed();
            QStringList moduleKeywords;
            const QJsonArray mk = obj.value(QStringLiteral("keywords")).toArray();
            moduleKeywords.reserve(mk.size());
            for (const QJsonValue &kw : mk) {
                const QString token = kw.toString().trimmed();
                if (!token.isEmpty()) {
                    moduleKeywords.push_back(token);
                }
            }
            int moduleScore = scoreSettingsMatch(query, moduleName)
                              + scoreSettingsMatch(query, group) / 2;
            for (const QString &kw : moduleKeywords) {
                moduleScore += scoreSettingsMatch(query, kw) / 2;
            }
            if (moduleScore > 0) {
                const QString id = QStringLiteral("module:%1").arg(moduleId);
                if (!seenIds.contains(id)) {
                    seenIds.insert(id);
                    QVariantMap row;
                    row.insert(QStringLiteral("id"), id);
                    row.insert(QStringLiteral("type"), QStringLiteral("module"));
                    row.insert(QStringLiteral("moduleId"), moduleId);
                    row.insert(QStringLiteral("settingId"), QString());
                    row.insert(QStringLiteral("title"), moduleName);
                    row.insert(QStringLiteral("subtitle"), group);
                    row.insert(QStringLiteral("icon"), icon);
                    row.insert(QStringLiteral("score"), moduleScore);
                    row.insert(QStringLiteral("action"), QStringLiteral("settings://%1").arg(moduleId));
                    out.push_back(row);
                }
            }

            const QJsonArray settings = obj.value(QStringLiteral("settings")).toArray();
            for (const QJsonValue &settingVal : settings) {
                const QJsonObject so = settingVal.toObject();
                const QString settingId = so.value(QStringLiteral("id")).toString().trimmed();
                if (settingId.isEmpty()) {
                    continue;
                }
                const QString label = so.value(QStringLiteral("label")).toString().trimmed();
                const QString description = so.value(QStringLiteral("description")).toString().trimmed();
                QStringList settingKeywords;
                const QJsonArray sk = so.value(QStringLiteral("keywords")).toArray();
                settingKeywords.reserve(sk.size());
                for (const QJsonValue &kw : sk) {
                    const QString token = kw.toString().trimmed();
                    if (!token.isEmpty()) {
                        settingKeywords.push_back(token);
                    }
                }

                int settingScore = scoreSettingsMatch(query, label) * 2
                                   + scoreSettingsMatch(query, description)
                                   + scoreSettingsMatch(query, moduleName) / 2;
                for (const QString &kw : settingKeywords) {
                    settingScore += scoreSettingsMatch(query, kw);
                }
                if (settingScore <= 0) {
                    continue;
                }
                const QString id = QStringLiteral("setting:%1/%2").arg(moduleId, settingId);
                if (seenIds.contains(id)) {
                    continue;
                }
                seenIds.insert(id);
                QVariantMap row;
                row.insert(QStringLiteral("id"), id);
                row.insert(QStringLiteral("type"), QStringLiteral("setting"));
                row.insert(QStringLiteral("moduleId"), moduleId);
                row.insert(QStringLiteral("settingId"), settingId);
                row.insert(QStringLiteral("title"), label);
                row.insert(QStringLiteral("subtitle"),
                           QStringLiteral("%1 - %2").arg(moduleName, description));
                row.insert(QStringLiteral("icon"), icon);
                row.insert(QStringLiteral("score"), settingScore + 15);
                row.insert(QStringLiteral("action"),
                           QStringLiteral("settings://%1/%2").arg(moduleId, settingId));
                out.push_back(row);
            }
        }
    }

    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const int sa = a.toMap().value(QStringLiteral("score")).toInt();
        const int sb = b.toMap().value(QStringLiteral("score")).toInt();
        if (sa != sb) {
            return sa > sb;
        }
        return a.toMap().value(QStringLiteral("title")).toString().localeAwareCompare(
                   b.toMap().value(QStringLiteral("title")).toString()) < 0;
    });
    if (out.size() > limit) {
        out = out.mid(0, limit);
    }
    return out;
}

QVariantList localSearchActionsFromDesktopEntries(const QString &query,
                                                  const QVariantMap &context,
                                                  int limit)
{
    QVariantList out;
    const QString scope = context.value(QStringLiteral("scope")).toString().trimmed().toLower();
    const QStringList scanDirs{
        QDir::home().filePath(QStringLiteral(".local/share/applications")),
        QStringLiteral("/usr/share/applications")
    };
    QSet<QString> seenIds;

    for (const QString &dirPath : scanDirs) {
        QDirIterator it(dirPath, QStringList{QStringLiteral("*.desktop")},
                        QDir::Files, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            const QString desktopPath = it.next();
            GKeyFile *kf = g_key_file_new();
            GError *err = nullptr;
            const gboolean loaded = g_key_file_load_from_file(
                kf, QFile::encodeName(desktopPath).constData(), G_KEY_FILE_NONE, &err);
            if (err) {
                g_error_free(err);
            }
            if (!loaded) {
                g_key_file_free(kf);
                continue;
            }

            gsize groupCount = 0;
            gchar **groups = g_key_file_get_groups(kf, &groupCount);
            if (!groups) {
                g_key_file_free(kf);
                continue;
            }
            for (gsize i = 0; i < groupCount; ++i) {
                const QString group = QString::fromUtf8(groups[i]);
                const QString prefix = QStringLiteral("Desktop Action ");
                if (!group.startsWith(prefix)) {
                    continue;
                }
                const QString actionName = group.mid(prefix.size()).trimmed();
                if (actionName.isEmpty()) {
                    continue;
                }
                const QByteArray groupUtf8 = group.toUtf8();
                const QString capabilities = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(),
                                          "X-SLM-Capabilities", nullptr));
                const QStringList capList = splitSemicolonList(capabilities);
                const bool hasSearchAction = tokenListContains(capList, QStringLiteral("searchaction"));
                const bool hasQuickAction = tokenListContains(capList, QStringLiteral("quickaction"));
                const bool hasContextMenu = tokenListContains(capList, QStringLiteral("contextmenu"));
                if (!hasSearchAction && !hasQuickAction && !hasContextMenu) {
                    continue;
                }

                const QString rawScopes = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(),
                                          "X-SLM-SearchAction-Scopes", nullptr));
                const QStringList scopes = splitSemicolonList(rawScopes);
                if (hasSearchAction && !scope.isEmpty() && !scopes.isEmpty()
                    && !tokenListContains(scopes, scope)) {
                    continue;
                }

                const QString name = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(), "Name", nullptr)).trimmed();
                const QString exec = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(), "Exec", nullptr)).trimmed();
                if (name.isEmpty() || exec.isEmpty()) {
                    continue;
                }
                const QString icon = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(), "Icon", nullptr)).trimmed();
                const QString intent = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(),
                                          "X-SLM-SearchAction-Intent", nullptr)).trimmed();
                const QString keywordsRaw = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(),
                                          "X-SLM-Keywords", nullptr)).trimmed();
                const QStringList keywords = splitSemicolonList(keywordsRaw);
                const int priority = QString::fromUtf8(
                    g_key_file_get_string(kf, groupUtf8.constData(),
                                          "X-SLM-Priority", nullptr)).trimmed().toInt();

                const QString actionId = QStringLiteral("%1::%2")
                                             .arg(QFileInfo(desktopPath).fileName(), actionName);
                if (seenIds.contains(actionId)) {
                    continue;
                }
                seenIds.insert(actionId);

                QVariantMap row;
                row.insert(QStringLiteral("id"), actionId);
                row.insert(QStringLiteral("name"), name);
                row.insert(QStringLiteral("exec"), exec);
                row.insert(QStringLiteral("icon"), icon);
                row.insert(QStringLiteral("keywords"), keywords);
                row.insert(QStringLiteral("priority"), priority);
                row.insert(QStringLiteral("desktopFilePath"), desktopPath);
                row.insert(QStringLiteral("score"),
                           scoreLocalSlmAction(name, keywords, intent, priority, query));
                QString inferredCapability = QStringLiteral("searchaction");
                if (!hasSearchAction && hasQuickAction) {
                    inferredCapability = QStringLiteral("quickaction");
                } else if (!hasSearchAction && !hasQuickAction && hasContextMenu) {
                    inferredCapability = QStringLiteral("contextmenu");
                }
                row.insert(QStringLiteral("capability"), inferredCapability);
                row.insert(QStringLiteral("searchAction"), QVariantMap{
                                                               {QStringLiteral("intent"), intent},
                                                               {QStringLiteral("scopes"), scopes},
                                                           });
                out.push_back(row);
            }
            g_strfreev(groups);
            g_key_file_free(kf);
        }
    }

    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap();
        const QVariantMap mb = b.toMap();
        const int sa = ma.value(QStringLiteral("score")).toInt();
        const int sb = mb.value(QStringLiteral("score")).toInt();
        if (sa != sb) {
            return sa > sb;
        }
        return ma.value(QStringLiteral("name")).toString().localeAwareCompare(
                   mb.value(QStringLiteral("name")).toString()) < 0;
    });
    if (out.size() > limit) {
        out = out.mid(0, limit);
    }
    return out;
}
}

GlobalSearchService::GlobalSearchService(DesktopAppModel *appModel,
                                         WorkspaceManager *workspaceManager,
                                         UIPreferences *uiPreferences,
                                         QObject *parent)
    : QObject(parent)
    , m_appModel(appModel)
    , m_workspaceManager(workspaceManager)
    , m_uiPreferences(uiPreferences)
    , m_startedAtUtc(QDateTime::currentDateTimeUtc())
    , m_auditLogger(&m_permissionStore)
{
    qRegisterMetaType<SearchResultEntry>("SearchResultEntry");
    qRegisterMetaType<SearchResultList>("SearchResultList");
    qDBusRegisterMetaType<SearchResultEntry>();
    qDBusRegisterMetaType<SearchResultList>();

    setupSecurity();
    registerDbusService();
    m_trackerPolicy.ignoredDirectories = defaultIgnoredTrackerDirs();
    m_searchProfileUpdatedAtUtc = QDateTime::currentDateTimeUtc();

    registerProvider(QStringLiteral("apps"));
    registerProvider(QStringLiteral("recent_files"));
    registerProvider(QStringLiteral("clipboard"));
    registerProvider(QStringLiteral("settings"));
    if (!QStandardPaths::findExecutable(QStringLiteral("tracker3")).isEmpty()) {
        registerProvider(QStringLiteral("tracker"));
    }
    registerProvider(QStringLiteral("slm_actions"));

    QTimer::singleShot(0, this, [this]() {
        ConfigureTrackerPreset(QVariantMap{});
    });
}

GlobalSearchService::~GlobalSearchService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool GlobalSearchService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString GlobalSearchService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

SearchResultList GlobalSearchService::Query(const QString &text, const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (!allowSearchCapability(Slm::Permissions::Capability::SearchQueryFiles,
                               QVariantMap{
                                   {QStringLiteral("resourceType"), QStringLiteral("search-query")},
                                   {QStringLiteral("sensitivityLevel"), QStringLiteral("Low")},
                                   {QStringLiteral("initiatedByUserGesture"),
                                    options.value(QStringLiteral("initiatedByUserGesture")).toBool()},
                                   {QStringLiteral("initiatedFromOfficialUI"),
                                    options.value(QStringLiteral("initiatedFromOfficialUI")).toBool()},
                               },
                               QStringLiteral("Query"),
                               requestId)) {
        return {};
    }
    emit IndexingStarted();

    QVariantMap effectiveOptions = options;
    if (!effectiveOptions.contains(QStringLiteral("searchProfile"))
        && !effectiveOptions.contains(QStringLiteral("profile"))) {
        effectiveOptions.insert(QStringLiteral("searchProfile"), GetActiveSearchProfile());
    }

    const int limit = qBound(1, effectiveOptions.value(QStringLiteral("limit"), 24).toInt(), 256);
    const bool includeApps = !effectiveOptions.contains(QStringLiteral("includeApps"))
            || effectiveOptions.value(QStringLiteral("includeApps")).toBool();
    const bool includeRecent = !effectiveOptions.contains(QStringLiteral("includeRecent"))
            || effectiveOptions.value(QStringLiteral("includeRecent")).toBool();
    const bool includeClipboard = !effectiveOptions.contains(QStringLiteral("includeClipboard"))
            || effectiveOptions.value(QStringLiteral("includeClipboard")).toBool();
    const bool includeTracker = !effectiveOptions.contains(QStringLiteral("includeTracker"))
            || effectiveOptions.value(QStringLiteral("includeTracker")).toBool();
    const bool includeActions = !effectiveOptions.contains(QStringLiteral("includeActions"))
            || effectiveOptions.value(QStringLiteral("includeActions")).toBool();
    const bool includeSettings = !effectiveOptions.contains(QStringLiteral("includeSettings"))
            || effectiveOptions.value(QStringLiteral("includeSettings")).toBool();
    const bool includePreview = !effectiveOptions.contains(QStringLiteral("includePreview"))
            || effectiveOptions.value(QStringLiteral("includePreview")).toBool();
    const int appBoost = sourceBoost(effectiveOptions, QStringLiteral("apps"), 0);
    const int recentBoost = sourceBoost(effectiveOptions, QStringLiteral("recent_files"), 0);
    const int clipboardBoost = sourceBoost(effectiveOptions, QStringLiteral("clipboard"), 0);
    const int trackerBoost = sourceBoost(effectiveOptions, QStringLiteral("tracker"), 0);
    const int actionsBoost = sourceBoost(effectiveOptions, QStringLiteral("slm_actions"), 0);
    const int settingsBoost = sourceBoost(effectiveOptions, QStringLiteral("settings"), 0);

    SearchResultList out;
    SearchResultList collected;
    QSet<QString> seen;
    m_resultIndex.clear();

    auto appendEntry = [&](const SearchResultEntry &entry) {
        if (entry.id.isEmpty() || entry.metadata.isEmpty() || seen.contains(entry.id)) {
            return;
        }
        seen.insert(entry.id);
        collected.push_back(entry);
    };

    if (includeApps && m_appModel) {
        const QVariantList rows = m_appModel->page(0, qMax(limit * 4, 48), text);
        for (const QVariant &item : rows) {
            const QVariantMap row = item.toMap();
            if (row.isEmpty()) {
                continue;
            }
            SearchResultEntry entry;
            entry.id = canonicalResultId(row);
            if (entry.id.isEmpty()) {
                continue;
            }
            QVariantMap metadata;
            metadata.insert(QStringLiteral("provider"), QStringLiteral("apps"));
            metadata.insert(QStringLiteral("title"), row.value(QStringLiteral("name")).toString());
            metadata.insert(QStringLiteral("subtitle"),
                            !row.value(QStringLiteral("desktopId")).toString().trimmed().isEmpty()
                                ? row.value(QStringLiteral("desktopId")).toString()
                                : row.value(QStringLiteral("executable")).toString());
            metadata.insert(QStringLiteral("icon"),
                            !row.value(QStringLiteral("iconSource")).toString().trimmed().isEmpty()
                                ? row.value(QStringLiteral("iconSource")).toString()
                                : row.value(QStringLiteral("iconName")).toString());
            metadata.insert(QStringLiteral("score"), row.value(QStringLiteral("score")).toInt());
            metadata.insert(QStringLiteral("score"),
                            metadata.value(QStringLiteral("score")).toInt() + appBoost);
            if (includePreview) {
                QVariantMap preview;
                preview.insert(QStringLiteral("type"), QStringLiteral("application"));
                preview.insert(QStringLiteral("desktopId"), row.value(QStringLiteral("desktopId")).toString());
                preview.insert(QStringLiteral("executable"), row.value(QStringLiteral("executable")).toString());
                preview.insert(QStringLiteral("lastLaunch"), row.value(QStringLiteral("lastLaunch")).toString());
                metadata.insert(QStringLiteral("preview"), preview);
            } else {
                metadata.insert(QStringLiteral("preview"), QVariantMap{});
            }
            metadata.insert(QStringLiteral("desktopId"), row.value(QStringLiteral("desktopId")).toString());
            metadata.insert(QStringLiteral("desktopFile"), row.value(QStringLiteral("desktopFile")).toString());
            metadata.insert(QStringLiteral("executable"), row.value(QStringLiteral("executable")).toString());
            entry.metadata = metadata;
            appendEntry(entry);
        }
    }

    if (includeRecent) {
        const QVariantList recentRows = queryRecentFiles(text, qMax(limit * 2, 48));
        for (const QVariant &item : recentRows) {
            const QVariantMap row = item.toMap();
            if (row.isEmpty()) {
                continue;
            }
            SearchResultEntry entry;
            entry.id = QStringLiteral("recent:") + row.value(QStringLiteral("path")).toString();
            QVariantMap metadata;
            metadata.insert(QStringLiteral("provider"), QStringLiteral("recent_files"));
            metadata.insert(QStringLiteral("title"), row.value(QStringLiteral("name")).toString());
            metadata.insert(QStringLiteral("subtitle"), row.value(QStringLiteral("path")).toString());
            metadata.insert(QStringLiteral("icon"), row.value(QStringLiteral("iconName")).toString());
            metadata.insert(QStringLiteral("score"), row.value(QStringLiteral("score")).toInt());
            metadata.insert(QStringLiteral("score"),
                            metadata.value(QStringLiteral("score")).toInt() + recentBoost);
            metadata.insert(QStringLiteral("path"), row.value(QStringLiteral("path")).toString());
            metadata.insert(QStringLiteral("isDir"), row.value(QStringLiteral("isDir")).toBool());
            metadata.insert(QStringLiteral("preview"), includePreview
                                ? QVariantMap{
                                      {QStringLiteral("type"), QStringLiteral("path")},
                                      {QStringLiteral("path"), row.value(QStringLiteral("path")).toString()},
                                      {QStringLiteral("lastOpened"), row.value(QStringLiteral("lastOpened")).toString()},
                                  }
                                : QVariantMap{});
            entry.metadata = metadata;
            appendEntry(entry);
        }
    }

    bool allowClipboardPreview = includeClipboard;
    if (includeClipboard) {
        allowClipboardPreview = allowSearchCapability(
            Slm::Permissions::Capability::SearchQueryClipboardPreview,
            QVariantMap{
                {QStringLiteral("resourceType"), QStringLiteral("search-query-clipboard")},
                {QStringLiteral("sensitivityLevel"), QStringLiteral("Medium")},
                {QStringLiteral("initiatedByUserGesture"),
                 effectiveOptions.value(QStringLiteral("initiatedByUserGesture")).toBool()},
                {QStringLiteral("initiatedFromOfficialUI"),
                 effectiveOptions.value(QStringLiteral("initiatedFromOfficialUI")).toBool()},
            },
            QStringLiteral("Query"),
            requestId);
    }

    if (includeClipboard && allowClipboardPreview) {
        const QVariantList clipboardRows = queryClipboard(text, qMax(limit * 2, 48), effectiveOptions);
        for (const QVariant &item : clipboardRows) {
            const QVariantMap row = item.toMap();
            if (row.isEmpty()) {
                continue;
            }
            const QString resultId = row.value(QStringLiteral("id")).toString().trimmed();
            if (resultId.isEmpty()) {
                continue;
            }
            SearchResultEntry entry;
            entry.id = resultId;
            QVariantMap metadata;
            metadata.insert(QStringLiteral("provider"), QStringLiteral("clipboard"));
            metadata.insert(QStringLiteral("title"), row.value(QStringLiteral("title")).toString());
            metadata.insert(QStringLiteral("subtitle"), row.value(QStringLiteral("subtitle")).toString());
            metadata.insert(QStringLiteral("icon"), row.value(QStringLiteral("iconName")).toString());
            metadata.insert(QStringLiteral("score"),
                            row.value(QStringLiteral("score")).toInt() + clipboardBoost);
            metadata.insert(QStringLiteral("clipboardId"), row.value(QStringLiteral("clipboardId")).toLongLong());
            metadata.insert(QStringLiteral("clipboardType"), row.value(QStringLiteral("clipboardType")).toString());
            metadata.insert(QStringLiteral("isSensitive"), row.value(QStringLiteral("isSensitive")).toBool());
            metadata.insert(QStringLiteral("preview"), includePreview
                                ? row.value(QStringLiteral("preview")).toMap()
                                : QVariantMap{});
            entry.metadata = metadata;
            appendEntry(entry);
        }
    }

    if (includeTracker) {
        const QVariantList trackerRows = queryTracker(text, qMax(limit * 2, 48));
        for (const QVariant &item : trackerRows) {
            const QVariantMap row = item.toMap();
            if (row.isEmpty()) {
                continue;
            }
            const QString path = row.value(QStringLiteral("path")).toString();
            if (path.isEmpty()) {
                continue;
            }
            SearchResultEntry entry;
            entry.id = QStringLiteral("tracker:") + path;
            QVariantMap metadata;
            metadata.insert(QStringLiteral("provider"), QStringLiteral("tracker"));
            metadata.insert(QStringLiteral("title"), row.value(QStringLiteral("name")).toString());
            metadata.insert(QStringLiteral("subtitle"), path);
            metadata.insert(QStringLiteral("icon"), row.value(QStringLiteral("iconName")).toString());
            metadata.insert(QStringLiteral("score"), row.value(QStringLiteral("score")).toInt());
            metadata.insert(QStringLiteral("score"),
                            metadata.value(QStringLiteral("score")).toInt() + trackerBoost);
            metadata.insert(QStringLiteral("path"), path);
            metadata.insert(QStringLiteral("isDir"), row.value(QStringLiteral("isDir")).toBool());
            metadata.insert(QStringLiteral("preview"), includePreview
                                ? QVariantMap{
                                      {QStringLiteral("type"), QStringLiteral("path")},
                                      {QStringLiteral("path"), path},
                                  }
                                : QVariantMap{});
            entry.metadata = metadata;
            appendEntry(entry);
        }
    }

    if (includeActions) {
        const QVariantList actionRows = querySlmSearchActions(text, effectiveOptions, qMax(limit * 2, 48));
        for (const QVariant &item : actionRows) {
            const QVariantMap row = item.toMap();
            if (row.isEmpty()) {
                continue;
            }
            const QString actionId = row.value(QStringLiteral("id")).toString().trimmed();
            if (actionId.isEmpty()) {
                continue;
            }
            SearchResultEntry entry;
            entry.id = QStringLiteral("slm-action:") + actionId;
            QVariantMap metadata;
            const QStringList keywords = row.value(QStringLiteral("keywords")).toStringList();
            const int keywordBonus = keywordMatchBonus(keywords, text);
            int baseScore = row.value(QStringLiteral("score")).toInt() + actionsBoost + keywordBonus;
            if (baseScore <= 0) {
                baseScore = 1;
            }
            metadata.insert(QStringLiteral("provider"), QStringLiteral("slm_actions"));
            metadata.insert(QStringLiteral("title"), row.value(QStringLiteral("name")).toString());
            metadata.insert(QStringLiteral("subtitle"),
                            row.value(QStringLiteral("searchAction")).toMap()
                                .value(QStringLiteral("intent")).toString());
            metadata.insert(QStringLiteral("icon"), row.value(QStringLiteral("icon")).toString());
            metadata.insert(QStringLiteral("score"), baseScore);
            metadata.insert(QStringLiteral("actionId"), actionId);
            metadata.insert(QStringLiteral("actionExec"), row.value(QStringLiteral("exec")).toString());
            metadata.insert(QStringLiteral("desktopId"), row.value(QStringLiteral("desktopId")).toString());
            metadata.insert(QStringLiteral("desktopFile"), row.value(QStringLiteral("desktopFilePath")).toString());
            metadata.insert(QStringLiteral("actionGroup"), row.value(QStringLiteral("group")).toString());
            metadata.insert(QStringLiteral("keywords"), keywords);
            metadata.insert(QStringLiteral("preview"), includePreview
                                ? QVariantMap{
                                      {QStringLiteral("type"), QStringLiteral("action")},
                                      {QStringLiteral("actionId"), actionId},
                                      {QStringLiteral("intent"),
                                       row.value(QStringLiteral("searchAction")).toMap()
                                           .value(QStringLiteral("intent")).toString()},
                                      {QStringLiteral("desktopId"), row.value(QStringLiteral("desktopId")).toString()},
                                      {QStringLiteral("desktopFile"), row.value(QStringLiteral("desktopFilePath")).toString()},
                                      {QStringLiteral("keywords"), keywords},
                                  }
                                : QVariantMap{});
            entry.metadata = metadata;
            appendEntry(entry);
        }
    }

    if (includeSettings) {
        const QVariantList settingRows = querySettings(text, qMax(limit * 2, 48), effectiveOptions);
        for (const QVariant &item : settingRows) {
            const QVariantMap row = item.toMap();
            if (row.isEmpty()) {
                continue;
            }
            const QString resultId = row.value(QStringLiteral("id")).toString().trimmed();
            const QString deepLink = row.value(QStringLiteral("action")).toString().trimmed();
            if (resultId.isEmpty() || deepLink.isEmpty()) {
                continue;
            }
            SearchResultEntry entry;
            entry.id = QStringLiteral("settings:") + resultId;
            QVariantMap metadata;
            const QString entryType = row.value(QStringLiteral("type"),
                                                QStringLiteral("module")).toString();
            metadata.insert(QStringLiteral("provider"), QStringLiteral("settings"));
            metadata.insert(QStringLiteral("title"), row.value(QStringLiteral("title")).toString());
            metadata.insert(QStringLiteral("subtitle"), row.value(QStringLiteral("subtitle")).toString());
            metadata.insert(QStringLiteral("icon"), row.value(QStringLiteral("icon")).toString());
            metadata.insert(QStringLiteral("score"),
                            row.value(QStringLiteral("score")).toInt() + settingsBoost);
            metadata.insert(QStringLiteral("resultType"), QStringLiteral("settings"));
            metadata.insert(QStringLiteral("entryType"), entryType);
            metadata.insert(QStringLiteral("moduleId"), row.value(QStringLiteral("moduleId")).toString());
            metadata.insert(QStringLiteral("settingId"), row.value(QStringLiteral("settingId")).toString());
            metadata.insert(QStringLiteral("deepLink"), deepLink);
            metadata.insert(QStringLiteral("action"), deepLink);
            metadata.insert(QStringLiteral("preview"), includePreview
                                ? QVariantMap{
                                      {QStringLiteral("type"), QStringLiteral("settings")},
                                      {QStringLiteral("entryType"), entryType},
                                      {QStringLiteral("deepLink"), deepLink},
                                  }
                                : QVariantMap{});
            entry.metadata = metadata;
            appendEntry(entry);
        }
    }

    std::sort(collected.begin(), collected.end(), [](const SearchResultEntry &a, const SearchResultEntry &b) {
        const int sa = a.metadata.value(QStringLiteral("score")).toInt();
        const int sb = b.metadata.value(QStringLiteral("score")).toInt();
        if (sa != sb) {
            return sa > sb;
        }
        const QString ta = a.metadata.value(QStringLiteral("title")).toString();
        const QString tb = b.metadata.value(QStringLiteral("title")).toString();
        return ta.localeAwareCompare(tb) < 0;
    });

    out.reserve(qMin(limit, collected.size()));
    for (const SearchResultEntry &entry : std::as_const(collected)) {
        if (out.size() >= limit) {
            break;
        }
        out.push_back(entry);
        m_resultIndex.insert(entry.id, entry.metadata);
    }

    emit IndexingFinished();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("Query"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), true},
                          {QStringLiteral("query"), text},
                          {QStringLiteral("count"), out.size()},
                          {QStringLiteral("limit"), limit},
                         {QStringLiteral("includeApps"), includeApps},
                         {QStringLiteral("includeRecent"), includeRecent},
                         {QStringLiteral("includeClipboard"), includeClipboard},
                         {QStringLiteral("includeTracker"), includeTracker},
                         {QStringLiteral("includeActions"), includeActions},
                         {QStringLiteral("includeSettings"), includeSettings}});
    return out;
}

void GlobalSearchService::ActivateResult(const QString &id, const QVariantMap &activate_data)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QVariantMap meta = m_resultIndex.value(id);
    const QString provider = activate_data.value(QStringLiteral("provider"),
                                                 meta.value(QStringLiteral("provider")))
                                 .toString()
                                 .trimmed();
    QString resultType = activate_data.value(QStringLiteral("result_type")).toString().trimmed();
    if (resultType.isEmpty()) {
        resultType = (provider == QStringLiteral("apps"))
                         ? QStringLiteral("app")
                         : (meta.value(QStringLiteral("isDir")).toBool()
                                ? QStringLiteral("folder")
                                : QStringLiteral("file"));
    }
    const Slm::Permissions::Capability activationCapability =
        activationCapabilityForResult(provider, resultType);
    if (!allowSearchCapability(activationCapability,
                               QVariantMap{
                                   {QStringLiteral("resourceType"), QStringLiteral("search-result")},
                                   {QStringLiteral("resourceId"), id},
                                   {QStringLiteral("sensitivityLevel"), QStringLiteral("Medium")},
                                   {QStringLiteral("initiatedByUserGesture"),
                                    activate_data.value(QStringLiteral("initiatedByUserGesture"), true).toBool()},
                                   {QStringLiteral("initiatedFromOfficialUI"),
                                    activate_data.value(QStringLiteral("initiatedFromOfficialUI")).toBool()},
                               },
                               QStringLiteral("ActivateResult"),
                               requestId)) {
        return;
    }
    QString action = activate_data.value(QStringLiteral("action")).toString().trimmed();
    if (action.isEmpty()) {
        action = (resultType == QStringLiteral("app"))
                     ? QStringLiteral("launch")
                     : QStringLiteral("open");
    }
    const QString query = activate_data.value(QStringLiteral("query")).toString();
    const QString desktopId = meta.value(QStringLiteral("desktopId")).toString().trimmed();
    const QString executable = meta.value(QStringLiteral("executable")).toString().trimmed();
    const QString actionId = meta.value(QStringLiteral("actionId")).toString().trimmed();

    bool ok = false;
    if (!actionId.isEmpty()) {
        QDBusInterface capIface(QString::fromLatin1(kCapabilityService),
                                QString::fromLatin1(kCapabilityPath),
                                QString::fromLatin1(kCapabilityIface),
                                QDBusConnection::sessionBus());
        if (capIface.isValid()) {
            QVariantMap actionContext = activate_data;
            const QStringList uris = activate_data.value(QStringLiteral("uris")).toStringList();
            if (!actionContext.contains(QStringLiteral("uris"))) {
                actionContext.insert(QStringLiteral("uris"), uris);
            }
            if (!actionContext.contains(QStringLiteral("selection_count"))) {
                actionContext.insert(QStringLiteral("selection_count"), uris.size());
            }
            QDBusReply<QVariantMap> invokeReply =
                capIface.call(QStringLiteral("InvokeActionWithContext"), actionId, actionContext);
            ok = invokeReply.isValid()
                 && invokeReply.value().value(QStringLiteral("ok")).toBool();
        }
    }
    if (!ok && !desktopId.isEmpty() && m_workspaceManager) {
        ok = m_workspaceManager->PresentWindow(desktopId);
    }
    if (!ok && provider == QStringLiteral("clipboard")) {
        const qlonglong clipId = meta.value(QStringLiteral("clipboardId")).toLongLong();
        if (clipId > 0) {
            QDBusInterface clipIface(QStringLiteral("org.desktop.Clipboard"),
                                     QStringLiteral("/org/desktop/Clipboard"),
                                     QStringLiteral("org.desktop.Clipboard"),
                                     QDBusConnection::sessionBus());
            if (clipIface.isValid()) {
                QDBusReply<bool> pasteReply = clipIface.call(QStringLiteral("PasteItem"), clipId);
                ok = pasteReply.isValid() && pasteReply.value();
            }
        }
    }
    if (!ok && provider == QStringLiteral("settings")) {
        const QString deepLink = meta.value(QStringLiteral("deepLink"),
                                            meta.value(QStringLiteral("action"))).toString().trimmed();
        if (!deepLink.isEmpty()) {
            ok = QProcess::startDetached(QStringLiteral("gio"),
                                         {QStringLiteral("open"), deepLink});
        } else {
            const QString moduleId = meta.value(QStringLiteral("moduleId")).toString().trimmed();
            if (!moduleId.isEmpty()) {
                ok = QProcess::startDetached(QStringLiteral("slm-settings"),
                                             {QStringLiteral("--module"), moduleId});
            }
        }
    }
    if (!ok && !executable.isEmpty()) {
        QStringList cmd = QProcess::splitCommand(executable);
        if (!cmd.isEmpty()) {
            const QString program = cmd.takeFirst();
            ok = QProcess::startDetached(program, cmd);
        }
    }
    if (!ok) {
        const QString path = meta.value(QStringLiteral("path")).toString().trimmed();
        if (!path.isEmpty()) {
            ok = QProcess::startDetached(QStringLiteral("gio"),
                                         {QStringLiteral("open"), path});
        }
    }

    pushActivationTelemetry(QVariantMap{
        {QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("id"), id},
        {QStringLiteral("query"), query},
        {QStringLiteral("provider"), provider},
        {QStringLiteral("result_type"), resultType},
        {QStringLiteral("action"), action},
        {QStringLiteral("ok"), ok},
        {QStringLiteral("caller"), caller},
    });

    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ActivateResult"),
                         requestId,
                         caller,
                         id,
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), ok},
                          {QStringLiteral("id"), id},
                          {QStringLiteral("desktopId"), desktopId},
                          {QStringLiteral("provider"), provider},
                          {QStringLiteral("result_type"), resultType},
                          {QStringLiteral("action"), action},
                          {QStringLiteral("query"), query}});
}

QVariantMap GlobalSearchService::PreviewResult(const QString &id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (!allowSearchCapability(Slm::Permissions::Capability::SearchQueryFiles,
                               QVariantMap{
                                   {QStringLiteral("resourceType"), QStringLiteral("search-preview")},
                                   {QStringLiteral("resourceId"), id},
                                   {QStringLiteral("sensitivityLevel"), QStringLiteral("Low")},
                               },
                               QStringLiteral("PreviewResult"),
                               requestId)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
            {QStringLiteral("id"), id},
        };
    }
    QVariantMap out;
    const QVariantMap meta = m_resultIndex.value(id);
    if (meta.isEmpty()) {
        out.insert(QStringLiteral("ok"), false);
        out.insert(QStringLiteral("error"), QStringLiteral("not-found"));
        out.insert(QStringLiteral("id"), id);
    } else {
        out.insert(QStringLiteral("ok"), true);
        out.insert(QStringLiteral("id"), id);
        out.insert(QStringLiteral("title"), meta.value(QStringLiteral("title")).toString());
        out.insert(QStringLiteral("subtitle"), meta.value(QStringLiteral("subtitle")).toString());
        out.insert(QStringLiteral("icon"), meta.value(QStringLiteral("icon")));
        out.insert(QStringLiteral("score"), meta.value(QStringLiteral("score")));
        out.insert(QStringLiteral("preview"), meta.value(QStringLiteral("preview")));
    }

    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("PreviewResult"),
                         requestId,
                         caller,
                         id,
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), out.value(QStringLiteral("ok")).toBool()}});
    return out;
}

QVariantMap GlobalSearchService::ResolveClipboardResult(const QString &id,
                                                        const QVariantMap &resolve_data)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (!allowSearchCapability(Slm::Permissions::Capability::SearchResolveClipboardResult,
                               QVariantMap{
                                   {QStringLiteral("resourceType"), QStringLiteral("clipboard-result")},
                                   {QStringLiteral("resourceId"), id},
                                   {QStringLiteral("sensitivityLevel"), QStringLiteral("High")},
                                   {QStringLiteral("initiatedByUserGesture"),
                                    resolve_data.value(QStringLiteral("initiatedByUserGesture"), false).toBool()},
                                   {QStringLiteral("initiatedFromOfficialUI"),
                                    resolve_data.value(QStringLiteral("initiatedFromOfficialUI"), false).toBool()},
                               },
                               QStringLiteral("ResolveClipboardResult"),
                               requestId)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("permission-denied")},
            {QStringLiteral("id"), id},
        };
    }

    const QVariantMap meta = m_resultIndex.value(id);
    if (meta.isEmpty()
        || meta.value(QStringLiteral("provider")).toString().trimmed() != QStringLiteral("clipboard")) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("not-found")},
            {QStringLiteral("id"), id},
        };
    }

    const qlonglong clipboardId = meta.value(QStringLiteral("clipboardId")).toLongLong();
    if (clipboardId <= 0) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-clipboard-id")},
            {QStringLiteral("id"), id},
        };
    }

    const bool recopy = resolve_data.value(QStringLiteral("recopyBeforePaste"), true).toBool();
    bool pasteOk = true;
    if (recopy) {
        QDBusInterface clipIface(QStringLiteral("org.desktop.Clipboard"),
                                 QStringLiteral("/org/desktop/Clipboard"),
                                 QStringLiteral("org.desktop.Clipboard"),
                                 QDBusConnection::sessionBus());
        if (!clipIface.isValid()) {
            pasteOk = false;
        } else {
            QDBusReply<bool> pasteReply = clipIface.call(QStringLiteral("PasteItem"), clipboardId);
            pasteOk = pasteReply.isValid() && pasteReply.value();
        }
    }

    QVariantMap resolved;
    resolved.insert(QStringLiteral("ok"), pasteOk);
    resolved.insert(QStringLiteral("id"), id);
    resolved.insert(QStringLiteral("provider"), QStringLiteral("clipboard"));
    resolved.insert(QStringLiteral("clipboardId"), clipboardId);
    resolved.insert(QStringLiteral("clipboardType"), meta.value(QStringLiteral("clipboardType")));
    resolved.insert(QStringLiteral("title"), meta.value(QStringLiteral("title")));
    resolved.insert(QStringLiteral("subtitle"), meta.value(QStringLiteral("subtitle")));
    resolved.insert(QStringLiteral("requiresRecopyBeforePaste"), recopy);
    resolved.insert(QStringLiteral("isSensitive"), meta.value(QStringLiteral("isSensitive")).toBool());
    if (!pasteOk) {
        resolved.insert(QStringLiteral("error"), QStringLiteral("clipboard-paste-failed"));
    }

    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ResolveClipboardResult"),
                         requestId,
                         caller,
                         id,
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), pasteOk},
                          {QStringLiteral("provider"), QStringLiteral("clipboard")},
                          {QStringLiteral("clipboardId"), clipboardId},
                          {QStringLiteral("recopyBeforePaste"), recopy}});
    return resolved;
}

QVariantMap GlobalSearchService::ConfigureTrackerPreset(const QVariantMap &preset)
{
    const QVariantMap out = applyTrackerPresetInternal(preset, false);
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ConfigureTrackerPreset"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), out.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("path"), out.value(QStringLiteral("path")).toString()}});
    return out;
}

QVariantMap GlobalSearchService::ResetTrackerPreset()
{
    const QVariantMap out = applyTrackerPresetInternal(QVariantMap{}, true);
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QStringLiteral("ResetTrackerPreset"),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), out.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("path"), out.value(QStringLiteral("path")).toString()}});
    return out;
}

QVariantMap GlobalSearchService::TrackerPresetStatus() const
{
    const QString path = trackerConfigPath();
    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("path"), path);
    out.insert(QStringLiteral("exists"), QFileInfo::exists(path));
    out.insert(QStringLiteral("initialDelaySec"), m_trackerPolicy.initialDelaySec);
    out.insert(QStringLiteral("cpuLimit"), m_trackerPolicy.cpuLimit);
    out.insert(QStringLiteral("idleOnly"), m_trackerPolicy.idleOnly);
    out.insert(QStringLiteral("chargingOnly"), m_trackerPolicy.chargingOnly);
    out.insert(QStringLiteral("ignoredDirectories"), m_trackerPolicy.ignoredDirectories);
    QVariantMap gate;
    const bool gateOk = canUseTrackerNow(&gate);
    out.insert(QStringLiteral("trackerGateOpen"), gateOk);
    out.insert(QStringLiteral("trackerGate"), gate);
    return out;
}

QVariantList GlobalSearchService::GetSearchProfiles() const
{
    const QVariantMap balancedBoost{
        {QStringLiteral("apps"), 20},
        {QStringLiteral("settings"), 12},
        {QStringLiteral("recent_files"), 10},
        {QStringLiteral("tracker"), 0},
    };
    const QVariantMap appsFirstBoost{
        {QStringLiteral("apps"), 40},
        {QStringLiteral("settings"), 18},
        {QStringLiteral("recent_files"), 8},
        {QStringLiteral("tracker"), -4},
    };
    const QVariantMap filesFirstBoost{
        {QStringLiteral("apps"), 5},
        {QStringLiteral("settings"), 4},
        {QStringLiteral("recent_files"), 20},
        {QStringLiteral("tracker"), 8},
    };
    return {
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("balanced")},
            {QStringLiteral("label"), QStringLiteral("Balanced")},
            {QStringLiteral("description"), QStringLiteral("Balanced mix between apps and files")},
            {QStringLiteral("sourceBoost"), balancedBoost},
        },
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("apps-first")},
            {QStringLiteral("label"), QStringLiteral("Apps First")},
            {QStringLiteral("description"), QStringLiteral("Prioritize applications over files")},
            {QStringLiteral("sourceBoost"), appsFirstBoost},
        },
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("files-first")},
            {QStringLiteral("label"), QStringLiteral("Files First")},
            {QStringLiteral("description"), QStringLiteral("Prioritize recent and indexed files")},
            {QStringLiteral("sourceBoost"), filesFirstBoost},
        },
    };
}

QString GlobalSearchService::GetActiveSearchProfile() const
{
    if (!m_uiPreferences) {
        return QStringLiteral("balanced");
    }
    const QString configured = m_uiPreferences
                                   ->getPreference(QStringLiteral("tothespot.searchProfile"),
                                                   QStringLiteral("balanced"))
                                   .toString();
    QVariantMap probe;
    probe.insert(QStringLiteral("searchProfile"), configured);
    return normalizedSearchProfile(probe);
}

QVariantMap GlobalSearchService::GetActiveSearchProfileMeta() const
{
    const QString profile = GetActiveSearchProfile();
    const QDateTime ts = m_searchProfileUpdatedAtUtc.isValid()
                             ? m_searchProfileUpdatedAtUtc
                             : QDateTime::currentDateTimeUtc();
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("profileId"), profile},
        {QStringLiteral("updatedAtUtc"), ts.toString(Qt::ISODateWithMs)},
        {QStringLiteral("source"), QStringLiteral("uipreferences")},
    };
}

bool GlobalSearchService::SetActiveSearchProfile(const QString &profileId)
{
    if (!m_uiPreferences) {
        return false;
    }
    QVariantMap probe;
    probe.insert(QStringLiteral("searchProfile"), profileId);
    const QString normalized = normalizedSearchProfile(probe);
    const bool changed =
        m_uiPreferences->setPreference(QStringLiteral("tothespot.searchProfile"), normalized);
    if (changed) {
        m_searchProfileUpdatedAtUtc = QDateTime::currentDateTimeUtc();
        emit SearchProfileChanged(normalized);
    }
    return changed;
}

QVariantMap GlobalSearchService::GetTelemetryMeta() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("activationCapacity"), m_activationTelemetryCapacity},
        {QStringLiteral("maxLimit"), 1000},
    };
}

QVariantList GlobalSearchService::GetActivationTelemetry(int limit) const
{
    const int capped = qBound(1, limit, 1000);
    QVariantList out;
    if (m_activationTelemetry.isEmpty()) {
        return out;
    }
    const int start = qMax(0, m_activationTelemetry.size() - capped);
    out.reserve(m_activationTelemetry.size() - start);
    for (int i = start; i < m_activationTelemetry.size(); ++i) {
        out.push_back(m_activationTelemetry.at(i));
    }
    return out;
}

bool GlobalSearchService::ResetActivationTelemetry()
{
    m_activationTelemetry.clear();
    return true;
}

void GlobalSearchService::pushActivationTelemetry(const QVariantMap &entry)
{
    if (entry.isEmpty()) {
        return;
    }
    m_activationTelemetry.push_back(entry);
    if (m_activationTelemetryCapacity <= 0) {
        return;
    }
    const int overflow = m_activationTelemetry.size() - m_activationTelemetryCapacity;
    if (overflow > 0) {
        m_activationTelemetry.erase(m_activationTelemetry.begin(),
                                    m_activationTelemetry.begin() + overflow);
    }
}

void GlobalSearchService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[slm-perm] global-search: failed to open permission store";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);
}

bool GlobalSearchService::allowSearchCapability(Slm::Permissions::Capability capability,
                                                const QVariantMap &context,
                                                const QString &methodName,
                                                const QString &requestId) const
{
    Slm::Permissions::PolicyDecision decision;
    if (calledFromDBus()) {
        QVariantMap accessContext = context;
        accessContext.insert(QStringLiteral("capability"),
                             Slm::Permissions::capabilityToString(capability));
        decision = m_securityGuard.check(message(), capability, accessContext);
    } else {
        decision.type = Slm::Permissions::DecisionType::Allow;
        decision.capability = capability;
        decision.reason = QStringLiteral("local-inprocess");
    }
    if (decision.isAllowed()) {
        return true;
    }
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         methodName,
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("deny"),
                         {{QStringLiteral("ok"), false},
                          {QStringLiteral("error"), QStringLiteral("permission-denied")},
                          {QStringLiteral("capability"),
                           Slm::Permissions::capabilityToString(capability)},
                          {QStringLiteral("decision"),
                           Slm::Permissions::decisionTypeToString(decision.type)},
                          {QStringLiteral("reason"), decision.reason}});
    return false;
}

void GlobalSearchService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllProperties |
                                           QDBusConnection::ExportAllSignals);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

QString GlobalSearchService::canonicalResultId(const QVariantMap &row)
{
    const QString desktopId = row.value(QStringLiteral("desktopId")).toString().trimmed();
    if (!desktopId.isEmpty()) {
        return QStringLiteral("app:") + desktopId;
    }
    const QString executable = row.value(QStringLiteral("executable")).toString().trimmed();
    if (!executable.isEmpty()) {
        return QStringLiteral("app:exec:") + executable;
    }
    const QString name = row.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty()) {
        return QStringLiteral("app:name:") + name.toLower();
    }
    return QString();
}

void GlobalSearchService::registerProvider(const QString &providerId)
{
    const QString id = providerId.trimmed();
    if (id.isEmpty() || m_providers.contains(id)) {
        return;
    }
    m_providers.insert(id);
    QTimer::singleShot(0, this, [this, id]() {
        emit ProviderRegistered(id);
    });
}

QVariantList GlobalSearchService::queryRecentFiles(const QString &text, int limit) const
{
    QVariantList out;
    if (limit <= 0) {
        return out;
    }

    const QString xbelPath = QDir::home().filePath(QStringLiteral(".local/share/recently-used.xbel"));
    GBookmarkFile *bookmark = g_bookmark_file_new();
    if (!bookmark) {
        return out;
    }
    GError *error = nullptr;
    const QByteArray native = QFile::encodeName(xbelPath);
    if (!g_bookmark_file_load_from_file(bookmark, native.constData(), &error)) {
        if (error) {
            g_error_free(error);
        }
        g_bookmark_file_free(bookmark);
        return out;
    }

    gsize uriCount = 0;
    gchar **uris = g_bookmark_file_get_uris(bookmark, &uriCount);
    if (!uris || uriCount == 0) {
        if (uris) {
            g_strfreev(uris);
        }
        g_bookmark_file_free(bookmark);
        return out;
    }

    const QString needle = text.trimmed();
    for (gsize i = 0; i < uriCount && out.size() < limit; ++i) {
        if (!uris[i]) {
            continue;
        }
        GFile *file = g_file_new_for_uri(uris[i]);
        if (!file) {
            continue;
        }
        char *localPathRaw = g_file_get_path(file);
        const QString localPath = localPathRaw ? QFileInfo(QString::fromUtf8(localPathRaw)).absoluteFilePath() : QString();
        g_free(localPathRaw);
        g_object_unref(file);
        if (localPath.isEmpty()) {
            continue;
        }
        const QFileInfo fi(localPath);
        if (!needle.isEmpty() && !fi.fileName().contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("name"), fi.fileName());
        row.insert(QStringLiteral("path"), localPath);
        row.insert(QStringLiteral("isDir"), fi.isDir());
        row.insert(QStringLiteral("iconName"), fi.isDir() ? QStringLiteral("folder") : QStringLiteral("text-x-generic"));
        row.insert(QStringLiteral("score"), 120 - out.size());

        GError *fieldErr = nullptr;
        GDateTime *modified = g_bookmark_file_get_modified_date_time(bookmark, uris[i], &fieldErr);
        if (fieldErr) {
            g_error_free(fieldErr);
            fieldErr = nullptr;
        }
        if (modified) {
            const gint64 sec = g_date_time_to_unix(modified);
            if (sec > 0) {
                row.insert(QStringLiteral("lastOpened"),
                           QDateTime::fromSecsSinceEpoch(sec, QTimeZone::utc())
                               .toString(Qt::ISODateWithMs));
            }
            g_date_time_unref(modified);
        }
        out.push_back(row);
    }

    g_strfreev(uris);
    g_bookmark_file_free(bookmark);
    return out;
}

QVariantList GlobalSearchService::queryClipboard(const QString &text,
                                                 int limit,
                                                 const QVariantMap &options) const
{
    Slm::Clipboard::ClipboardSearchProvider provider;
    QVariantMap providerOptions;
    providerOptions.insert(QStringLiteral("includePinned"),
                           options.value(QStringLiteral("includePinned"), true).toBool());
    providerOptions.insert(QStringLiteral("includeImages"),
                           options.value(QStringLiteral("includeImages"), true).toBool());
    providerOptions.insert(QStringLiteral("includeSensitive"),
                           options.value(QStringLiteral("includeSensitive"), false).toBool());
    // Trusted context comes from shell call path or explicit official UI marker.
    const bool trusted = !calledFromDBus()
                         || options.value(QStringLiteral("initiatedFromOfficialUI")).toBool();
    providerOptions.insert(QStringLiteral("trustedCaller"), trusted);
    providerOptions.insert(QStringLiteral("userGesture"),
                           options.value(QStringLiteral("initiatedByUserGesture"), false).toBool());
    providerOptions.insert(QStringLiteral("callerRole"),
                           trusted ? QStringLiteral("global-search-ui")
                                   : QStringLiteral("external-caller"));
    return provider.query(text, limit, providerOptions);
}

QVariantList GlobalSearchService::querySettings(const QString &text,
                                                int limit,
                                                const QVariantMap &options) const
{
    QVariantList out;
    const QString query = text.trimmed();
    if (query.isEmpty() || limit <= 0) {
        return out;
    }

    QDBusInterface iface(QStringLiteral("org.slm.Settings.Search"),
                         QStringLiteral("/org/slm/Settings/Search"),
                         QStringLiteral("org.slm.Settings.Search.v1"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        QVariantMap providerOptions = options;
        providerOptions.insert(QStringLiteral("limit"), limit);
        QDBusReply<SearchResultList> reply =
            iface.call(QStringLiteral("Query"), query, providerOptions);
        if (reply.isValid()) {
            const SearchResultList rows = reply.value();
            out.reserve(rows.size());
            for (const SearchResultEntry &entry : rows) {
                QVariantMap row = entry.metadata;
                if (!row.contains(QStringLiteral("id"))) {
                    row.insert(QStringLiteral("id"), entry.id);
                }
                out.push_back(row);
            }
        }
    }

    if (!out.isEmpty()) {
        if (out.size() > limit) {
            out = out.mid(0, limit);
        }
        return out;
    }
    return localSettingsRowsFromMetadata(query, limit);
}

QVariantList GlobalSearchService::queryTracker(const QString &text, int limit) const
{
    QVariantList out;
    if (limit <= 0 || text.trimmed().isEmpty()) {
        return out;
    }
    if (QStandardPaths::findExecutable(QStringLiteral("tracker3")).isEmpty()) {
        return out;
    }
    QVariantMap gate;
    if (!canUseTrackerNow(&gate)) {
        return out;
    }
    const QString cacheKey = text.trimmed().toCaseFolded();
    auto cacheIt = m_trackerCache.constFind(cacheKey);
    if (cacheIt != m_trackerCache.constEnd() && cacheIt->timer.isValid()
        && cacheIt->timer.elapsed() <= kTrackerCacheTtlMs) {
        const QVariantList &cached = cacheIt->rows;
        const int take = qMin(limit, cached.size());
        for (int i = 0; i < take; ++i) {
            out.push_back(cached.at(i));
        }
        return out;
    }

    QProcess proc;
    QStringList args;
    const int fetchLimit = qBound(1, qMax(limit, 32), 64);
    args << QStringLiteral("search")
         << QStringLiteral("--disable-snippets")
         << QStringLiteral("--disable-color")
         << QStringLiteral("--limit")
         << QString::number(fetchLimit)
         << text.trimmed();
    proc.start(QStringLiteral("tracker3"), args);
    if (!proc.waitForFinished(1200)) {
        proc.kill();
        proc.waitForFinished(100);
        return out;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return out;
    }
    const QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = stdoutText.split('\n', Qt::SkipEmptyParts);
    for (const QString &lineRaw : lines) {
        if (out.size() >= limit) {
            break;
        }
        const QString line = lineRaw.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QString path = normalizePathFromUriOrText(line);
        if (path.isEmpty()) {
            continue;
        }
        const QFileInfo fi(path);
        if (!fi.exists()) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("name"), fi.fileName());
        row.insert(QStringLiteral("path"), fi.absoluteFilePath());
        row.insert(QStringLiteral("isDir"), fi.isDir());
        row.insert(QStringLiteral("iconName"), fi.isDir() ? QStringLiteral("folder") : QStringLiteral("text-x-generic"));
        row.insert(QStringLiteral("score"), 80 - out.size());
        out.push_back(row);
    }
    TrackerCacheEntry entry;
    entry.timer.start();
    entry.rows = out;
    m_trackerCache.insert(cacheKey, entry);
    if (m_trackerCache.size() > 96) {
        m_trackerCache.clear();
    }
    return out;
}

QVariantList GlobalSearchService::querySlmSearchActions(const QString &text,
                                                        const QVariantMap &options,
                                                        int limit) const
{
    QVariantList out;
    const QString query = text.trimmed();
    if (query.isEmpty() || limit <= 0) {
        return out;
    }

    QVariantMap context;
    context.insert(QStringLiteral("scope"),
                   options.value(QStringLiteral("scope"), QStringLiteral("tothespot")).toString());
    context.insert(QStringLiteral("selection_count"),
                   options.value(QStringLiteral("selection_count"), 0).toInt());
    context.insert(QStringLiteral("source_app"),
                   options.value(QStringLiteral("source_app"), QStringLiteral("org.slm.tothespot")).toString());
    context.insert(QStringLiteral("text"), query);

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityIface),
                         QDBusConnection::sessionBus());
    const auto localFallback = [&]() -> QVariantList {
        const QVariantList rows = localSearchActionsFromDesktopEntries(query, context, limit);
        qInfo().noquote() << "[slm-search] local-fallback query=" << query
                          << "scope=" << context.value(QStringLiteral("scope")).toString()
                          << "count=" << rows.size();
        return rows;
    };
    if (!iface.isValid()) {
        return localFallback();
    }

    QDBusReply<QVariantList> reply =
        iface.call(QStringLiteral("SearchActions"), query, context);
    if (!reply.isValid()) {
        return localFallback();
    }
    out = reply.value();
    qInfo().noquote() << "[slm-search] dbus SearchActions query=" << query
                      << "scope=" << context.value(QStringLiteral("scope")).toString()
                      << "count=" << out.size();
    if (out.isEmpty()) {
        return localFallback();
    }
    if (out.size() > limit) {
        out = out.mid(0, limit);
    }
    return out;
}

QVariantMap GlobalSearchService::applyTrackerPresetInternal(const QVariantMap &preset, bool resetOnly) const
{
    const QString cfgPath = trackerConfigPath();
    QFileInfo cfgInfo(cfgPath);
    QDir parent = cfgInfo.absoluteDir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("mkdir-failed")},
            {QStringLiteral("path"), cfgPath},
        };
    }

    if (resetOnly) {
        QFile::remove(cfgPath);
        QFile::remove(trackerStatePath());
        auto *self = const_cast<GlobalSearchService *>(this);
        self->m_trackerPolicy = TrackerPresetPolicy{};
        self->m_trackerPolicy.ignoredDirectories = defaultIgnoredTrackerDirs();
        self->m_startedAtUtc = QDateTime::currentDateTimeUtc();
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("path"), cfgPath},
            {QStringLiteral("statePath"), trackerStatePath()},
            {QStringLiteral("reset"), true},
        };
    }

    const int delaySec = qBound(0, preset.value(QStringLiteral("initialDelaySec"), 120).toInt(), 3600);
    const int cpuLimit = qBound(1, preset.value(QStringLiteral("cpuLimit"), 15).toInt(), 100);
    const bool idleOnly = !preset.contains(QStringLiteral("idleOnly")) || preset.value(QStringLiteral("idleOnly")).toBool();
    const bool chargingOnly = !preset.contains(QStringLiteral("chargingOnly")) || preset.value(QStringLiteral("chargingOnly")).toBool();
    QStringList ignored = preset.value(QStringLiteral("ignoredDirectories")).toStringList();
    if (ignored.isEmpty()) {
        ignored = defaultIgnoredTrackerDirs();
    }
    auto *self = const_cast<GlobalSearchService *>(this);
    self->m_trackerPolicy.initialDelaySec = delaySec;
    self->m_trackerPolicy.cpuLimit = cpuLimit;
    self->m_trackerPolicy.idleOnly = idleOnly;
    self->m_trackerPolicy.chargingOnly = chargingOnly;
    self->m_trackerPolicy.ignoredDirectories = ignored;
    self->m_startedAtUtc = QDateTime::currentDateTimeUtc();

    QSettings cfg(cfgPath, QSettings::IniFormat);
    cfg.clear();
    cfg.beginGroup(QStringLiteral("Ignored"));
    cfg.setValue(QStringLiteral("directories"), ignored.join(QLatin1Char(';')));
    cfg.endGroup();
    cfg.beginGroup(QStringLiteral("Indexing"));
    cfg.setValue(QStringLiteral("initial_delay_sec"), delaySec);
    cfg.setValue(QStringLiteral("cpu_limit"), cpuLimit);
    cfg.setValue(QStringLiteral("idle_only"), idleOnly);
    cfg.setValue(QStringLiteral("charging_only"), chargingOnly);
    cfg.endGroup();
    cfg.beginGroup(QStringLiteral("Control"));
    cfg.setValue(QStringLiteral("preset"), QStringLiteral("slm-balanced"));
    cfg.endGroup();
    cfg.sync();

    QSettings state(trackerStatePath(), QSettings::IniFormat);
    state.clear();
    state.beginGroup(QStringLiteral("Policy"));
    state.setValue(QStringLiteral("initial_delay_sec"), delaySec);
    state.setValue(QStringLiteral("cpu_limit"), cpuLimit);
    state.setValue(QStringLiteral("idle_only"), idleOnly);
    state.setValue(QStringLiteral("charging_only"), chargingOnly);
    state.setValue(QStringLiteral("ignored_directories"), ignored.join(QLatin1Char(';')));
    state.setValue(QStringLiteral("applied_at_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    state.endGroup();
    state.sync();

    return {
        {QStringLiteral("ok"), cfg.status() == QSettings::NoError},
        {QStringLiteral("path"), cfgPath},
        {QStringLiteral("statePath"), trackerStatePath()},
        {QStringLiteral("initialDelaySec"), delaySec},
        {QStringLiteral("cpuLimit"), cpuLimit},
        {QStringLiteral("idleOnly"), idleOnly},
        {QStringLiteral("chargingOnly"), chargingOnly},
        {QStringLiteral("ignoredDirectories"), ignored},
    };
}

QString GlobalSearchService::trackerConfigPath() const
{
    return QDir::home().filePath(QStringLiteral(".config/tracker3/miners/files.cfg"));
}

QString GlobalSearchService::trackerStatePath() const
{
    return QDir::home().filePath(QStringLiteral(".config/slm/search/tracker-preset.ini"));
}

QStringList GlobalSearchService::defaultIgnoredTrackerDirs()
{
    return {
        QStringLiteral("node_modules"),
        QStringLiteral(".git"),
        QStringLiteral("build"),
        QStringLiteral("dist"),
        QStringLiteral("target"),
        QStringLiteral(".cache"),
        QStringLiteral("venv"),
        QStringLiteral("__pycache__"),
        QStringLiteral(".gradle"),
        QStringLiteral(".cargo"),
        QStringLiteral("Android"),
        QStringLiteral("ios"),
    };
}

bool GlobalSearchService::canUseTrackerNow(QVariantMap *reason) const
{
    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const int elapsed = m_startedAtUtc.isValid() ? int(m_startedAtUtc.secsTo(now)) : 0;
    out.insert(QStringLiteral("elapsedSecSincePreset"), elapsed);
    out.insert(QStringLiteral("requiredDelaySec"), m_trackerPolicy.initialDelaySec);
    if (elapsed < m_trackerPolicy.initialDelaySec) {
        out.insert(QStringLiteral("ok"), false);
        out.insert(QStringLiteral("reason"), QStringLiteral("waiting-initial-delay"));
    }

    if (out.value(QStringLiteral("ok")).toBool() && m_trackerPolicy.idleOnly && !isSessionIdle()) {
        out.insert(QStringLiteral("ok"), false);
        out.insert(QStringLiteral("reason"), QStringLiteral("session-not-idle"));
    }

    if (out.value(QStringLiteral("ok")).toBool() && m_trackerPolicy.chargingOnly && !isOnACPower()) {
        out.insert(QStringLiteral("ok"), false);
        out.insert(QStringLiteral("reason"), QStringLiteral("not-charging"));
    }

    if (reason) {
        *reason = out;
    }
    return out.value(QStringLiteral("ok")).toBool();
}

bool GlobalSearchService::isSessionIdle() const
{
    QDBusInterface screensaver(QStringLiteral("org.freedesktop.ScreenSaver"),
                               QStringLiteral("/org/freedesktop/ScreenSaver"),
                               QStringLiteral("org.freedesktop.ScreenSaver"),
                               QDBusConnection::sessionBus());
    if (!screensaver.isValid()) {
        // If we cannot determine idle state, avoid blocking search.
        return true;
    }
    QDBusReply<bool> active = screensaver.call(QStringLiteral("GetActive"));
    if (!active.isValid()) {
        return true;
    }
    return active.value();
}

bool GlobalSearchService::isOnACPower() const
{
    QDBusInterface upower(QStringLiteral("org.freedesktop.UPower"),
                          QStringLiteral("/org/freedesktop/UPower"),
                          QStringLiteral("org.freedesktop.DBus.Properties"),
                          QDBusConnection::systemBus());
    if (!upower.isValid()) {
        return true;
    }
    QDBusReply<QVariant> onBattery = upower.call(QStringLiteral("Get"),
                                                 QStringLiteral("org.freedesktop.UPower"),
                                                 QStringLiteral("OnBattery"));
    if (!onBattery.isValid()) {
        return true;
    }
    return !onBattery.value().toBool();
}

QString GlobalSearchService::normalizePathFromUriOrText(const QString &value)
{
    return UrlUtils::absoluteLocalPathFromUriOrText(value);
}

int GlobalSearchService::sourceBoost(const QVariantMap &options,
                                     const QString &provider,
                                     int fallback) const
{
    const QString profile = normalizedSearchProfile(options);
    int profileBoost = fallback;
    if (profile == QStringLiteral("apps-first")) {
        if (provider == QStringLiteral("apps")) {
            profileBoost = 40;
        } else if (provider == QStringLiteral("settings")) {
            profileBoost = 18;
        } else if (provider == QStringLiteral("recent_files")) {
            profileBoost = 8;
        } else if (provider == QStringLiteral("tracker")) {
            profileBoost = -4;
        }
    } else if (profile == QStringLiteral("files-first")) {
        if (provider == QStringLiteral("apps")) {
            profileBoost = 5;
        } else if (provider == QStringLiteral("settings")) {
            profileBoost = 4;
        } else if (provider == QStringLiteral("recent_files")) {
            profileBoost = 20;
        } else if (provider == QStringLiteral("tracker")) {
            profileBoost = 8;
        }
    } else {
        if (provider == QStringLiteral("apps")) {
            profileBoost = 20;
        } else if (provider == QStringLiteral("settings")) {
            profileBoost = 12;
        } else if (provider == QStringLiteral("recent_files")) {
            profileBoost = 10;
        } else if (provider == QStringLiteral("tracker")) {
            profileBoost = 0;
        }
    }

    const QVariantMap boosts = options.value(QStringLiteral("sourceBoost")).toMap();
    if (boosts.contains(provider)) {
        return boosts.value(provider).toInt();
    }
    const QString perProviderKey = provider + QStringLiteral("Boost");
    if (options.contains(perProviderKey)) {
        return options.value(perProviderKey).toInt();
    }
    return profileBoost;
}
