#include "useraccountscontroller.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFileInfo>
#include <QVariantMap>

#include <pwd.h>
#include <unistd.h>

namespace {
constexpr auto kAccountsService = "org.freedesktop.Accounts";
constexpr auto kAccountsPath = "/org/freedesktop/Accounts";
constexpr auto kAccountsIface = "org.freedesktop.Accounts";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
constexpr auto kUserIface = "org.freedesktop.Accounts.User";
}

UserAccountsController::UserAccountsController(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void UserAccountsController::refresh()
{
    const GuestBackendState guest = probeGuestBackend();
    m_guestSessionSupported = guest.supported;
    m_guestSessionEnabled = guest.enabled;
    m_guestSessionBackend = guest.backend;
    m_guestSessionWritePath = guest.writePath;

    loadFallbackUser();
    if (!loadFromAccountsService()) {
        emit changed();
        return;
    }
    emit changed();
}

bool UserAccountsController::setAutomaticLoginEnabled(bool enabled)
{
    if (m_username.isEmpty()) {
        setLastError(QStringLiteral("Current user is unknown"));
        emit changed();
        return false;
    }

    QDBusInterface accounts(QString::fromLatin1(kAccountsService),
                            QString::fromLatin1(kAccountsPath),
                            QString::fromLatin1(kAccountsIface),
                            QDBusConnection::systemBus());
    if (!accounts.isValid()) {
        setLastError(QStringLiteral("AccountsService unavailable"));
        emit changed();
        return false;
    }

    QDBusReply<void> reply = accounts.call(QStringLiteral("SetAutomaticLogin"),
                                           m_username,
                                           enabled);
    if (!reply.isValid()) {
        setLastError(reply.error().message());
        emit changed();
        return false;
    }

    m_automaticLoginEnabled = enabled;
    setLastError({});
    emit changed();
    return true;
}

bool UserAccountsController::setGuestSessionEnabled(bool enabled)
{
    Q_UNUSED(enabled);
    setLastError(QStringLiteral("Guest session is not available on the current login backend"));
    emit changed();
    return false;
}

void UserAccountsController::loadFallbackUser()
{
    const uid_t uid = ::geteuid();
    const passwd *pw = ::getpwuid(uid);
    if (!pw) {
        return;
    }

    const QString username = QString::fromLocal8Bit(pw->pw_name ? pw->pw_name : "");
    const QString gecos = QString::fromLocal8Bit(pw->pw_gecos ? pw->pw_gecos : "");
    const QString display = gecos.section(',', 0, 0).trimmed();

    if (!username.isEmpty()) {
        m_username = username;
    }
    if (!display.isEmpty()) {
        m_displayName = display;
    } else if (!m_username.isEmpty()) {
        m_displayName = m_username;
    }
}

bool UserAccountsController::loadFromAccountsService()
{
    QDBusInterface accounts(QString::fromLatin1(kAccountsService),
                            QString::fromLatin1(kAccountsPath),
                            QString::fromLatin1(kAccountsIface),
                            QDBusConnection::systemBus());
    if (!accounts.isValid()) {
        m_available = false;
        setLastError(QStringLiteral("AccountsService unavailable"));
        return false;
    }

    const uint uid = static_cast<uint>(::geteuid());
    QDBusReply<QDBusObjectPath> userPathReply = accounts.call(QStringLiteral("FindUserById"), uid);
    if (!userPathReply.isValid()) {
        m_available = false;
        setLastError(userPathReply.error().message());
        return false;
    }

    const QString userPath = userPathReply.value().path();
    if (userPath.isEmpty()) {
        m_available = false;
        setLastError(QStringLiteral("AccountsService returned empty user path"));
        return false;
    }

    QDBusInterface props(QString::fromLatin1(kAccountsService),
                         userPath,
                         QString::fromLatin1(kPropsIface),
                         QDBusConnection::systemBus());
    if (!props.isValid()) {
        m_available = false;
        setLastError(QStringLiteral("Unable to query user properties"));
        return false;
    }

    QDBusReply<QVariantMap> allReply =
        props.call(QStringLiteral("GetAll"), QString::fromLatin1(kUserIface));
    if (!allReply.isValid()) {
        m_available = false;
        setLastError(allReply.error().message());
        return false;
    }

    const QVariantMap all = allReply.value();
    const QString username = all.value(QStringLiteral("UserName")).toString().trimmed();
    const QString realName = all.value(QStringLiteral("RealName")).toString().trimmed();
    const QString iconFile = all.value(QStringLiteral("IconFile")).toString().trimmed();
    const int type = all.value(QStringLiteral("AccountType")).toInt();
    const bool autoLogin = all.value(QStringLiteral("AutomaticLogin")).toBool();

    if (!username.isEmpty()) {
        m_username = username;
    }
    if (!realName.isEmpty()) {
        m_displayName = realName;
    } else if (!m_username.isEmpty()) {
        m_displayName = m_username;
    }

    if (!iconFile.isEmpty() && QFileInfo::exists(iconFile)) {
        m_avatarPath = iconFile;
    } else {
        m_avatarPath.clear();
    }

    m_accountType = (type == 1) ? QStringLiteral("Administrator")
                                : QStringLiteral("Standard");
    m_automaticLoginEnabled = autoLogin;

    m_users.clear();
    QDBusReply<QList<QDBusObjectPath>> usersReply = accounts.call(QStringLiteral("ListCachedUsers"));
    if (usersReply.isValid()) {
        const QList<QDBusObjectPath> paths = usersReply.value();
        for (const QDBusObjectPath &path : paths) {
            const QVariantMap user = loadUserObject(path.path());
            if (!user.isEmpty()) {
                m_users.push_back(user);
            }
        }
    }

    m_available = true;
    setLastError({});
    return true;
}

void UserAccountsController::setLastError(const QString &error)
{
    m_lastError = error.trimmed();
}

UserAccountsController::GuestBackendState UserAccountsController::probeGuestBackend() const
{
    GuestBackendState out;
    out.supported = false;
    out.enabled = false;
    out.backend = QStringLiteral("Unavailable");
    return out;
}

QVariantMap UserAccountsController::loadUserObject(const QString &path) const
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    QDBusInterface props(QString::fromLatin1(kAccountsService),
                         path,
                         QString::fromLatin1(kPropsIface),
                         QDBusConnection::systemBus());
    if (!props.isValid()) {
        return {};
    }
    QDBusReply<QVariantMap> allReply =
        props.call(QStringLiteral("GetAll"), QString::fromLatin1(kUserIface));
    if (!allReply.isValid()) {
        return {};
    }
    const QVariantMap all = allReply.value();
    const QString username = all.value(QStringLiteral("UserName")).toString().trimmed();
    if (username.isEmpty()) {
        return {};
    }

    const QString realName = all.value(QStringLiteral("RealName")).toString().trimmed();
    const QString iconFile = all.value(QStringLiteral("IconFile")).toString().trimmed();
    const int type = all.value(QStringLiteral("AccountType")).toInt();
    const bool autoLogin = all.value(QStringLiteral("AutomaticLogin")).toBool();
    const bool locked = all.value(QStringLiteral("Locked")).toBool();
    const qint64 uid = all.value(QStringLiteral("Uid")).toLongLong();

    QVariantMap out;
    out.insert(QStringLiteral("username"), username);
    out.insert(QStringLiteral("displayName"), realName.isEmpty() ? username : realName);
    out.insert(QStringLiteral("accountType"),
               (type == 1) ? QStringLiteral("Administrator")
                           : QStringLiteral("Standard"));
    out.insert(QStringLiteral("automaticLogin"), autoLogin);
    out.insert(QStringLiteral("locked"), locked);
    out.insert(QStringLiteral("uid"), uid);
    if (!iconFile.isEmpty() && QFileInfo::exists(iconFile)) {
        out.insert(QStringLiteral("avatarPath"), iconFile);
    } else {
        out.insert(QStringLiteral("avatarPath"), QString());
    }
    return out;
}
