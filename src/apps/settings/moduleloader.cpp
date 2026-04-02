#include "moduleloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {
QString groupForModule(const QVariantMap &module)
{
    const QString raw = module.value("group").toString().trimmed();
    if (!raw.isEmpty()) {
        return raw;
    }
    return QStringLiteral("General");
}
}

ModuleLoader::ModuleLoader(QObject *parent)
    : QObject(parent)
{
}

QVariantList ModuleLoader::modules() const
{
    return m_modules;
}

QVariantList ModuleLoader::groups() const
{
    QVariantList out;
    QSet<QString> seen;
    for (const QVariant &modVar : m_modules) {
        const QVariantMap mod = modVar.toMap();
        const QString group = groupForModule(mod);
        if (seen.contains(group)) {
            continue;
        }
        seen.insert(group);
        out.push_back(group);
    }
    return out;
}

void ModuleLoader::scanModules(const QStringList &paths)
{
    m_modules.clear();
    QSet<QString> seenModuleIds;
    for (const QString &path : paths) {
        QDir dir(path);
        if (!dir.exists()) {
            continue;
        }
        const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            const QString dirPath = dir.absoluteFilePath(entry);
            loadModuleFromDir(dirPath);
            if (!m_modules.isEmpty()) {
                const QString loadedId = m_modules.back().toMap().value("id").toString();
                if (loadedId.isEmpty()) {
                    m_modules.removeLast();
                } else if (seenModuleIds.contains(loadedId)) {
                    m_modules.removeLast();
                } else {
                    seenModuleIds.insert(loadedId);
                }
            }
        }
    }

    addBuiltinModuleOrder();
    emit modulesChanged();
}

QVariantMap ModuleLoader::moduleById(const QString &id) const
{
    for (const QVariant &modVar : m_modules) {
        const QVariantMap mod = modVar.toMap();
        if (mod.value("id").toString() == id) {
            return mod;
        }
    }
    return {};
}

QString ModuleLoader::moduleMainQmlUrl(const QString &id) const
{
    const QVariantMap mod = moduleById(id);
    const QString path = mod.value("mainPath").toString();
    if (path.isEmpty()) {
        return {};
    }
    return QStringLiteral("file://") + path;
}

QVariantList ModuleLoader::modulesForQuery(const QString &query) const
{
    const QString q = normalizeToken(query);
    if (q.isEmpty()) {
        return m_modules;
    }

    QVariantList out;
    for (const QVariant &modVar : m_modules) {
        const QVariantMap mod = modVar.toMap();
        const QString name = normalizeToken(mod.value("name").toString());
        const QString group = normalizeToken(mod.value("group").toString());
        const QStringList keywords = mod.value("keywords").toStringList();
        QVariantList settingItems = mod.value("settings").toList();

        bool match = fuzzyContains(name, q) || fuzzyContains(group, q);
        if (!match) {
            for (const QString &kw : keywords) {
                if (fuzzyContains(normalizeToken(kw), q)) {
                    match = true;
                    break;
                }
            }
        }
        if (!match) {
            for (const QVariant &settingVar : settingItems) {
                const QVariantMap setting = settingVar.toMap();
                if (fuzzyContains(normalizeToken(setting.value("label").toString()), q) ||
                    fuzzyContains(normalizeToken(setting.value("description").toString()), q)) {
                    match = true;
                    break;
                }
                const QStringList sk = setting.value("keywords").toStringList();
                for (const QString &kw : sk) {
                    if (fuzzyContains(normalizeToken(kw), q)) {
                        match = true;
                        break;
                    }
                }
                if (match) {
                    break;
                }
            }
        }

        if (match) {
            out.push_back(mod);
        }
    }
    return out;
}

QVariantList ModuleLoader::settingsForModule(const QString &moduleId) const
{
    return moduleById(moduleId).value("settings").toList();
}

void ModuleLoader::loadModuleFromDir(const QString &dirPath)
{
    QFile metaFile(dirPath + "/metadata.json");
    if (!metaFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
    if (!doc.isObject()) {
        return;
    }

    const QJsonObject obj = doc.object();
    const QString id = obj.value("id").toString().trimmed();
    const QString name = obj.value("name").toString().trimmed();
    if (id.isEmpty() || name.isEmpty()) {
        return;
    }

    QVariantMap mod;
    mod.insert("id", id);
    mod.insert("name", name);
    mod.insert("icon", obj.value("icon").toString());
    mod.insert("group", obj.value("group").toString(obj.value("category").toString("General")));
    mod.insert("description", obj.value("description").toString());

    QStringList keywords;
    const QJsonArray kwArray = obj.value("keywords").toArray();
    for (const QJsonValue &kw : kwArray) {
        const QString token = kw.toString().trimmed();
        if (!token.isEmpty()) {
            keywords << token;
        }
    }
    mod.insert("keywords", keywords);

    const QString main = obj.value("main").toString();
    mod.insert("mainPath", QDir(dirPath).absoluteFilePath(main));
    mod.insert("modulePath", dirPath);
    mod.insert("moduleQml", main);

    QVariantList settingItems;
    const QJsonArray settings = obj.value("settings").toArray();
    for (const QJsonValue &settingVal : settings) {
        const QJsonObject so = settingVal.toObject();
        const QString sid = so.value("id").toString().trimmed();
        if (sid.isEmpty()) {
            continue;
        }
        QVariantMap setting;
        setting.insert("id", sid);
        setting.insert("label", so.value("label").toString());
        setting.insert("description", so.value("description").toString());
        setting.insert("type", so.value("type").toString("action"));
        setting.insert("backendBinding", so.value("backend_binding").toString());
        setting.insert("privilegedAction", so.value("privileged_action").toString());
        setting.insert("anchor", so.value("anchor").toString(sid));
        setting.insert("deepLink", QStringLiteral("settings://%1/%2").arg(id, sid));

        QStringList settingKeywords;
        const QJsonArray sk = so.value("keywords").toArray();
        for (const QJsonValue &kw : sk) {
            const QString token = kw.toString().trimmed();
            if (!token.isEmpty()) {
                settingKeywords << token;
            }
        }
        setting.insert("keywords", settingKeywords);
        settingItems.push_back(setting);
    }
    mod.insert("settings", settingItems);

    m_modules.push_back(mod);
}

void ModuleLoader::addBuiltinModuleOrder()
{
    static const QStringList preferred = {
        QStringLiteral("appearance"),
        QStringLiteral("network"),
        QStringLiteral("bluetooth"),
        QStringLiteral("sound"),
        QStringLiteral("display"),
        QStringLiteral("power"),
        QStringLiteral("print"),
        QStringLiteral("keyboard"),
        QStringLiteral("mouse"),
        QStringLiteral("applications"),
        QStringLiteral("useraccounts"),
        QStringLiteral("permissions"),
        QStringLiteral("developer"),
        QStringLiteral("about")
    };

    std::sort(m_modules.begin(), m_modules.end(), [&](const QVariant &a, const QVariant &b) {
        const QString aid = a.toMap().value("id").toString();
        const QString bid = b.toMap().value("id").toString();
        const int ai = preferred.indexOf(aid);
        const int bi = preferred.indexOf(bid);
        if (ai >= 0 || bi >= 0) {
            if (ai < 0) return false;
            if (bi < 0) return true;
            return ai < bi;
        }
        const QString an = a.toMap().value("name").toString().toLower();
        const QString bn = b.toMap().value("name").toString().toLower();
        return an < bn;
    });
}

QString ModuleLoader::normalizeToken(const QString &text)
{
    QString s = text.toLower().trimmed();
    s.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return s;
}

bool ModuleLoader::fuzzyContains(const QString &haystack, const QString &needle)
{
    if (needle.isEmpty()) {
        return true;
    }
    if (haystack.contains(needle)) {
        return true;
    }
    // Simple subsequence fuzzy fallback.
    int i = 0;
    for (const QChar &c : haystack) {
        if (i < needle.size() && c == needle.at(i)) {
            ++i;
        }
    }
    return i == needle.size();
}
