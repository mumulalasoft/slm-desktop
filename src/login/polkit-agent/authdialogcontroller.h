#pragma once

#include <QObject>
#include <QString>

namespace Slm::Login {

class AuthSession;

class AuthDialogController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString actionId READ actionId NOTIFY actionIdChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString identity READ identity NOTIFY identityChanged)
    Q_PROPERTY(QString prompt READ prompt NOTIFY promptChanged)
    Q_PROPERTY(bool promptEchoOn READ promptEchoOn NOTIFY promptEchoOnChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit AuthDialogController(QObject *parent = nullptr);

    void setSession(AuthSession *session);
    void beginRequest(const QString &actionId, const QString &message, const QString &identity);
    void endRequest();

    QString actionId() const;
    QString message() const;
    QString identity() const;
    QString prompt() const;
    bool promptEchoOn() const;
    bool busy() const;
    QString errorMessage() const;
    bool active() const;

    Q_INVOKABLE void authenticate(const QString &password);
    Q_INVOKABLE void cancel();

    void handleSessionRequest(const QString &prompt, bool echoOn);
    void handleSessionError(const QString &message);
    void handleSessionInfo(const QString &message);
    void handleSessionCompleted();

Q_SIGNALS:
    void actionIdChanged();
    void messageChanged();
    void identityChanged();
    void promptChanged();
    void promptEchoOnChanged();
    void busyChanged();
    void errorMessageChanged();
    void activeChanged();

private:
    void setPrompt(const QString &prompt, bool echoOn);
    void setBusy(bool busy);
    void setErrorMessage(const QString &message);

    AuthSession *m_session = nullptr;
    QString m_actionId;
    QString m_message;
    QString m_identity;
    QString m_prompt;
    QString m_errorMessage;
    bool m_promptEchoOn = false;
    bool m_busy = false;
    bool m_active = false;
};

} // namespace Slm::Login
