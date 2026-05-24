#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace Slm {

class SettingsSqlStore
{
public:
    SettingsSqlStore();
    ~SettingsSqlStore();

    // Opens (or creates) the DB.  Must be called before any other method.
    bool open(QString *error = nullptr);
    bool isOpen() const;

    // Returns the value for a key from the user layer, or an invalid QVariant
    // if the key has never been written.
    QVariant get(const QString &key) const;

    // Returns all user-layer values as a flat dotted-key map.
    QVariantMap loadAll() const;

    // Writes a single key-value pair inside a transaction.
    bool set(const QString &key, const QVariant &value, QString *error = nullptr);

    // Writes multiple key-value pairs atomically.
    bool setMany(const QVariantMap &kvs, QStringList *changed = nullptr,
                 QString *error = nullptr);

    // Bulk-imports a flat dotted-key map.  Used for JSON migration.
    bool importFlat(const QVariantMap &flat, QString *error = nullptr);

    quint64 revision() const;

private:
    bool initSchema(QString *error);
    bool enableWal();
    bool loadRevision();
    bool bumpRevision(QString *error);
    bool writeRow(const QString &key, const QVariant &value,
                  quint64 rev, QString *error);
    static QString dbPath();
    static QString typeString(const QVariant &v);

    QSqlDatabase m_db;
    quint64      m_revision = 0;
};

} // namespace Slm
