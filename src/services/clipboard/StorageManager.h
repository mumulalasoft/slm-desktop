#pragma once

#include <QSqlDatabase>
#include <QVariantList>

namespace Slm::Clipboard {

class StorageManager
{
public:
    StorageManager();
    ~StorageManager();

    bool open();
    bool isOpen() const;

    qint64 addItem(const QVariantMap &item);
    QVariantMap itemById(qint64 id) const;
    QVariantList history(int limit) const;
    QVariantList search(const QString &query, int limit) const;

    bool setPinned(qint64 id, bool pinned);
    bool deleteItem(qint64 id);
    bool clearHistory();

private:
    QString dbPath() const;
    bool ensureSchema();
    void enforceLimits();
    void pruneType(const QString &type, int maxCount);

    QSqlDatabase m_db;
    QString m_connectionName;
};

} // namespace Slm::Clipboard
