#include "powercontroller.h"

#include "powerbridge.h"
#include "schedulecontroller.h"
#include "sessioncontroller.h"
#include "../../core/shell/shellstatecontroller.h"

#include <QDateTime>
#include <QProcess>
#include <QTimer>

PowerController::PowerController(PowerBridge *powerBridge,
                                 SessionController *sessionController,
                                 ScheduleController *scheduleController,
                                 ShellStateController *shellStateController,
                                 QObject *parent)
    : QObject(parent)
    , m_powerBridge(powerBridge)
    , m_sessionController(sessionController)
    , m_scheduleController(scheduleController)
    , m_shellStateController(shellStateController)
{
    if (m_sessionController) {
        connect(m_sessionController, &SessionController::preparePowerActionFinished,
                this, [this](const QString &action, const QVariantList &apps) {
            if (m_state != State::SafetyCheck && m_state != State::WaitingAppsClose) {
                return;
            }
            setRemainingApps(apps);
            if (!apps.isEmpty()) {
                m_powerDialogVisible = false;
                m_safetyDialogVisible = true;
                setState(State::WaitingAppsClose);
                setBusy(false);
                emit overlayVisibleChanged();
                return;
            }
            setState(State::ReadyToExecute);
            executeAction(action);
        });
    }
    if (m_scheduleController) {
        connect(m_scheduleController, &ScheduleController::executeScheduledActionRequested,
                this, [this](const QString &action) {
            requestNow(action);
        });
        connect(m_scheduleController, &ScheduleController::blockingWarningRequested,
                this, [this](const QString &action, int) {
            if (m_state == State::Executing) {
                return;
            }
            setPendingAction(action);
            m_blockingWarningVisible = true;
            if (m_shellStateController) {
                m_shellStateController->setTopOverlayActive(true);
            }
            emit overlayVisibleChanged();
        });
    }
}

QString PowerController::powerState() const { return stateName(m_state); }
QString PowerController::pendingAction() const { return m_pendingAction; }
bool PowerController::overlayVisible() const { return m_powerDialogVisible || m_safetyDialogVisible || m_scheduledDialogVisible || m_blockingWarningVisible; }
bool PowerController::powerDialogVisible() const { return m_powerDialogVisible; }
bool PowerController::safetyDialogVisible() const { return m_safetyDialogVisible; }
bool PowerController::scheduledDialogVisible() const { return m_scheduledDialogVisible; }
bool PowerController::blockingWarningVisible() const { return m_blockingWarningVisible; }
bool PowerController::busy() const { return m_busy; }
QString PowerController::lastError() const { return m_lastError; }
QVariantList PowerController::remainingApps() const { return m_remainingApps; }

bool PowerController::openPowerDialog(const QString &action)
{
    if (m_state != State::Idle && m_state != State::Cancelled) {
        return false;
    }
    if (m_shellStateController) {
        m_shellStateController->setAppDeckVisible(false);
        m_shellStateController->setDockHoveredItem(QString());
        m_shellStateController->setDockExpandedItem(QString());
        m_shellStateController->setDockActiveItem(QString());
    }
    setPendingAction(action.trimmed().toLower());
    m_powerDialogVisible = true;
    m_safetyDialogVisible = false;
    m_scheduledDialogVisible = false;
    m_blockingWarningVisible = false;
    setState(State::PowerDialogOpen);
    if (m_shellStateController) {
        m_shellStateController->setTopOverlayActive(true);
    }
    emit overlayVisibleChanged();
    return true;
}

bool PowerController::openScheduledDialog(const QString &action)
{
    if (m_state != State::PowerDialogOpen && m_state != State::Idle) {
        return false;
    }
    setPendingAction(action.trimmed().toLower());
    m_powerDialogVisible = false;
    m_scheduledDialogVisible = true;
    if (m_shellStateController) {
        m_shellStateController->setTopOverlayActive(true);
    }
    setState(State::ScheduledDialogOpen);
    emit overlayVisibleChanged();
    return true;
}

bool PowerController::requestNow(const QString &action)
{
    const QString normalized = action.trimmed().toLower();
    if (normalized.isEmpty() || m_state == State::Executing || m_busy) {
        return false;
    }
    setPendingAction(normalized);
    if (normalized == QStringLiteral("sleep")) {
        m_powerDialogVisible = false;
        m_scheduledDialogVisible = false;
        m_blockingWarningVisible = false;
        emit overlayVisibleChanged();
        setState(State::ReadyToExecute);
        return executeAction(normalized);
    }
    m_powerDialogVisible = true;
    m_scheduledDialogVisible = false;
    m_blockingWarningVisible = false;
    emit overlayVisibleChanged();
    return beginSafetyCheck(normalized);
}

bool PowerController::scheduleAt(const QString &action, const QDateTime &executeAt)
{
    if (!m_scheduleController || m_state == State::Executing) {
        return false;
    }
    const bool ok = m_scheduleController->schedule(action, QStringLiteral("AtTime"), executeAt);
    if (ok) {
        closeOverlays();
    }
    return ok;
}

bool PowerController::scheduleInMinutes(const QString &action, int minutes)
{
    if (!m_scheduleController || m_state == State::Executing) {
        return false;
    }
    const bool ok = m_scheduleController->scheduleInMinutes(action, minutes);
    if (ok) {
        closeOverlays();
    }
    return ok;
}

void PowerController::cancel()
{
    closeOverlays();
    setBusy(false);
    setRemainingApps({});
    setState(State::Cancelled);
    QTimer::singleShot(0, this, [this]() { setState(State::Idle); });
}

void PowerController::reviewApps()
{
    cancel();
}

bool PowerController::forceExecute()
{
    if (m_pendingAction.isEmpty() || m_state == State::Executing) {
        return false;
    }
    if (m_sessionController) {
        m_sessionController->terminateRemainingApps(m_pendingAction);
    }
    setState(State::ReadyToExecute);
    return executeAction(m_pendingAction);
}

bool PowerController::executeScheduledNow()
{
    if (!m_scheduleController || !m_scheduleController->hasPending()) {
        return false;
    }
    const QString action = m_scheduleController->action();
    m_scheduleController->cancelSchedule();
    return requestNow(action);
}

void PowerController::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit powerStateChanged(stateName(m_state));
}

void PowerController::setPendingAction(const QString &action)
{
    if (m_pendingAction == action) {
        return;
    }
    m_pendingAction = action;
    emit pendingActionChanged(m_pendingAction);
}

void PowerController::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(m_busy);
}

void PowerController::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged(m_lastError);
}

void PowerController::setRemainingApps(const QVariantList &apps)
{
    m_remainingApps = apps;
    emit remainingAppsChanged(m_remainingApps);
}

void PowerController::closeOverlays()
{
    m_powerDialogVisible = false;
    m_safetyDialogVisible = false;
    m_scheduledDialogVisible = false;
    m_blockingWarningVisible = false;
    emit overlayVisibleChanged();
    if (m_shellStateController) {
        m_shellStateController->setTopOverlayActive(overlayVisible());
    }
}

bool PowerController::beginSafetyCheck(const QString &action)
{
    if (!m_sessionController) {
        setState(State::ReadyToExecute);
        return executeAction(action);
    }
    setState(State::SafetyCheck);
    setBusy(true);
    return m_sessionController->preparePowerAction(action);
}

bool PowerController::executeAction(const QString &action)
{
    if (m_state == State::Executing) {
        return false;
    }
    closeOverlays();
    setState(State::Executing);
    setBusy(true);
    bool ok = false;
    if (action == QStringLiteral("shutdown")) {
        ok = m_powerBridge && m_powerBridge->powerOff();
    } else if (action == QStringLiteral("restart")) {
        ok = m_powerBridge && m_powerBridge->reboot();
    } else if (action == QStringLiteral("sleep")) {
        ok = m_powerBridge && m_powerBridge->sleep();
    } else if (action == QStringLiteral("logout")) {
        ok = m_powerBridge && m_powerBridge->logout();
    }
    if (!ok) {
        setLastError(QStringLiteral("Power action could not be started."));
        setBusy(false);
        setState(State::Idle);
        return false;
    }
    return true;
}

QString PowerController::stateName(State state) const
{
    switch (state) {
    case State::Idle: return QStringLiteral("Idle");
    case State::PowerDialogOpen: return QStringLiteral("PowerDialogOpen");
    case State::ScheduledDialogOpen: return QStringLiteral("ScheduledDialogOpen");
    case State::SafetyCheck: return QStringLiteral("SafetyCheck");
    case State::WaitingAppsClose: return QStringLiteral("WaitingAppsClose");
    case State::ReadyToExecute: return QStringLiteral("ReadyToExecute");
    case State::Executing: return QStringLiteral("Executing");
    case State::Cancelled: return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Idle");
}
