#pragma once

#include <QObject>
#include <QLockFile>
#include <QQueue>
#include <QString>
#include <QTimer>

namespace Slm::Login {

class AgentNotifier;
class AuthDialogController;
class AuthSession;

class PolkitAgentApp : public QObject
{
    Q_OBJECT

public:
    explicit PolkitAgentApp(QObject *parent = nullptr);
    ~PolkitAgentApp() override;

    bool start(QString *error = nullptr);
    AuthDialogController *dialogController() const;

#ifdef SLM_HAVE_POLKIT_AGENT
    bool beginAuthenticationRequest(const QString &actionId,
                                    const QString &message,
                                    const QString &iconName,
                                    void *details,
                                    const QString &cookie,
                                    void *identities,
                                    void *task,
                                    void *cancellable);
    void completeAuthenticationRequest(bool gainedAuthorization, const QString &errorMessage = QString());
#endif

private:
    QLockFile m_lockFile;
    QTimer m_heartbeat;
    AuthSession *m_authSession = nullptr;
    AuthDialogController *m_dialogController = nullptr;
    AgentNotifier *m_agentNotifier = nullptr;
#ifdef SLM_HAVE_POLKIT_AGENT
    struct QueuedRequest {
        QString actionId;
        QString message;
        QString cookie;
        QString identityLabel;
        void *identity = nullptr;    // PolkitIdentity*, g_object_ref'd
        void *task = nullptr;        // GTask*
        void *cancellable = nullptr; // GCancellable*
    };

    void processNextRequest();

    void *m_listener = nullptr;
    void *m_registrationHandle = nullptr;
    void *m_pendingTask = nullptr;
    void *m_pendingCancellable = nullptr;
    unsigned long m_pendingCancelHandler = 0;
    QQueue<QueuedRequest> m_requestQueue;
    QTimer m_requestTimeout;
#endif
};

} // namespace Slm::Login
