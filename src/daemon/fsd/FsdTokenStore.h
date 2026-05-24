#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>

// FsdToken represents a single granted access token.
// Tokens are in-memory only — they are not persisted to disk.
// On daemon restart all tokens are invalidated by design.
struct FsdToken {
    QString      id;             // UUID-v4
    QString      pathPrefix;     // canonicalized absolute path prefix
    QStringList  opSet;          // permitted operation names e.g. {"rename","delete"}
    QDateTime    expiresAt;      // UTC; token invalid at or after this instant
    uint         ownerUid;       // UID from GetConnectionUnixUser at issuance
    QString      callerUniqueName; // D-Bus unique name ":1.NNN" — revoke on disconnect
    QString      snapshotName;   // BTRFS snapshot taken at issuance ("" if none)

    bool isExpired() const;
    bool allowsOp(const QString &op) const;
    // Returns true if canonPath is under pathPrefix and op is in opSet
    bool coversScope(const QString &canonPath, const QString &op) const;

    QVariantMap toVariantMap() const;
};

// FsdTokenStore manages the lifecycle of all active tokens.
// Thread-safety: designed for single-threaded use (Qt event loop).
class FsdTokenStore : public QObject
{
    Q_OBJECT

public:
    static constexpr int kDefaultTtlSeconds = 600; // 10 minutes

    explicit FsdTokenStore(QObject *parent = nullptr);
    ~FsdTokenStore() override;

    // Issue a new token for (pathPrefix, opSet) bound to callerUniqueName/ownerUid.
    // Takes an optional BTRFS snapshot name to associate.
    FsdToken issueToken(const QString &pathPrefix,
                        const QStringList &opSet,
                        uint ownerUid,
                        const QString &callerUniqueName,
                        const QString &snapshotName = {});

    // Look up a token by ID. Returns nullptr if not found or expired.
    const FsdToken *findToken(const QString &tokenId) const;

    // Explicitly release a token (caller surrenders it early).
    bool releaseToken(const QString &tokenId);

    // Revoke all tokens associated with a D-Bus unique name (called on disconnect).
    void revokeByConnection(const QString &callerUniqueName, const QString &reason = QStringLiteral("ConnectionLost"));

    // Return all tokens owned by a given UID (for GetTokens).
    QList<FsdToken> tokensForUid(uint uid) const;

    // Validate a token for a specific (path, op) pair. Returns an error string,
    // or an empty string on success.
    QString validateToken(const QString &tokenId,
                          const QString &canonPath,
                          const QString &op,
                          uint callerUid) const;

signals:
    void tokenIssued(const FsdToken &token);
    void tokenExpired(const QString &tokenId);
    void tokenRevoked(const QString &tokenId, const QString &reason);

private slots:
    void onTokenTimerFired();

private:
    void scheduleExpiry(const FsdToken &token);

    QHash<QString, FsdToken>  m_tokens;   // tokenId → FsdToken
    QHash<QString, QTimer *>  m_timers;   // tokenId → expiry QTimer
};
