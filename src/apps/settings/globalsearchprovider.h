#pragma once

#include <QObject>
#include <QDBusArgument>
#include <QDBusContext>
#include <QVariantMap>

class ModuleLoader;
class SearchEngine;

struct SettingsSearchResult {
    QString id;
    QVariantMap metadata;
};

Q_DECLARE_METATYPE(SettingsSearchResult)

inline QDBusArgument &operator<<(QDBusArgument &argument, const SettingsSearchResult &res) {
    argument.beginStructure();
    argument << res.id << res.metadata;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SettingsSearchResult &res) {
    argument.beginStructure();
    argument >> res.id >> res.metadata;
    argument.endStructure();
    return argument;
}

class GlobalSearchProvider : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Settings.Search.v1")

public:
    explicit GlobalSearchProvider(ModuleLoader *loader, SearchEngine *searchEngine, QObject *parent = nullptr);

public slots:
    QList<SettingsSearchResult> Query(const QString &text, const QVariantMap &options);
    void Activate(const QString &id);

signals:
    void activationRequested(const QString &id);

private:
    ModuleLoader *m_loader;
    SearchEngine *m_searchEngine;
};
