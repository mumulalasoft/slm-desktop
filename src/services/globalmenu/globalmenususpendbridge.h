#pragma once

#include <QObject>
#include <QTimer>

class GlobalMenuManager;

// GlobalMenuSuspendBridge — listens to org.freedesktop.login1.Manager.PrepareForSleep
// and triggers a menu refresh + rebind after system resume.
//
// On resume it retries GlobalMenuManager::refresh() up to kMaxRetries times at
// kRetryIntervalMs intervals to handle apps that are slow to reattach their
// D-Bus menu registrations after wake.

class GlobalMenuSuspendBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool resumePending READ resumePending NOTIFY resumePendingChanged)

public:
    explicit GlobalMenuSuspendBridge(QObject *parent = nullptr);

    void setMenuManager(GlobalMenuManager *manager);

    bool resumePending() const;

signals:
    void resumePendingChanged();

private slots:
    void onPrepareForSleep(bool sleeping);

private:
    void startRetryLoop();
    void stopRetryLoop();

    GlobalMenuManager *m_manager = nullptr;
    QTimer *m_retryTimer = nullptr;
    int m_retryCount = 0;
    bool m_resumePending = false;

    static constexpr int kMaxRetries = 5;
    static constexpr int kRetryIntervalMs = 1000;
};
