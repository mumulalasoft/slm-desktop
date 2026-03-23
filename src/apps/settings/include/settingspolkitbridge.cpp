#include "settingspolkitbridge.h"

#include <QProcess>
#include <QMetaObject>

SettingsPolkitBridge::SettingsPolkitBridge(QObject *parent)
    : QObject(parent)
{
}

QString SettingsPolkitBridge::requestAuthorization(const QString &actionId,
                                                   const QString &moduleId,
                                                   const QString &settingId)
{
    const QString trimmedAction = actionId.trimmed();
    const QString requestId = QStringLiteral("auth-%1").arg(++m_counter);

    auto emitLater = [this, requestId, moduleId, settingId](bool allowed, const QString &reason) {
        QMetaObject::invokeMethod(this, [this, requestId, moduleId, settingId, allowed, reason]() {
            emit authorizationFinished(requestId, moduleId, settingId, allowed, reason);
        }, Qt::QueuedConnection);
    };

    if (trimmedAction.isEmpty()) {
        emitLater(true, QStringLiteral("no-authorization-required"));
        return requestId;
    }

    const QString devOverride = qEnvironmentVariable("SLM_SETTINGS_AUTH_ALLOW_ALL").trimmed().toLower();
    if (devOverride == QStringLiteral("1")
        || devOverride == QStringLiteral("true")
        || devOverride == QStringLiteral("yes")) {
        emitLater(true, QStringLiteral("dev-override-allow-all"));
        return requestId;
    }

    auto *proc = new QProcess(this);
    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, proc, requestId, moduleId, settingId](int exitCode, QProcess::ExitStatus status) {
                const bool ok = (status == QProcess::NormalExit && exitCode == 0);
                QString reason;
                if (ok) {
                    reason = QStringLiteral("authorized");
                } else {
                    const QString stdErr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
                    reason = stdErr.isEmpty() ? QStringLiteral("authorization-denied") : stdErr;
                }
                emit authorizationFinished(requestId, moduleId, settingId, ok, reason);
                proc->deleteLater();
            });

    const QString program = QStringLiteral("pkcheck");
    const QStringList args{
        QStringLiteral("--action-id"), trimmedAction,
        QStringLiteral("--allow-user-interaction"),
    };
    proc->start(program, args);
    if (!proc->waitForStarted(1500)) {
        emitLater(false, QStringLiteral("pkcheck-unavailable"));
        proc->deleteLater();
    }
    return requestId;
}
