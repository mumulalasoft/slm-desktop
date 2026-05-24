#pragma once

#include <QString>
#include "notificationtypes.h"

class NotificationGroupingEngine
{
public:
    static QString groupIdFor(const NotificationEntry &entry);
};
