#include "greeterapp.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

using namespace Qt::StringLiterals;

#ifdef SLM_HAVE_PAM
// Direct PAM path available — the slmpamsession.cpp is compiled in.
#define SLM_PAM_DIRECT 1
#endif

namespace {
QString resolveSessionBrokerCommand()
{
    const QString envOverride = qEnvironmentVariable("SLM_SESSION_BROKER_PATH").trimmed();
    if (!envOverride.isEmpty() && QFileInfo::exists(envOverride)) {
        return envOverride;
    }

    const QStringList candidates = {
        QStringLiteral("/usr/local/libexec/slm-session-broker-launch"),
        QStringLiteral("/usr/local/bin/slm-session-broker"),
        QStringLiteral("/usr/libexec/slm-session-broker"),
    };
    for (const QString &candidate : candidates) {
        QFileInfo fi(candidate);
        if (fi.exists() && fi.isExecutable()) {
            return candidate;
        }
    }

    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("slm-session-broker"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    return {};
}
}

namespace Slm::Login {

GreeterApp::GreeterApp(QObject *parent)
    : QObject(parent)
    , m_greetd(new GreetdClient(this))
{
    QString err;
    const bool stateOk = SessionStateIO::load(m_state, err);
    qInfo("state: load=%s crash_count=%d last_boot_status=%s config_pending=%s safe_mode_forced=%s recovery_reason=%s last_crash_reason=%s",
          stateOk ? "ok" : qPrintable(u"fail(%1)"_s.arg(err)),
          m_state.crashCount,
          qPrintable(m_state.lastBootStatus),
          m_state.configPending  ? "true" : "false",
          m_state.safeModeForced ? "true" : "false",
          qPrintable(m_state.recoveryReason),
          qPrintable(m_state.lastCrashReason));

    connect(m_greetd, &GreetdClient::connectedChanged,
            this, &GreeterApp::greetdConnectedChanged);
    connect(m_greetd, &GreetdClient::authMessage,
            this, &GreeterApp::onAuthMessage);
    connect(m_greetd, &GreetdClient::success,
            this, &GreeterApp::onSuccess);
    connect(m_greetd, &GreetdClient::error,
            this, &GreeterApp::onError);

#ifdef SLM_HAVE_PAM
    // Pre-allocate the PAM session object so it survives across login cycles.
    m_pamSession = new SlmPamSession(this);
    connect(m_pamSession, &SlmPamSession::sessionFinished,
            this, &GreeterApp::onPamSessionFinished);
    qInfo("greeter: direct PAM path available (SLM_HAVE_PAM)");
#endif
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
    // Greeter-local background asset (not tied to any specific DM theme layout).
    const QString qrcPath = QStringLiteral(
        "qrc:/qt/qml/SlmGreeter/Qml/greeter/assets/Background.jpeg");
    return qrcPath;
}

// ── Public invokables ─────────────────────────────────────────────────────────

void GreeterApp::login(const QString &username,
                       const QString &password,
                       const QString &mode)
{
    qInfo("login: user='%s' mode='%s' greetd_connected=%s pam_direct=%s",
          qPrintable(username), qPrintable(mode),
          m_greetd->isConnected() ? "yes" : "no",
          m_pamSession            ? "yes" : "no");

    if (m_loginStep != LoginStep::Idle) {
        qWarning("login: rejected — another login already in progress (step=%d)",
                 static_cast<int>(m_loginStep));
        return;
    }

    if (m_greetd->isConnected()) {
        // greetd path — existing behaviour.
        m_pendingPassword = password;
        m_pendingMode     = mode;
        m_loginStep       = LoginStep::WaitingAuthChallenge;
        qInfo("login: sending create_session for user='%s'", qPrintable(username));
        m_greetd->createSession(username);
    } else if (m_pamSession) {
        // Direct PAM path (no greetd available).
        loginViaPam(username, password, mode);
    } else {
        // Dev/test mode — no greetd and no PAM support compiled in.
        qWarning("login: no greetd and no PAM — simulating success (dev mode)");
        emit loginSuccess();
    }
}

void GreeterApp::loginViaPam(const QString &username, const QString &password,
                              const QString &mode)
{
#ifdef SLM_PAM_DIRECT
    qInfo("SLM-GREETER: direct PAM login for user='%s' mode='%s'",
          qPrintable(username), qPrintable(mode));

    m_loginStep = LoginStep::WaitingAuthChallenge;

    if (!m_pamSession->authenticate(username, password)) {
        m_loginStep = LoginStep::Idle;
        emit loginError(QStringLiteral("auth_error"),
                        QStringLiteral("Authentication failed"));
        return;
    }

    if (!m_pamSession->openSession()) {
        m_loginStep = LoginStep::Idle;
        emit loginError(QStringLiteral("session_error"),
                        QStringLiteral("Failed to open PAM session"));
        return;
    }

    const qint64 pid = m_pamSession->launchSession();
    if (pid < 0) {
        m_pamSession->closeSession();
        m_loginStep = LoginStep::Idle;
        emit loginError(QStringLiteral("session_start_error"),
                        QStringLiteral("Failed to launch user session"));
        return;
    }

    m_loginStep     = LoginStep::Idle;
    m_sessionActive = true;
    emit sessionActiveChanged();
    emit loginSuccess();
    qInfo("SLM-GREETER: user session launched (pid=%lld) — greeter waiting in background",
          (long long)pid);
#else
    Q_UNUSED(username) Q_UNUSED(password) Q_UNUSED(mode)
    qWarning("loginViaPam: called but SLM_HAVE_PAM not compiled in");
    emit loginError(QStringLiteral("no_pam"), QStringLiteral("PAM support not compiled"));
#endif
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
    // Don't log message content for "secret" type (password prompt).
    if (type == QStringLiteral("secret")) {
        qInfo("auth_message: type='secret' (content suppressed)");
    } else {
        qInfo("auth_message: type='%s' message='%s'", qPrintable(type), qPrintable(message));
    }
    emit authMessageReceived(type, message);

    if (m_loginStep == LoginStep::WaitingAuthChallenge) {
        if (type == QStringLiteral("secret") || type == QStringLiteral("visible")) {
            qInfo("auth_message: posting password response for type='%s'", qPrintable(type));
            m_greetd->postAuthResponse(m_pendingPassword);
        } else {
            qInfo("auth_message: type='%s' — not posting response", qPrintable(type));
        }
    } else {
        qWarning("auth_message: unexpected step=%d for type='%s'",
                 static_cast<int>(m_loginStep), qPrintable(type));
    }
}

void GreeterApp::onSuccess()
{
    qInfo("greetd_success: step=%d", static_cast<int>(m_loginStep));
    switch (m_loginStep) {
    case LoginStep::WaitingAuthChallenge: {
        m_loginStep = LoginStep::WaitingStartSession;
        const QString broker = resolveSessionBrokerCommand();
        qInfo("greetd_success: auth ok — resolved broker='%s'", qPrintable(broker));
        if (broker.isEmpty()) {
            qWarning("greetd_success: broker not found — cancelling session");
            m_loginStep = LoginStep::Idle;
            m_greetd->cancelSession();
            emit loginError(QStringLiteral("session_start_error"),
                            QStringLiteral("SLM session broker not found"));
            return;
        }
        qInfo("greetd_success: sending start_session broker='%s' mode='%s'",
              qPrintable(broker), qPrintable(m_pendingMode));
        // Pass session env via greetd IPC — greetd calls pam_putenv() for each
        // entry before pam_open_session(), so pam_systemd.so registers the
        // session as Type=wayland on seat0 (not tty).
        m_greetd->startSession(
            {broker, QStringLiteral("--mode"), m_pendingMode},
            {
                {QStringLiteral("XDG_SESSION_TYPE"),    QStringLiteral("wayland")},
                {QStringLiteral("XDG_SESSION_CLASS"),   QStringLiteral("user")},
                {QStringLiteral("XDG_SEAT"),            QStringLiteral("seat0")},
                {QStringLiteral("DESKTOP_SESSION"),     QStringLiteral("slm")},
                {QStringLiteral("XDG_SESSION_DESKTOP"), QStringLiteral("slm")},
                {QStringLiteral("SLM_OFFICIAL_SESSION"),QStringLiteral("1")},
            });
        break;
    }
    case LoginStep::WaitingStartSession:
        m_loginStep = LoginStep::Idle;
        qInfo("greetd_success: start_session confirmed — exiting greeter for session handoff");
        emit loginSuccess();
        QTimer::singleShot(0, [] {
            QCoreApplication::exit(0);
        });
        break;
    default:
        qWarning("greetd_success: unexpected success at step=%d (ignored)",
                 static_cast<int>(m_loginStep));
        break;
    }
}

void GreeterApp::onError(const QString &errorType, const QString &description)
{
    qWarning("greetd_error: type='%s' description='%s' step=%d",
             qPrintable(errorType), qPrintable(description),
             static_cast<int>(m_loginStep));
    m_loginStep = LoginStep::Idle;
    m_pendingPassword.clear();
    m_greetd->cancelSession();
    emit loginError(errorType, description);
}

void GreeterApp::onPamSessionFinished(int exitCode)
{
    qInfo("SLM-GREETER: user session finished (exit_code=%d) — returning to login screen",
          exitCode);
    m_sessionActive = false;
    emit sessionActiveChanged();
    // Greeter window should show the login page again; QML observes sessionActive.
}

} // namespace Slm::Login
