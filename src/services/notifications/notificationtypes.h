#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

struct NotificationEntry {
    uint id = 0;
    QString appId;
    QString appName;
    QString appIcon;
    QString summary;
    QString body;
    QStringList actions;
    QString priority = QStringLiteral("normal");
    QString groupId;
    bool read = false;
    bool banner = true;
    QString lifecycleState = QStringLiteral("Archived");
    int urgency = 1;
    int expireTimeoutMs = -1;
    QDateTime timestamp;
};

struct NotificationRuntimeContext {
    bool fullscreen = false;
    bool screenShare = false;
    bool focusMode = false;
};
