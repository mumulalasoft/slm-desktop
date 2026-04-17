#include "authdialogcontroller.h"

#include "authsession.h"

namespace Slm::Login {

AuthDialogController::AuthDialogController(QObject *parent)
    : QObject(parent)
{
}

void AuthDialogController::setSession(AuthSession *session)
{
    if (m_session == session) {
        return;
    }
    m_session = session;
}

void AuthDialogController::beginRequest(const QString &actionId, const QString &message, const QString &identity)
{
    const bool actionChanged = (m_actionId != actionId);
    const bool messageChangedLocal = (m_message != message);
    const bool identityChangedLocal = (m_identity != identity);
    const bool activeChangedLocal = !m_active;

    m_actionId = actionId;
    m_message = message;
    m_identity = identity;
    m_active = true;
    setBusy(false);
    setErrorMessage(QString());

    if (actionChanged) {
        Q_EMIT actionIdChanged();
    }
    if (messageChangedLocal) {
        Q_EMIT messageChanged();
    }
    if (identityChangedLocal) {
        Q_EMIT identityChanged();
    }
    if (activeChangedLocal) {
        Q_EMIT activeChanged();
    }
}

void AuthDialogController::endRequest()
{
    const bool hadActive = m_active;
    m_active = false;
    setBusy(false);
    setPrompt(QString(), false);
    setErrorMessage(QString());
    if (hadActive) {
        Q_EMIT activeChanged();
    }
}

QString AuthDialogController::actionId() const
{
    return m_actionId;
}

QString AuthDialogController::message() const
{
    return m_message;
}

QString AuthDialogController::identity() const
{
    return m_identity;
}

QString AuthDialogController::prompt() const
{
    return m_prompt;
}

bool AuthDialogController::promptEchoOn() const
{
    return m_promptEchoOn;
}

bool AuthDialogController::busy() const
{
    return m_busy;
}

QString AuthDialogController::errorMessage() const
{
    return m_errorMessage;
}

bool AuthDialogController::active() const
{
    return m_active;
}

void AuthDialogController::authenticate(const QString &password)
{
    if (!m_session || !m_active) {
        return;
    }
    setBusy(true);
    setErrorMessage(QString());
    m_session->respondPassword(password);
}

void AuthDialogController::cancel()
{
    if (!m_session || !m_active) {
        return;
    }
    m_session->cancel();
    endRequest();
}

void AuthDialogController::handleSessionRequest(const QString &prompt, bool echoOn)
{
    setBusy(false);
    setPrompt(prompt, echoOn);
}

void AuthDialogController::handleSessionError(const QString &message)
{
    setBusy(false);
    setErrorMessage(message);
}

void AuthDialogController::handleSessionInfo(const QString &message)
{
    Q_UNUSED(message);
}

void AuthDialogController::handleSessionCompleted()
{
    endRequest();
}

void AuthDialogController::setPrompt(const QString &prompt, bool echoOn)
{
    const bool promptChangedLocal = (m_prompt != prompt);
    const bool echoChanged = (m_promptEchoOn != echoOn);
    m_prompt = prompt;
    m_promptEchoOn = echoOn;
    if (promptChangedLocal) {
        Q_EMIT promptChanged();
    }
    if (echoChanged) {
        Q_EMIT promptEchoOnChanged();
    }
}

void AuthDialogController::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

void AuthDialogController::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    Q_EMIT errorMessageChanged();
}

} // namespace Slm::Login
