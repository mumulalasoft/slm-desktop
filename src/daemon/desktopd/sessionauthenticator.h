#pragma once

#include <QString>

class SessionAuthenticator
{
public:
    static bool authenticateCurrentUser(const QString &password, QString *reason = nullptr);
};
