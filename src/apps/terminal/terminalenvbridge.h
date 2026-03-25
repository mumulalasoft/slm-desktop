#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <functional>

class EnvServiceClient;

// TerminalEnvBridge — fetches the session-wide baseline environment from
// slm-envd at terminal startup.
//
// The fetch is non-blocking (timer-based retry).  Once the environment is
// ready the callback is invoked exactly once.  If the service remains
// unavailable after kMaxRetries attempts, the callback is called with
// QProcessEnvironment::systemEnvironment() so the terminal always starts.
//
class TerminalEnvBridge : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxRetries  = 3;
    static constexpr int kRetryMs     = 1000;

    using EnvCallback = std::function<void(const QProcessEnvironment &env)>;

    explicit TerminalEnvBridge(EnvServiceClient *client, QObject *parent = nullptr);

    // Fetch the baseline environment asynchronously.  Calls `callback` once
    // with the resolved env (or system env as fallback).
    void fetchBaseline(EnvCallback callback);

    // Returns the last successfully fetched environment, or the system
    // environment if fetchBaseline has never been called / succeeded.
    QProcessEnvironment cachedEnv() const;

signals:
    void baselineFetched(const QProcessEnvironment &env);

private slots:
    void onRetryTimer();

private:
    EnvServiceClient    *m_client;
    EnvCallback          m_pending;
    int                  m_retriesLeft = 0;
    QProcessEnvironment  m_cachedEnv;
};
