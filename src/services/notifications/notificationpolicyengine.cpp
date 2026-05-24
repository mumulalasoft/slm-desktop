#include "notificationpolicyengine.h"

QString NotificationPolicyEngine::normalizePriority(const QString &priority)
{
    const QString lowered = priority.trimmed().toLower();
    if (lowered == QStringLiteral("critical")
        || lowered == QStringLiteral("high")
        || lowered == QStringLiteral("normal")
        || lowered == QStringLiteral("low")
        || lowered == QStringLiteral("silent")) {
        return lowered;
    }
    return QStringLiteral("normal");
}

int NotificationPolicyEngine::urgencyForPriority(const QString &priority)
{
    const QString normalized = normalizePriority(priority);
    if (normalized == QStringLiteral("critical") || normalized == QStringLiteral("high")) {
        return 2;
    }
    if (normalized == QStringLiteral("low") || normalized == QStringLiteral("silent")) {
        return 0;
    }
    return 1;
}

bool NotificationPolicyEngine::shouldDropNotification(const NotificationEntry &entry,
                                                      const Snapshot &snapshot)
{
    if (snapshot.notificationsEnabled) {
        return false;
    }
    if (!snapshot.allowCriticalAlerts) {
        return true;
    }
    return normalizePriority(entry.priority) != QStringLiteral("critical");
}

bool NotificationPolicyEngine::shouldShowBanner(const NotificationEntry &entry,
                                                const Snapshot &snapshot,
                                                const NotificationRuntimeContext &runtimeContext)
{
    const QString priority = normalizePriority(entry.priority);
    const bool isCritical = priority == QStringLiteral("critical");
    const bool isQuietPriority = priority == QStringLiteral("low")
            || priority == QStringLiteral("silent");

    if (isCritical && snapshot.allowCriticalAlerts) {
        return true;
    }
    if (snapshot.doNotDisturb || isQuietPriority || snapshot.deliverQuietly) {
        return false;
    }
    if (snapshot.silenceFullscreen && runtimeContext.fullscreen) {
        return false;
    }
    if (snapshot.silenceScreenShare && runtimeContext.screenShare) {
        return false;
    }
    if (snapshot.focusModeIntegration && runtimeContext.focusMode) {
        return false;
    }
    return true;
}
