#include "authsession.h"

#include <QByteArray>

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
#endif

namespace Slm::Login {
namespace {
void clearByteArray(QByteArray &bytes)
{
    volatile char *p = bytes.data();
    for (int i = 0; i < bytes.size(); ++i) {
        p[i] = 0;
    }
    bytes.clear();
}

#ifdef SLM_HAVE_POLKIT_AGENT
PolkitAgentSession *asSession(void *handle)
{
    return static_cast<PolkitAgentSession *>(handle);
}

void onRequest(PolkitAgentSession *, const gchar *request, gboolean echoOn, gpointer userData)
{
    auto *self = static_cast<AuthSession *>(userData);
    if (!self) {
        return;
    }
    Q_EMIT self->request(QString::fromUtf8(request ? request : ""), static_cast<bool>(echoOn));
}

void onShowError(PolkitAgentSession *, const gchar *message, gpointer userData)
{
    auto *self = static_cast<AuthSession *>(userData);
    if (!self) {
        return;
    }
    Q_EMIT self->showError(QString::fromUtf8(message ? message : ""));
}

void onShowInfo(PolkitAgentSession *, const gchar *message, gpointer userData)
{
    auto *self = static_cast<AuthSession *>(userData);
    if (!self) {
        return;
    }
    Q_EMIT self->showInfo(QString::fromUtf8(message ? message : ""));
}

void onCompleted(PolkitAgentSession *, gboolean gainedAuthorization, gpointer userData)
{
    auto *self = static_cast<AuthSession *>(userData);
    if (!self) {
        return;
    }
    self->handleBackendCompleted(static_cast<bool>(gainedAuthorization));
}
#endif
} // namespace

AuthSession::AuthSession(QObject *parent)
    : QObject(parent)
{
}

AuthSession::~AuthSession()
{
    clearSession();
}

bool AuthSession::start(void *identity, const QString &cookie, QString *error)
{
#ifdef SLM_HAVE_POLKIT_AGENT
    if (!identity) {
        if (error) {
            *error = QStringLiteral("identity-required");
        }
        return false;
    }
    if (cookie.isEmpty()) {
        if (error) {
            *error = QStringLiteral("cookie-required");
        }
        return false;
    }
    if (m_session) {
        if (error) {
            *error = QStringLiteral("session-already-active");
        }
        return false;
    }

    QByteArray cookieUtf8 = cookie.toUtf8();
    PolkitAgentSession *session = polkit_agent_session_new(static_cast<PolkitIdentity *>(identity), cookieUtf8.constData());
    if (!session) {
        if (error) {
            *error = QStringLiteral("session-init-failed");
        }
        return false;
    }

    g_signal_connect(session, "request", G_CALLBACK(onRequest), this);
    g_signal_connect(session, "show-error", G_CALLBACK(onShowError), this);
    g_signal_connect(session, "show-info", G_CALLBACK(onShowInfo), this);
    g_signal_connect(session, "completed", G_CALLBACK(onCompleted), this);

    m_session = session;
    m_active = true;
    polkit_agent_session_initiate(session);
    return true;
#else
    Q_UNUSED(identity);
    Q_UNUSED(cookie);
    if (error) {
        *error = QStringLiteral("polkit-not-available");
    }
    return false;
#endif
}

void AuthSession::cancel()
{
#ifdef SLM_HAVE_POLKIT_AGENT
    if (!m_session) {
        return;
    }
    polkit_agent_session_cancel(asSession(m_session));
    clearSession();
#endif
}

void AuthSession::respondPassword(const QString &password)
{
#ifdef SLM_HAVE_POLKIT_AGENT
    if (!m_session) {
        return;
    }
    QByteArray passwordUtf8 = password.toUtf8();
    polkit_agent_session_response(asSession(m_session), passwordUtf8.constData());
    clearByteArray(passwordUtf8);
#else
    Q_UNUSED(password);
#endif
}

bool AuthSession::isActive() const
{
    return m_active;
}

void AuthSession::handleBackendCompleted(bool gainedAuthorization)
{
    clearSession();
    Q_EMIT completed(gainedAuthorization);
}

void AuthSession::clearSession()
{
#ifdef SLM_HAVE_POLKIT_AGENT
    if (m_session) {
        PolkitAgentSession *session = asSession(m_session);
        g_signal_handlers_disconnect_by_data(session, this);
        g_object_unref(session);
        m_session = nullptr;
    }
#endif
    m_active = false;
}

} // namespace Slm::Login
