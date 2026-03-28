#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class UserAccountsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString username READ username NOTIFY changed)
    Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
    Q_PROPERTY(QString avatarPath READ avatarPath NOTIFY changed)
    Q_PROPERTY(QString accountType READ accountType NOTIFY changed)
    Q_PROPERTY(bool automaticLoginEnabled READ automaticLoginEnabled NOTIFY changed)
    Q_PROPERTY(bool guestSessionSupported READ guestSessionSupported NOTIFY changed)
    Q_PROPERTY(bool guestSessionEnabled READ guestSessionEnabled NOTIFY changed)
    Q_PROPERTY(QString guestSessionBackend READ guestSessionBackend NOTIFY changed)
    Q_PROPERTY(QVariantList users READ users NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

public:
    explicit UserAccountsController(QObject *parent = nullptr);

    QString username() const { return m_username; }
    QString displayName() const { return m_displayName; }
    QString avatarPath() const { return m_avatarPath; }
    QString accountType() const { return m_accountType; }
    bool automaticLoginEnabled() const { return m_automaticLoginEnabled; }
    bool guestSessionSupported() const { return m_guestSessionSupported; }
    bool guestSessionEnabled() const { return m_guestSessionEnabled; }
    QString guestSessionBackend() const { return m_guestSessionBackend; }
    QVariantList users() const { return m_users; }
    bool available() const { return m_available; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setAutomaticLoginEnabled(bool enabled);
    Q_INVOKABLE bool setGuestSessionEnabled(bool enabled);

signals:
    void changed();

private:
    struct GuestBackendState {
        bool supported = false;
        bool enabled = false;
        QString backend;
        QString readPath;
        QString writePath;
    };

    void loadFallbackUser();
    bool loadFromAccountsService();
    GuestBackendState probeGuestBackend() const;
    void setLastError(const QString &error);
    QVariantMap loadUserObject(const QString &path) const;

    QString m_username;
    QString m_displayName;
    QString m_avatarPath;
    QString m_accountType = QStringLiteral("Standard");
    bool m_automaticLoginEnabled = false;
    bool m_guestSessionSupported = false;
    bool m_guestSessionEnabled = false;
    QString m_guestSessionBackend = QStringLiteral("Unavailable");
    QString m_guestSessionWritePath;
    QVariantList m_users;
    bool m_available = false;
    QString m_lastError;
};
