#include "greeterapp.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include "src/core/system/componentregistry.h"
#include "src/core/system/dependencyguard.h"

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
    SessionStateIO::load(m_state, err);

    connect(m_greetd, &GreetdClient::connectedChanged,
            this, &GreeterApp::greetdConnectedChanged);
    connect(m_greetd, &GreetdClient::authMessage,
            this, &GreeterApp::onAuthMessage);
    connect(m_greetd, &GreetdClient::success,
            this, &GreeterApp::onSuccess);
    connect(m_greetd, &GreetdClient::error,
            this, &GreeterApp::onError);

    m_missingComponents = checkRequiredComponents();
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

QVariantList GreeterApp::missingComponents() const
{
    return m_missingComponents;
}

// ── Public invokables ─────────────────────────────────────────────────────────

void GreeterApp::login(const QString &username,
                       const QString &password,
                       const QString &mode)
{
    qWarning("slm-greeter: login request user='%s' mode='%s' connected=%d",
             qPrintable(username),
             qPrintable(mode),
             m_greetd->isConnected() ? 1 : 0);
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

QVariantList GreeterApp::refreshMissingComponents()
{
    const QVariantList next = checkRequiredComponents();
    if (next != m_missingComponents) {
        m_missingComponents = next;
        emit missingComponentsChanged();
    }
    return m_missingComponents;
}

QVariantMap GreeterApp::installMissingComponent(const QString &componentId)
{
    const QString id = componentId.trimmed().toLower();
    Slm::System::ComponentRequirement req;
    if (Slm::System::ComponentRegistry::findById(id, &req) && req.autoInstallable) {
        const QVariantMap result = Slm::System::installComponentWithPolkit(req);
        refreshMissingComponents();
        return result;
    }
    return QVariantMap{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("unsupported-component")},
        {QStringLiteral("componentId"), id},
    };
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

QVariantList GreeterApp::checkRequiredComponents() const
{
    QVariantList out;

    const QString broker = resolveSessionBrokerCommand();
    if (broker.trimmed().isEmpty()) {
        out.push_back(QVariantMap{
            {QStringLiteral("componentId"), QStringLiteral("slm-session-broker")},
            {QStringLiteral("title"), QStringLiteral("SLM Session Broker")},
            {QStringLiteral("description"), QStringLiteral("Komponen inti untuk memulai sesi desktop tidak ditemukan.")},
            {QStringLiteral("packageName"), QStringLiteral("slm-desktop")},
            {QStringLiteral("autoInstallable"), false},
            {QStringLiteral("guidance"), QStringLiteral("Masuk ke Recovery lalu pasang ulang paket slm-desktop.")},
        });
    }

    const QList<Slm::System::ComponentRequirement> requirements =
        Slm::System::ComponentRegistry::forDomain(QStringLiteral("greeter"));
    for (const Slm::System::ComponentRequirement &req : requirements) {
        const QVariantMap result = Slm::System::checkComponent(req);
        if (!result.value(QStringLiteral("ready")).toBool()) {
            out.push_back(result);
        }
    }

    return out;
}

void GreeterApp::onAuthMessage(const QString &type, const QString &message)
{
    qWarning("slm-greeter: auth message type='%s' message='%s'",
             qPrintable(type),
             qPrintable(message));
    emit authMessageReceived(type, message);

    if (m_loginStep == LoginStep::WaitingAuthChallenge) {
        if (type == QStringLiteral("secret") || type == QStringLiteral("visible")) {
            qWarning("slm-greeter: posting auth response for type='%s'", qPrintable(type));
            m_greetd->postAuthResponse(m_pendingPassword);
        }
    }
}

void GreeterApp::onSuccess()
{
    qWarning("slm-greeter: success callback step=%d", static_cast<int>(m_loginStep));
    switch (m_loginStep) {
    case LoginStep::WaitingAuthChallenge:
        m_loginStep = LoginStep::WaitingStartSession;
        {
        const QString broker = resolveSessionBrokerCommand();
        qWarning("slm-greeter: resolved session broker='%s'", qPrintable(broker));
        if (broker.isEmpty()) {
            m_loginStep = LoginStep::Idle;
            m_greetd->cancelSession();
            emit loginError(QStringLiteral("session_start_error"),
                            QStringLiteral("SLM session broker not found"));
            return;
        }
        m_greetd->startSession(
            {broker,
             QStringLiteral("--mode"),
             m_pendingMode},
            {});
        qWarning("slm-greeter: startSession sent with mode='%s'", qPrintable(m_pendingMode));
        }
        break;
    case LoginStep::WaitingStartSession:
        m_loginStep = LoginStep::Idle;
        qWarning("slm-greeter: login success final");
        emit loginSuccess();
        break;
    default:
        qWarning("slm-greeter: success callback ignored at idle");
        break;
    }
}

void GreeterApp::onError(const QString &errorType, const QString &description)
{
    qWarning("slm-greeter: error type='%s' description='%s' step=%d",
             qPrintable(errorType),
             qPrintable(description),
             static_cast<int>(m_loginStep));
    m_loginStep = LoginStep::Idle;
    m_pendingPassword.clear();
    m_greetd->cancelSession();
    emit loginError(errorType, description);
}

} // namespace Slm::Login
