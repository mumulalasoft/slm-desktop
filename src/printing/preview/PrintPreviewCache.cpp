#include "PrintPreviewCache.h"

namespace Slm::Print {

PrintPreviewCache::PrintPreviewCache(int maxEntries)
    : m_maxEntries(qMax(1, maxEntries))
{
}

QImage PrintPreviewCache::get(const QString &key) const
{
    QWriteLocker locker(&m_lock); // write lock: we update LRU order on hit
    if (!m_store.contains(key)) {
        return QImage();
    }
    // Promote to MRU
    m_lruOrder.removeAll(key);
    m_lruOrder.append(key);
    return m_store.value(key);
}

void PrintPreviewCache::put(const QString &key, const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    QWriteLocker locker(&m_lock);
    if (m_store.contains(key)) {
        m_store.insert(key, image);
        m_lruOrder.removeAll(key);
        m_lruOrder.append(key);
        return;
    }
    while (m_store.size() >= m_maxEntries) {
        evictLru();
    }
    m_store.insert(key, image);
    m_lruOrder.append(key);
}

void PrintPreviewCache::clear()
{
    QWriteLocker locker(&m_lock);
    m_store.clear();
    m_lruOrder.clear();
}

int PrintPreviewCache::size() const
{
    QReadLocker locker(&m_lock);
    return m_store.size();
}

void PrintPreviewCache::evictLru()
{
    // Caller holds write lock.
    if (m_lruOrder.isEmpty()) {
        return;
    }
    const QString oldest = m_lruOrder.takeFirst();
    m_store.remove(oldest);
}

} // namespace Slm::Print
