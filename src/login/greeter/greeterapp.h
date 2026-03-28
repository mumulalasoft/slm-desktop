#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include "greetdclient.h"
#include "src/login/libslmlogin/slmsessionstate.h"

namespace Slm::Login {

// GreeterApp is the C++ bridge exposed to QML as "GreeterApp".
class GreeterApp : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString      recoveryReason   READ recoveryReason   CONSTANT)
    Q_PROPERTY(bool         hasPreviousCrash READ hasPreviousCrash CONSTANT)
    Q_PROPERTY(QString      lastBootStatus   READ lastBootStatus   CONSTANT)
    Q_PROPERTY(bool         configPending    READ configPending    CONSTANT)
    Q_PROPERTY(bool         safeModeForced   READ safeModeForced   CONSTANT)
    Q_PROPERTY(int          crashCount       READ crashCount       CONSTANT)
    Q_PROPERTY(bool         greetdConnected  READ greetdConnected  NOTIFY greetdConnectedChanged)
    Q_PROPERTY(bool         canSuspend       READ canSuspend       CONSTANT)
    Q_PROPERTY(bool         canReboot        READ canReboot        CONSTANT)
    Q_PROPERTY(bool         canPowerOff      READ canPowerOff      CONSTANT)
    Q_PROPERTY(QVariantList usersList        READ usersList        CONSTANT)
    Q_PROPERTY(QString      lastUser         READ lastUser         CONSTANT)
    Q_PROPERTY(QString      backgroundSource READ backgroundSource CONSTANT)

public:
    explicit GreeterApp(QObject *parent = nullptr);

    QString      recoveryReason()   const;
    bool         hasPreviousCrash() const;
    QString      lastBootStatus()   const;
    bool         configPending()    const;
    bool         safeModeForced()   const;
    int          crashCount()       const;
    bool         greetdConnected()  const;
    bool         canSuspend()       const;
    bool         canReboot()        const;
    bool         canPowerOff()      const;
    QVariantList usersList()        const;
    QString      lastUser()         const;
    QString      backgroundSource() const;

    Q_INVOKABLE void login(const QString &username,
                           const QString &password,
                           const QString &mode);
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void powerOff();

signals:
    void greetdConnectedChanged();
    void authMessageReceived(const QString &type, const QString &message);
    void loginSuccess();
    void loginError(const QString &errorType, const QString &description);

private slots:
    void onAuthMessage(const QString &type, const QString &message);
    void onSuccess();
    void onError(const QString &errorType, const QString &description);

private:
    enum class LoginStep { Idle, WaitingAuthChallenge, WaitingStartSession };

    static bool systemctlCan(const QString &verb);

    GreetdClient *m_greetd    = nullptr;
    SessionState  m_state;
    QString       m_pendingPassword;
    QString       m_pendingMode;
    LoginStep     m_loginStep = LoginStep::Idle;
};

} // namespace Slm::Login
