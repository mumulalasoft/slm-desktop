#include "notificationgroupingengine.h"

QString NotificationGroupingEngine::groupIdFor(const NotificationEntry &entry)
{
    const QString explicitGroup = entry.groupId.trimmed();
    if (!explicitGroup.isEmpty()) {
        return explicitGroup;
    }
    const QString appGroup = entry.appId.trimmed();
    if (!appGroup.isEmpty()) {
        return appGroup;
    }
    return QStringLiteral("unknown.app");
}
