#include "globalsearchprovider.h"
#include "moduleloader.h"
#include "searchengine.h"

#include <QDBusConnection>
#include <QDBusMetaType>
#include <QSet>

GlobalSearchProvider::GlobalSearchProvider(ModuleLoader *loader, SearchEngine *searchEngine, QObject *parent)
    : QObject(parent)
    , m_loader(loader)
    , m_searchEngine(searchEngine)
{
    qDBusRegisterMetaType<SettingsSearchResult>();
    qDBusRegisterMetaType<QList<SettingsSearchResult>>();

    QDBusConnection::sessionBus().registerObject("/org/slm/Settings/Search", this, QDBusConnection::ExportAllSlots);
    QDBusConnection::sessionBus().registerService("org.slm.Settings.Search");
}

QList<SettingsSearchResult> GlobalSearchProvider::Query(const QString &text, const QVariantMap &options)
{
    QList<SettingsSearchResult> results;
    if (!m_searchEngine) {
        return results;
    }

    int limit = options.value(QStringLiteral("limit"), 30).toInt();
    if (limit <= 0) {
        limit = 30;
    }
    const QVariantList entries = m_searchEngine->query(text, limit);
    QSet<QString> seenIds;
    for (const QVariant &entryVar : entries) {
        const QVariantMap entry = entryVar.toMap();
        SettingsSearchResult res;
        res.id = entry.value("id").toString();
        if (res.id.isEmpty() || seenIds.contains(res.id)) {
            continue;
        }
        seenIds.insert(res.id);
        res.metadata = entry;
        const QString deepLink = entry.value(QStringLiteral("action")).toString();
        res.metadata.insert(QStringLiteral("provider"), QStringLiteral("settings"));
        res.metadata.insert(QStringLiteral("kind"), QStringLiteral("settings"));
        res.metadata.insert(QStringLiteral("entryType"),
                            entry.value(QStringLiteral("type"), QStringLiteral("module")).toString());
        res.metadata.insert(QStringLiteral("deepLink"), deepLink);
        res.metadata.insert(QStringLiteral("action"), deepLink);
        res.metadata.insert(QStringLiteral("category"),
                            entry.value(QStringLiteral("moduleId")).toString());
        res.metadata.insert(QStringLiteral("openTarget"), res.id);
        results.append(res);
    }
    return results;
}

void GlobalSearchProvider::Activate(const QString &id)
{
    emit activationRequested(id);
}
