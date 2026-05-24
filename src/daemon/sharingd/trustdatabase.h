#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

class TrustDatabase : public QObject
{
    Q_OBJECT

public:
    enum class TrustLevel {
        Blocked,
        Untrusted,
        Trusted,
        Full,
    };
    Q_ENUM(TrustLevel)

    explicit TrustDatabase(QObject *parent = nullptr);
    ~TrustDatabase() override;

    bool open();

    bool upsertDevice(const QString &deviceId, const QVariantMap &info);
    bool removeDevice(const QString &deviceId);
    QVariantMap deviceInfo(const QString &deviceId) const;
    QVariantList allDevices() const;

    bool setTrustLevel(const QString &deviceId, TrustLevel level);
    TrustLevel trustLevel(const QString &deviceId) const;
    static QString trustLevelToString(TrustLevel level);
    static TrustLevel trustLevelFromString(const QString &value);

    bool setPermission(const QString &deviceId, const QString &permission, bool allowed);
    QVariantMap permissions(const QString &deviceId) const;
    bool hasPermission(const QString &deviceId, const QString &permission) const;

    bool updateLastSeen(const QString &deviceId);

private:
    bool createSchema();
    static QString dbPath();

    QSqlDatabase m_db;
};
