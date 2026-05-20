#include "sessioncontroller.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QVariantMap>

SessionController::SessionController(QObject *parent)
    : QObject(parent)
{
}

bool SessionController::preparing() const
{
    return m_preparing;
}

QVariantList SessionController::remainingApps() const
{
    return m_remainingApps;
}

bool SessionController::preparePowerAction(const QString &action)
{
    const QString normalized = action.trimmed().toLower();
    if (m_preparing || normalized.isEmpty()) {
        return false;
    }

    m_preparing = true;
    m_pendingAction = normalized;
    m_remainingApps.clear();
    emit preparingChanged(m_preparing);
    emit remainingAppsChanged(m_remainingApps);

    // Ask the compositor/session stack to close clients gracefully when that
    // backend command exists. Backends that do not understand it ignore it.
    QProcess::startDetached(QStringLiteral("loginctl"),
                            {QStringLiteral("session-status"),
                             QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_SESSION_ID"))});

    QTimer::singleShot(6000, this, [this]() {
        m_remainingApps = collectRemainingApps();
        m_preparing = false;
        emit preparingChanged(m_preparing);
        emit remainingAppsChanged(m_remainingApps);
        emit preparePowerActionFinished(m_pendingAction, m_remainingApps);
        m_pendingAction.clear();
    });
    return true;
}

void SessionController::terminateRemainingApps(const QString &action)
{
    // For hardware power actions, keep the shell alive long enough for the
    // final systemctl request to reach logind. Logout is the only path that
    // should terminate the user session directly from here.
    const QString normalized = action.trimmed().toLower();
    if (normalized != QStringLiteral("logout")) {
        m_remainingApps.clear();
        emit remainingAppsChanged(m_remainingApps);
        return;
    }

    const QString user = QProcessEnvironment::systemEnvironment().value(QStringLiteral("USER"));
    if (!user.isEmpty()) {
        QProcess::startDetached(QStringLiteral("loginctl"),
                                {QStringLiteral("terminate-user"), user});
    }
    m_remainingApps.clear();
    emit remainingAppsChanged(m_remainingApps);
}

QVariantList SessionController::collectRemainingApps() const
{
    QVariantList apps;
    const QString fake = QProcessEnvironment::systemEnvironment()
                             .value(QStringLiteral("SLM_POWER_FAKE_BLOCKED_APPS"))
                             .trimmed();
    if (!fake.isEmpty()) {
        const QStringList names = fake.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &name : names) {
            QVariantMap row;
            row.insert(QStringLiteral("name"), name.trimmed());
            row.insert(QStringLiteral("title"), QString());
            row.insert(QStringLiteral("icon"), QStringLiteral("application-x-executable"));
            apps.append(row);
        }
    }
    return apps;
}
