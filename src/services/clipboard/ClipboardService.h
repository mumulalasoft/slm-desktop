#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::Clipboard {

class StorageManager;
class ClipboardHistory;
class ClipboardWatcher;

class ClipboardService : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardService(QObject *parent = nullptr);
    ~ClipboardService() override;

    bool isReady() const;

    QVariantList getHistory(int limit = 200) const;
    QVariantList search(const QString &query, int limit = 200) const;
    QString backendMode() const;
    bool pasteItem(qint64 id);
    bool deleteItem(qint64 id);
    bool pinItem(qint64 id, bool pinned = true);
    bool clearHistory();

signals:
    void ClipboardChanged(const QVariantMap &item);
    void HistoryUpdated();

private slots:
    void onCaptured(const QVariantMap &item);

private:
    StorageManager *m_storage = nullptr;
    ClipboardHistory *m_history = nullptr;
    ClipboardWatcher *m_watcher = nullptr;
    bool m_ready = false;
};

} // namespace Slm::Clipboard
