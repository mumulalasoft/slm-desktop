#include "systemenvcontroller.h"

#include <QProcess>

static constexpr char kActionId[] = "org.slm.environment.write-system";

SystemEnvController::SystemEnvController(EnvServiceClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &EnvServiceClient::systemVarsChanged,
            this, &SystemEnvController::loadVars);
    connect(m_client, &EnvServiceClient::serviceAvailableChanged,
            this, &SystemEnvController::loadVars);

    loadVars();
}

// ── Authorization ─────────────────────────────────────────────────────────────

void SystemEnvController::requestAuth()
{
    if (m_authPending || m_authorized)
        return;

    // Respect a dev override that bypasses pkcheck (mirrors SettingsPolkitBridge).
    const QString devOverride = qEnvironmentVariable("SLM_SETTINGS_AUTH_ALLOW_ALL")
                                    .trimmed().toLower();
    if (devOverride == QLatin1String("1")
            || devOverride == QLatin1String("true")
            || devOverride == QLatin1String("yes")) {
        m_authorized = true;
        emit authorizedChanged();
        return;
    }

    m_authPending = true;
    emit authPendingChanged();

    auto *proc = new QProcess(this);
    m_pkcheckProc = proc;

    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        m_pkcheckProc = nullptr;
        m_authPending = false;
        emit authPendingChanged();

        const bool ok = (status == QProcess::NormalExit && exitCode == 0);
        if (ok) {
            m_authorized = true;
            emit authorizedChanged();
            loadVars();
        } else {
            const QString reason = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            setLastError(reason.isEmpty()
                         ? QStringLiteral("Authorization denied")
                         : reason);
        }
        proc->deleteLater();
    });

    proc->start(QStringLiteral("pkcheck"),
                { QStringLiteral("--action-id"), QLatin1String(kActionId),
                  QStringLiteral("--allow-user-interaction") });

    if (!proc->waitForStarted(1500)) {
        m_pkcheckProc = nullptr;
        m_authPending = false;
        emit authPendingChanged();
        setLastError(QStringLiteral("pkcheck not available"));
        proc->deleteLater();
    }
}

// ── Var operations ────────────────────────────────────────────────────────────

bool SystemEnvController::writeVar(const QString &key, const QString &value,
                                    const QString &comment, const QString &mergeMode,
                                    bool enabled)
{
    if (!m_authorized) {
        setLastError(QStringLiteral("Not authorized"));
        return false;
    }
    const bool ok = m_client->writeSystemVar(key, value, comment, mergeMode, enabled);
    if (!ok)
        setLastError(m_client->lastError());
    else
        loadVars();
    return ok;
}

bool SystemEnvController::deleteVar(const QString &key)
{
    if (!m_authorized) {
        setLastError(QStringLiteral("Not authorized"));
        return false;
    }
    const bool ok = m_client->deleteSystemVar(key);
    if (!ok)
        setLastError(m_client->lastError());
    else
        loadVars();
    return ok;
}

void SystemEnvController::reload()
{
    loadVars();
}

// ── Private ───────────────────────────────────────────────────────────────────

void SystemEnvController::loadVars()
{
    m_vars = m_client->getSystemVars();
    emit varsChanged();
}

void SystemEnvController::setLastError(const QString &msg)
{
    m_lastError = msg;
    emit lastErrorChanged();
}
