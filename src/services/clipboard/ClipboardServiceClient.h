#pragma once

#include <QObject>
#include <QVariantList>

class QDBusInterface;

namespace Slm::Clipboard {

class ClipboardServiceClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    explicit ClipboardServiceClient(QObject *parent = nullptr);
    ~ClipboardServiceClient() override;

    bool serviceAvailable() const;
    QVariantList history() const;

    Q_INVOKABLE QVariantList getHistory(int limit = 200);
    Q_INVOKABLE QVariantList search(const QString &query, int limit = 200);
    Q_INVOKABLE bool pasteItem(qlonglong id);
    Q_INVOKABLE bool deleteItem(qlonglong id);
    Q_INVOKABLE bool pinItem(qlonglong id, bool pinned = true);
    Q_INVOKABLE bool clearHistory();
    Q_INVOKABLE void refreshHistory(int limit = 80);

signals:
    void serviceAvailableChanged();
    void historyChanged();
    void clipboardChanged(const QVariantMap &item);

private slots:
    void onHistoryUpdated();
    void onClipboardChanged(const QVariantMap &item);
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void bindSignals();
    void refreshServiceAvailability();

    QDBusInterface *m_iface = nullptr;
    bool m_serviceAvailable = false;
    QVariantList m_history;
};

} // namespace Slm::Clipboard
