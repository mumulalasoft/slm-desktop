#pragma once

#include <QObject>
#include <QDateTime>
#include <QVariantList>

class PowerBridge;
class ScheduleController;
class SessionController;
class ShellStateController;

class PowerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString powerState READ powerState NOTIFY powerStateChanged)
    Q_PROPERTY(QString pendingAction READ pendingAction NOTIFY pendingActionChanged)
    Q_PROPERTY(bool overlayVisible READ overlayVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool powerDialogVisible READ powerDialogVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool safetyDialogVisible READ safetyDialogVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool scheduledDialogVisible READ scheduledDialogVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool blockingWarningVisible READ blockingWarningVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList remainingApps READ remainingApps NOTIFY remainingAppsChanged)

public:
    explicit PowerController(PowerBridge *powerBridge,
                             SessionController *sessionController,
                             ScheduleController *scheduleController,
                             ShellStateController *shellStateController,
                             QObject *parent = nullptr);

    QString powerState() const;
    QString pendingAction() const;
    bool overlayVisible() const;
    bool powerDialogVisible() const;
    bool safetyDialogVisible() const;
    bool scheduledDialogVisible() const;
    bool blockingWarningVisible() const;
    bool busy() const;
    QString lastError() const;
    QVariantList remainingApps() const;

    Q_INVOKABLE bool openPowerDialog(const QString &action = QString());
    Q_INVOKABLE bool openScheduledDialog(const QString &action);
    Q_INVOKABLE bool requestNow(const QString &action);
    Q_INVOKABLE bool scheduleAt(const QString &action, const QDateTime &executeAt);
    Q_INVOKABLE bool scheduleInMinutes(const QString &action, int minutes);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reviewApps();
    Q_INVOKABLE bool forceExecute();
    Q_INVOKABLE bool executeScheduledNow();

signals:
    void powerStateChanged(const QString &state);
    void pendingActionChanged(const QString &action);
    void overlayVisibleChanged();
    void busyChanged(bool busy);
    void lastErrorChanged(const QString &message);
    void remainingAppsChanged(const QVariantList &apps);

private:
    enum class State {
        Idle,
        PowerDialogOpen,
        ScheduledDialogOpen,
        SafetyCheck,
        WaitingAppsClose,
        ReadyToExecute,
        Executing,
        Cancelled
    };

    void setState(State state);
    void setPendingAction(const QString &action);
    void setBusy(bool busy);
    void setLastError(const QString &message);
    void setRemainingApps(const QVariantList &apps);
    void closeOverlays();
    bool beginSafetyCheck(const QString &action);
    bool executeAction(const QString &action);
    QString stateName(State state) const;

    PowerBridge *m_powerBridge = nullptr;
    SessionController *m_sessionController = nullptr;
    ScheduleController *m_scheduleController = nullptr;
    ShellStateController *m_shellStateController = nullptr;
    State m_state = State::Idle;
    QString m_pendingAction;
    QVariantList m_remainingApps;
    QString m_lastError;
    bool m_busy = false;
    bool m_powerDialogVisible = false;
    bool m_safetyDialogVisible = false;
    bool m_scheduledDialogVisible = false;
    bool m_blockingWarningVisible = false;
};
