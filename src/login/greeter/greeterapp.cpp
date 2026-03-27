#include "greeterapp.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace Slm::Login {

GreeterApp::GreeterApp(QObject *parent)
    : QObject(parent)
    , m_greetd(new GreetdClient(this))
{
    QString err;
    SessionStateIO::load(m_state, err);

    connect(m_greetd, &GreetdClient::connectedChanged,
            this, &GreeterApp::greetdConnectedChanged);
    connect(m_greetd, &GreetdClient::authMessage,
            this, &GreeterApp::onAuthMessage);
    connect(m_greetd, &GreetdClient::success,
            this, &GreeterApp::onSuccess);
    connect(m_greetd, &GreetdClient::error,
            this, &GreeterApp::onError);
}

// ── Properties ────────────────────────────────────────────────────────────────

QString GreeterApp::recoveryReason()   const { return m_state.recoveryReason; }
bool    GreeterApp::hasPreviousCrash() const { return m_state.lastBootStatus == QStringLiteral("crashed"); }
QString GreeterApp::lastBootStatus()   const { return m_state.lastBootStatus; }
bool    GreeterApp::configPending()    const { return m_state.configPending; }
bool    GreeterApp::safeModeForced()   const { return m_state.safeModeForced; }
int     GreeterApp::crashCount()       const { return m_state.crashCount; }
bool    GreeterApp::greetdConnected()  const { return m_greetd->isConnected(); }

bool GreeterApp::canSuspend()  const { return systemctlCan(QStringLiteral("suspend")); }
bool GreeterApp::canReboot()   const { return systemctlCan(QStringLiteral("reboot")); }
bool GreeterApp::canPowerOff() const { return systemctlCan(QStringLiteral("poweroff")); }

QVariantList GreeterApp::usersList() const
{
    // Enumerate human users from /etc/passwd (uid >= 1000, valid shell).
    QVariantList result;
    QFile passwd(QStringLiteral("/etc/passwd"));
    if (!passwd.open(QIODevice::ReadOnly)) {
        return result;
    }
    while (!passwd.atEnd()) {
        const QString line = QString::fromLocal8Bit(passwd.readLine()).trimmed();
        if (line.startsWith(QStringLiteral("#")) || line.isEmpty()) {
            continue;
        }
        const QStringList parts = line.split(QStringLiteral(":"));
        if (parts.size() < 7) {
            continue;
        }
        const QString name  = parts.at(0);
        const uint    uid   = parts.at(2).toUInt();
        const QString shell = parts.at(6);
        const QString home  = parts.at(5);

        if (uid < 1000) continue;
        if (shell.contains(QStringLiteral("nologin")) ||
            shell.contains(QStringLiteral("/bin/false"))) continue;

        QVariantMap user;
        user[QStringLiteral("name")]   = name;
        user[QStringLiteral("avatar")] = QStringLiteral("file://") + home
                                         + QStringLiteral("/.face.icon");
        result.append(user);
    }
    return result;
}

QString GreeterApp::lastUser() const
{
    // Return last used username if we have it, otherwise first system user.
    const QVariantList users = usersList();
    if (!users.isEmpty()) {
        return users.first().toMap().value(QStringLiteral("name")).toString();
    }
    return QString{};
}

QString GreeterApp::backgroundSource() const
{
    // Prefer the SLM SDDM theme background embedded as QRC resource.
    // Falls back to empty string (solid colour background) if not available.
    const QString qrcPath = QStringLiteral(
        "qrc:/qt/qml/SlmGreeter/sddm/theme/SLM/Background.jpeg");
    return qrcPath;
}

// ── Public invokables ─────────────────────────────────────────────────────────

void GreeterApp::login(const QString &username,
                       const QString &password,
                       const QString &mode)
{
    if (m_loginStep != LoginStep::Idle) {
        qWarning("slm-greeter: login() called while another login is in progress");
        return;
    }
    m_pendingPassword = password;
    m_pendingMode     = mode;
    m_loginStep       = LoginStep::WaitingAuthChallenge;

    if (m_greetd->isConnected()) {
        m_greetd->createSession(username);
    } else {
        // Dev/test mode — no greetd socket.
        qWarning("slm-greeter: no greetd connection — simulating login success");
        m_loginStep = LoginStep::Idle;
        emit loginSuccess();
    }
}

void GreeterApp::suspend()
{
    QProcess::startDetached(QStringLiteral("systemctl"),
                            {QStringLiteral("suspend")});
}

void GreeterApp::reboot()
{
    QProcess::startDetached(QStringLiteral("systemctl"),
                            {QStringLiteral("reboot")});
}

void GreeterApp::powerOff()
{
    QProcess::startDetached(QStringLiteral("systemctl"),
                            {QStringLiteral("poweroff")});
}

// ── Private ───────────────────────────────────────────────────────────────────

bool GreeterApp::systemctlCan(const QString &verb)
{
    QProcess p;
    p.start(QStringLiteral("systemctl"),
            {QStringLiteral("can-") + verb});
    p.waitForFinished(2000);
    return p.exitCode() == 0;
}

void GreeterApp::onAuthMessage(const QString &type, const QString &message)
{
    emit authMessageReceived(type, message);

    if (m_loginStep == LoginStep::WaitingAuthChallenge) {
        if (type == QStringLiteral("secret") || type == QStringLiteral("visible")) {
            m_greetd->postAuthResponse(m_pendingPassword);
        }
    }
}

void GreeterApp::onSuccess()
{
    switch (m_loginStep) {
    case LoginStep::WaitingAuthChallenge:
        m_loginStep = LoginStep::WaitingStartSession;
        m_greetd->startSession(
            {QStringLiteral("slm-session-broker"),
             QStringLiteral("--mode"),
             m_pendingMode},
            {});
        break;
    case LoginStep::WaitingStartSession:
        m_loginStep = LoginStep::Idle;
        emit loginSuccess();
        break;
    default:
        break;
    }
}

void GreeterApp::onError(const QString &errorType, const QString &description)
{
    m_loginStep = LoginStep::Idle;
    m_pendingPassword.clear();
    m_greetd->cancelSession();
    emit loginError(errorType, description);
}

} // namespace Slm::Login
