#include "searchengine.h"
#include "moduleloader.h"

#include <QElapsedTimer>
#include <QMap>
#include <QSet>
#include <algorithm>

namespace {
QString groupForModule(const QVariantMap &module)
{
    const QString group = module.value("group").toString().trimmed();
    if (!group.isEmpty()) {
        return group;
    }
    return QStringLiteral("General");
}
}

SearchEngine::SearchEngine(ModuleLoader *loader, QObject *parent)
    : QObject(parent)
    , m_loader(loader)
{
    connect(m_loader, &ModuleLoader::modulesChanged, this, &SearchEngine::performSearch);
}

void SearchEngine::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query) {
        return;
    }
    m_searchQuery = query;
    emit searchQueryChanged();
    performSearch();
}

QVariantList SearchEngine::query(const QString &text, int limit) const
{
    return queryInternal(text, limit);
}

QVariantList SearchEngine::queryInternal(const QString &text, int limit) const
{
    const QString query = norm(text);
    QVariantList out;
    if (query.isEmpty()) {
        return out;
    }

    const QVariantList modules = m_loader->modules();
    for (const QVariant &modVar : modules) {
        const QVariantMap mod = modVar.toMap();
        const QString moduleId = mod.value("id").toString();
        const QString moduleName = mod.value("name").toString();
        const QString group = mod.value("group").toString();
        const QString icon = mod.value("icon").toString();
        const QStringList keywords = mod.value("keywords").toStringList();

        int moduleScore = scoreMatch(query, moduleName) + scoreMatch(query, group) / 2;
        for (const QString &kw : keywords) {
            moduleScore += scoreMatch(query, kw) / 2;
        }

        if (moduleScore > 0) {
            QVariantMap entry;
            entry.insert("id", QStringLiteral("module:%1").arg(moduleId));
            entry.insert("type", "module");
            entry.insert("moduleId", moduleId);
            entry.insert("settingId", "");
            entry.insert("title", moduleName);
            entry.insert("subtitle", group);
            entry.insert("icon", icon);
            entry.insert("score", moduleScore);
            entry.insert("action", QStringLiteral("settings://%1").arg(moduleId));
            out.push_back(entry);
        }

        const QVariantList settings = mod.value("settings").toList();
        for (const QVariant &settingVar : settings) {
            const QVariantMap setting = settingVar.toMap();
            const QString settingId = setting.value("id").toString();
            const QString label = setting.value("label").toString();
            const QString description = setting.value("description").toString();
            const QStringList settingKeywords = setting.value("keywords").toStringList();

            int settingScore = 0;
            settingScore += scoreMatch(query, label) * 2;
            settingScore += scoreMatch(query, description);
            for (const QString &kw : settingKeywords) {
                settingScore += scoreMatch(query, kw);
            }
            settingScore += scoreMatch(query, moduleName) / 2;

            if (settingScore <= 0) {
                continue;
            }
            QVariantMap entry;
            entry.insert("id", QStringLiteral("setting:%1/%2").arg(moduleId, settingId));
            entry.insert("type", "setting");
            entry.insert("moduleId", moduleId);
            entry.insert("settingId", settingId);
            entry.insert("title", label);
            entry.insert("subtitle", QStringLiteral("%1 - %2").arg(moduleName, description));
            entry.insert("icon", icon);
            entry.insert("score", settingScore + 15);
            entry.insert("action", QStringLiteral("settings://%1/%2").arg(moduleId, settingId));
            out.push_back(entry);
        }
    }

    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("score").toInt() > b.toMap().value("score").toInt();
    });
    if (limit > 0 && out.size() > limit) {
        out = out.mid(0, limit);
    }
    return out;
}

int SearchEngine::scoreMatch(const QString &query, const QString &text)
{
    const QString q = norm(query);
    const QString t = norm(text);
    if (q.isEmpty() || t.isEmpty()) {
        return 0;
    }
    if (t == q) {
        return 150;
    }
    if (t.startsWith(q)) {
        return 110;
    }
    if (t.contains(q)) {
        return 80;
    }
    // Subsequence fuzzy score.
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

QString SearchEngine::norm(const QString &v)
{
    return v.toLower().simplified();
}

void SearchEngine::performSearch()
{
    QElapsedTimer timer;
    timer.start();
    m_results = queryInternal(m_searchQuery, 120);

    QMap<QString, QVariantList> grouped;
    QMap<QString, QVariantMap> moduleMap;
    for (const QVariant &modVar : m_loader->modules()) {
        const QVariantMap mod = modVar.toMap();
        moduleMap.insert(mod.value("id").toString(), mod);
    }

    QSet<QString> seenModules;
    QVariantList sidebar;
    for (const QVariant &entryVar : m_results) {
        const QVariantMap entry = entryVar.toMap();
        const QString moduleId = entry.value("moduleId").toString();
        if (!moduleMap.contains(moduleId)) {
            continue;
        }
        const QVariantMap mod = moduleMap.value(moduleId);
        grouped[groupForModule(mod)].append(entry);
        if (!seenModules.contains(moduleId)) {
            sidebar.push_back(mod);
            seenModules.insert(moduleId);
        }
    }

    m_sidebarModules = sidebar;
    m_groupedResults.clear();
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        QVariantMap group;
        group.insert("group", it.key());
        group.insert("entries", it.value());
        m_groupedResults.push_back(group);
    }
    m_lastSearchResultCount = m_results.size();
    m_lastSearchLatencyMs = timer.elapsed();
    emit searchTelemetryChanged();
    emit resultsChanged();
}
