#pragma once

#include "appentry.h"
#include "appregistry.h"
#include "classificationengine.h"
#include "uiexposurepolicy.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace Slm::Appd {

struct LaunchRequest {
    QString appId;
    QString executableHint;
    QString desktopFile;
    qint64  pid = -1;
    qint64  timestampMs = 0;
};

// LifecycleEngine — drives AppState transitions and monitors processes.
//
// State machine:
//   CREATED → LAUNCHING → RUNNING → IDLE → BACKGROUND
//           → UNRESPONSIVE → CRASHED → TERMINATED
class LifecycleEngine : public QObject
{
    Q_OBJECT
public:
    explicit LifecycleEngine(AppRegistry *registry,
                             ClassificationEngine *classifier,
                             QObject *parent = nullptr);

    // ── External events ──────────────────────────────────────────────────────

    // Called when user or system requests app launch.
    void onLaunch(const LaunchRequest &req);

    // Called when compositor reports a new window for a PID / appId.
    void onWindowCreated(const QString &appId,
                         qint64 pid,
                         const WindowInfo &window,
                         const QStringList &desktopCategories = {});

    // Called when compositor reports window closed.
    void onWindowClosed(const QString &windowId);

    // Called when compositor reports focus change.
    void onFocusChanged(const QString &windowId, bool focused);

    // Called periodically from process monitor.
    void onProcessExited(qint64 pid, int exitCode);

    // Called from hang detector when app is unresponsive.
    void onUnresponsive(const QString &appId);

    // Power context: true → suspend background apps.
    void setLowPowerMode(bool lowPower);

    // ── Internal polling ─────────────────────────────────────────────────────

    // Called by the daemon to drive PID lifecycle checks.
    void pollProcessStates();

signals:
    void appAdded(const QString &appId);
    void appRemoved(const QString &appId);
    void appStateChanged(const QString &appId, const QString &state);
    void appWindowsChanged(const QString &appId);
    void appFocusChanged(const QString &appId, bool focused);
    void appUpdated(const QString &appId);

private:
    void transitionState(AppEntry *entry, AppState next);
    void classifyAndExpose(AppEntry *entry,
                           const QString &desktopFile,
                           const QStringList &desktopCategories = {});
    QString resolveAppIdFromPid(qint64 pid) const;

    // Track PID → appId for quick reverse lookup.
    QHash<qint64, QString> m_pidToAppId;

    AppRegistry         *m_registry;
    ClassificationEngine *m_classifier;
    bool                 m_lowPower = false;
};

} // namespace Slm::Appd
