#pragma once

#include <QObject>
#include <QVariantList>

namespace Slm::Clipboard {

class StorageManager;

class ClipboardHistory : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardHistory(StorageManager *storage, QObject *parent = nullptr);

    qint64 add(const QVariantMap &item);
    QVariantList list(int limit = 200) const;
    QVariantList search(const QString &query, int limit = 200) const;
    QVariantMap itemById(qint64 id) const;

    bool setPinned(qint64 id, bool pinned);
    bool deleteItem(qint64 id);
    bool clearHistory();

signals:
    void historyUpdated();

private:
    StorageManager *m_storage = nullptr;
};

} // namespace Slm::Clipboard
