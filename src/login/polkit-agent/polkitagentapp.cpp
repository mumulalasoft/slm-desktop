#include "polkitagentapp.h"

#include "authdialogcontroller.h"
#include "authsession.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QProcessEnvironment>

#ifdef SLM_HAVE_POLKIT_AGENT
#ifdef signals
#undef signals
#define SLM_POLKIT_AGENT_RESTORE_QT_SIGNALS
#endif
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
extern "C" {
#include <gio/gio.h>
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>
}
#ifdef SLM_POLKIT_AGENT_RESTORE_QT_SIGNALS
#define signals Q_SIGNALS
#undef SLM_POLKIT_AGENT_RESTORE_QT_SIGNALS
#endif
#include <unistd.h>
#endif

namespace Slm::Login {
namespace {
QString lockFilePath()
{
    return QDir(QDir::tempPath()).filePath(QStringLiteral("slm-polkit-agent.lock"));
}

#ifdef SLM_HAVE_POLKIT_AGENT
PolkitSubject *createSessionSubject(GError **error)
{
    const QString sessionId = qEnvironmentVariable("XDG_SESSION_ID").trimmed();
    if (!sessionId.isEmpty()) {
        PolkitSubject *subject = POLKIT_SUBJECT(polkit_unix_session_new(sessionId.toUtf8().constData()));
        if (subject) {
            return subject;
        }
    }
    return polkit_unix_session_new_for_process_sync(getpid(), nullptr, error);
}

struct SlmPolkitListener {
    PolkitAgentListener parent_instance;
    PolkitAgentApp *app = nullptr;
};

struct SlmPolkitListenerClass {
    PolkitAgentListenerClass parent_class;
};

G_DEFINE_TYPE(SlmPolkitListener, slm_polkit_listener, POLKIT_AGENT_TYPE_LISTENER)

SlmPolkitListener *asSlmListener(PolkitAgentListener *listener)
{
    return reinterpret_cast<SlmPolkitListener *>(listener);
}

GTask *asTask(void *task)
{
    return static_cast<GTask *>(task);
}

GList *asIdentityList(void *identities)
{
    return static_cast<GList *>(identities);
}

GCancellable *asCancellable(void *cancellable)
{
    return static_cast<GCancellable *>(cancellable);
}

void onPendingCancelled(GCancellable *, gpointer userData)
{
    auto *app = static_cast<PolkitAgentApp *>(userData);
    if (!app) {
        return;
    }
    QMetaObject::invokeMethod(app,
                              [app]() {
                                  app->completeAuthenticationRequest(false, QStringLiteral("request-cancelled"));
                              },
                              Qt::QueuedConnection);
}

void slm_listener_initiate_authentication(PolkitAgentListener *listener,
                                          const gchar *action_id,
                                          const gchar *message,
                                          const gchar *icon_name,
                                          PolkitDetails *details,
                                          const gchar *cookie,
                                          GList *identities,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
    auto *typedListener = asSlmListener(listener);
    if (!typedListener || !typedListener->app) {
        GTask *task = g_task_new(listener, cancellable, callback, user_data);
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED, "listener-not-initialized");
        g_object_unref(task);
        return;
    }

    GTask *task = g_task_new(listener, cancellable, callback, user_data);
    const bool accepted = typedListener->app->beginAuthenticationRequest(QString::fromUtf8(action_id ? action_id : ""),
                                                                         QString::fromUtf8(message ? message : ""),
                                                                         QString::fromUtf8(icon_name ? icon_name : ""),
                                                                         details,
                                                                         QString::fromUtf8(cookie ? cookie : ""),
                                                                         identities,
                                                                         task,
                                                                         cancellable);
    if (!accepted) {
        g_object_unref(task);
    }
}

gboolean slm_listener_initiate_authentication_finish(PolkitAgentListener *, GAsyncResult *res, GError **error)
{
    return g_task_propagate_boolean(G_TASK(res), error);
}

void slm_polkit_listener_init(SlmPolkitListener *)
{
}

void slm_polkit_listener_class_init(SlmPolkitListenerClass *klass)
{
    auto *listenerClass = POLKIT_AGENT_LISTENER_CLASS(klass);
    listenerClass->initiate_authentication = slm_listener_initiate_authentication;
    listenerClass->initiate_authentication_finish = slm_listener_initiate_authentication_finish;
}

PolkitAgentListener *createListener(PolkitAgentApp *app)
{
    auto *listener = static_cast<SlmPolkitListener *>(g_object_new(slm_polkit_listener_get_type(), nullptr));
    listener->app = app;
    return POLKIT_AGENT_LISTENER(listener);
}
#endif
} // namespace

PolkitAgentApp::PolkitAgentApp(QObject *parent)
    : QObject(parent)
    , m_lockFile(lockFilePath())
    , m_authSession(new AuthSession(this))
    , m_dialogController(new AuthDialogController(this))
{
    m_dialogController->setSession(m_authSession);

    m_lockFile.setStaleLockTime(15000);
    m_heartbeat.setInterval(30000);
    connect(&m_heartbeat, &QTimer::timeout, this, []() {
        qInfo().noquote() << "[slm-polkit-agent] heartbeat"
                          << QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    });

    connect(m_authSession, &AuthSession::request, this, [this](const QString &prompt, bool echoOn) {
        qInfo().noquote() << "[slm-polkit-agent] auth prompt" << prompt;
        if (m_dialogController) {
            m_dialogController->handleSessionRequest(prompt, echoOn);
        }
    });

    connect(m_authSession, &AuthSession::showError, this, [this](const QString &message) {
        qWarning().noquote() << "[slm-polkit-agent] auth error:" << message;
        if (m_dialogController) {
            m_dialogController->handleSessionError(message);
        }
    });

    connect(m_authSession, &AuthSession::showInfo, this, [](const QString &message) {
        qInfo().noquote() << "[slm-polkit-agent] auth info:" << message;
    });

    connect(m_authSession, &AuthSession::completed, this, [this](bool gainedAuthorization) {
        if (m_dialogController) {
            m_dialogController->handleSessionCompleted();
        }
        completeAuthenticationRequest(gainedAuthorization, gainedAuthorization ? QString() : QStringLiteral("authorization-denied"));
    });
}

PolkitAgentApp::~PolkitAgentApp()
{
#ifdef SLM_HAVE_POLKIT_AGENT
    completeAuthenticationRequest(false, QStringLiteral("agent-shutdown"));
    if (m_registrationHandle) {
        polkit_agent_listener_unregister(m_registrationHandle);
        m_registrationHandle = nullptr;
    }
    if (m_listener) {
        g_object_unref(m_listener);
        m_listener = nullptr;
    }
#endif
    if (m_lockFile.isLocked()) {
        m_lockFile.unlock();
    }
}

bool PolkitAgentApp::start(QString *error)
{
    if (!m_lockFile.tryLock()) {
        // Recover stale lock file when previous process crashed.
        const QString lockPath = m_lockFile.fileName();
        if ((m_lockFile.removeStaleLockFile() || QFile::remove(lockPath)) && m_lockFile.tryLock()) {
            qInfo().noquote() << "[slm-polkit-agent] recovered stale lock file:" << lockPath;
        } else {
            qint64 stalePid = -1;
            QString staleHost;
            QString staleApp;
            m_lockFile.getLockInfo(&stalePid, &staleHost, &staleApp);
            qWarning().noquote() << "[slm-polkit-agent] lock held file=" << lockPath
                                 << "pid=" << stalePid
                                 << "host=" << staleHost << "app=" << staleApp;
            if (error) {
                *error = QStringLiteral("another-instance-running");
            }
            return false;
        }
    }

    if (!m_lockFile.isLocked()) {
        if (error) {
            *error = QStringLiteral("another-instance-running");
        }
        return false;
    }

#ifdef SLM_HAVE_POLKIT_AGENT
    GError *gerror = nullptr;
    bool registered = false;
    PolkitAgentListener *listener = createListener(this);
    if (!listener) {
        qWarning().noquote() << "[slm-polkit-agent] listener init failed";
        qWarning().noquote() << "[slm-polkit-agent] running bootstrap mode without listener";
        if (error) {
            *error = QStringLiteral("listener-init-failed");
        }
    } else {
        gerror = nullptr;
        PolkitSubject *subject = createSessionSubject(&gerror);
        if (!subject) {
            if (gerror) {
                qWarning().noquote() << "[slm-polkit-agent] subject init failed:" << gerror->message;
                if (error) {
                    *error = QString::fromUtf8(gerror->message);
                }
                g_error_free(gerror);
            }
            g_object_unref(listener);
            listener = nullptr;
        } else {
            gerror = nullptr;
            gpointer registration = polkit_agent_listener_register(
                listener,
                POLKIT_AGENT_REGISTER_FLAGS_RUN_IN_THREAD,
                subject,
                "/org/slm/PolkitAgent",
                nullptr,
                &gerror);
            g_object_unref(subject);
            if (!registration) {
                if (gerror) {
                    qWarning().noquote() << "[slm-polkit-agent] register failed:" << gerror->message;
                    if (error) {
                        *error = QString::fromUtf8(gerror->message);
                    }
                    g_error_free(gerror);
                }
                g_object_unref(listener);
                listener = nullptr;
            } else {
                m_listener = listener;
                m_registrationHandle = registration;
                registered = true;
                qInfo().noquote() << "[slm-polkit-agent] listener registered at /org/slm/PolkitAgent";
            }
        }
    }
    if (!registered) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("agent-registration-failed");
        }
        return false;
    }
#else
    qInfo().noquote() << "[slm-polkit-agent] built without polkit headers; running bootstrap-only mode";
#endif

    m_heartbeat.start();
    qInfo().noquote() << "[slm-polkit-agent] started";
    return true;
}

AuthDialogController *PolkitAgentApp::dialogController() const
{
    return m_dialogController;
}

#ifdef SLM_HAVE_POLKIT_AGENT
bool PolkitAgentApp::beginAuthenticationRequest(const QString &actionId,
                                                 const QString &message,
                                                 const QString &iconName,
                                                 void *details,
                                                 const QString &cookie,
                                                 void *identities,
                                                 void *task,
                                                 void *cancellable)
{
    Q_UNUSED(iconName);
    Q_UNUSED(details);

    if (m_pendingTask) {
        g_task_return_new_error(asTask(task), G_IO_ERROR, G_IO_ERROR_BUSY, "another-authentication-is-active");
        return false;
    }

    GList *identityList = asIdentityList(identities);
    if (!identityList || !identityList->data) {
        g_task_return_new_error(asTask(task), G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "identity-not-provided");
        return false;
    }

    PolkitIdentity *identity = POLKIT_IDENTITY(identityList->data);
    gchar *identityText = polkit_identity_to_string(identity);
    const QString identityLabel = QString::fromUtf8(identityText ? identityText : "");
    g_free(identityText);

    m_dialogController->beginRequest(actionId, message, identityLabel);

    QString startError;
    if (!m_authSession->start(identity, cookie, &startError)) {
        m_dialogController->endRequest();
        g_task_return_new_error(asTask(task), G_IO_ERROR, G_IO_ERROR_FAILED, "%s", startError.toUtf8().constData());
        return false;
    }

    m_pendingTask = task;
    m_pendingCancellable = cancellable;
    if (m_pendingCancellable) {
        g_object_ref(m_pendingCancellable);
        m_pendingCancelHandler = g_cancellable_connect(asCancellable(m_pendingCancellable),
                                                       G_CALLBACK(onPendingCancelled),
                                                       this,
                                                       nullptr);
    }

    qInfo().noquote() << "[slm-polkit-agent] auth request accepted action=" << actionId << "identity=" << identityLabel;
    return true;
}

void PolkitAgentApp::completeAuthenticationRequest(bool gainedAuthorization, const QString &errorMessage)
{
    if (!m_pendingTask) {
        return;
    }

    if (m_authSession->isActive()) {
        m_authSession->cancel();
    }

    m_dialogController->endRequest();

    if (m_pendingCancellable) {
        g_cancellable_disconnect(asCancellable(m_pendingCancellable), m_pendingCancelHandler);
        g_object_unref(m_pendingCancellable);
        m_pendingCancellable = nullptr;
        m_pendingCancelHandler = 0;
    }

    GTask *task = asTask(m_pendingTask);
    m_pendingTask = nullptr;

    if (gainedAuthorization) {
        g_task_return_boolean(task, TRUE);
    } else {
        const QByteArray utf8 = errorMessage.isEmpty() ? QByteArray("authentication-failed") : errorMessage.toUtf8();
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", utf8.constData());
    }
    g_object_unref(task);
}
#endif

} // namespace Slm::Login
