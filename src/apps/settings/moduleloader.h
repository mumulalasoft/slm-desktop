#pragma once

#include <QObject>
#include <QVariantList>
#include <QStringList>

class ModuleLoader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList modules READ modules NOTIFY modulesChanged)
    Q_PROPERTY(QVariantList groups READ groups NOTIFY modulesChanged)

public:
    explicit ModuleLoader(QObject *parent = nullptr);

    QVariantList modules() const;
    QVariantList groups() const;

    void scanModules(const QStringList &paths);

    Q_INVOKABLE QVariantMap moduleById(const QString &id) const;
    Q_INVOKABLE QString moduleMainQmlUrl(const QString &id) const;
    Q_INVOKABLE QVariantList modulesForQuery(const QString &query) const;
    Q_INVOKABLE QVariantList settingsForModule(const QString &moduleId) const;

signals:
    void modulesChanged();

private:
    void loadModuleFromDir(const QString &dirPath);
    void addBuiltinModuleOrder();
    static QString normalizeToken(const QString &text);
    static bool fuzzyContains(const QString &haystack, const QString &needle);

    QVariantList m_modules;
};
