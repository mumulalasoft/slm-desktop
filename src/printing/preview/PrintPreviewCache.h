#pragma once

#include <QHash>
#include <QImage>
#include <QReadWriteLock>
#include <QStringList>
#include <QList>

namespace Slm::Print {

// Thread-safe LRU image cache keyed by SHA256-based cache keys.
// Used by PrintPreviewModel to avoid re-rendering unchanged pages.
class PrintPreviewCache
{
public:
    explicit PrintPreviewCache(int maxEntries = 24);

    // Returns a null QImage if the key is not in cache.
    QImage get(const QString &key) const;

    // Stores image under key. Evicts the least-recently-used entry if full.
    void put(const QString &key, const QImage &image);

    // Removes all cached entries.
    void clear();

    int size() const;
    int maxEntries() const { return m_maxEntries; }

private:
    void evictLru();

    mutable QReadWriteLock m_lock;
    mutable QHash<QString, QImage> m_store;
    mutable QStringList m_lruOrder; // front = LRU, back = MRU
    int m_maxEntries;
};

} // namespace Slm::Print
