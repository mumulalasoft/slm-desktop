#pragma once

#include <QVector>
#include <QString>

#include "notificationtypes.h"

class NotificationStore
{
public:
    struct LoadedState {
        bool ok = false;
        uint nextId = 1;
        QVector<NotificationEntry> entries;
    };

    static bool saveHistory(const QString &path, uint nextId, const QVector<NotificationEntry> &entries);
    static LoadedState loadHistory(const QString &path, int maxEntries);
};
