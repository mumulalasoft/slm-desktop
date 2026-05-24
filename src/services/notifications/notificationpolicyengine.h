#pragma once

#include "notificationtypes.h"

class NotificationPolicyEngine
{
public:
    struct Snapshot {
        bool notificationsEnabled = true;
        bool allowCriticalAlerts = true;
        bool silenceFullscreen = true;
        bool silenceScreenShare = true;
        bool focusModeIntegration = true;
        bool deliverQuietly = false;
        bool doNotDisturb = false;
    };

    static QString normalizePriority(const QString &priority);
    static int urgencyForPriority(const QString &priority);
    static bool shouldDropNotification(const NotificationEntry &entry, const Snapshot &snapshot);
    static bool shouldShowBanner(const NotificationEntry &entry,
                                 const Snapshot &snapshot,
                                 const NotificationRuntimeContext &runtimeContext);
};
