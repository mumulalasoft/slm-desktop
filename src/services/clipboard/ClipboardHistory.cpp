#include "ClipboardHistory.h"

#include "StorageManager.h"

namespace Slm::Clipboard {

ClipboardHistory::ClipboardHistory(StorageManager *storage, QObject *parent)
    : QObject(parent)
    , m_storage(storage)
{
}

qint64 ClipboardHistory::add(const QVariantMap &item)
{
    if (!m_storage) {
        return -1;
    }
    const qint64 id = m_storage->addItem(item);
    if (id > 0) {
        emit historyUpdated();
    }
    return id;
}

QVariantList ClipboardHistory::list(int limit) const
{
    return m_storage ? m_storage->history(limit) : QVariantList{};
}

QVariantList ClipboardHistory::search(const QString &query, int limit) const
{
    return m_storage ? m_storage->search(query, limit) : QVariantList{};
}

QVariantMap ClipboardHistory::itemById(qint64 id) const
{
    return m_storage ? m_storage->itemById(id) : QVariantMap{};
}

bool ClipboardHistory::setPinned(qint64 id, bool pinned)
{
    if (!m_storage) {
        return false;
    }
    const bool ok = m_storage->setPinned(id, pinned);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool ClipboardHistory::deleteItem(qint64 id)
{
    if (!m_storage) {
        return false;
    }
    const bool ok = m_storage->deleteItem(id);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool ClipboardHistory::clearHistory()
{
    if (!m_storage) {
        return false;
    }
    const bool ok = m_storage->clearHistory();
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

} // namespace Slm::Clipboard
