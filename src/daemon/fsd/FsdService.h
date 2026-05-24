#pragma once

#include "FsdPathPolicy.h"
#include "FsdPolkit.h"
#include "FsdRecoveryTrash.h"
#include "FsdSnapshot.h"
#include "FsdTokenStore.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// FsdService is the root D-Bus object for org.slm.Desktop.Fsd1.
// It dispatches incoming method calls through the security stack:
//   1. Path validation    (FsdPathPolicy)
//   2. Token check        (FsdTokenStore) — for calls with an existing token
//   3. Polkit check       (FsdPolkit)     — per verb
//   4. Pre-op snapshot    (FsdSnapshot)   — for destructive ops on protected paths
//   5. Filesystem op      — Phase 1 implementation; stubs return NotImplemented now
//   6. Signal emission    — OperationCompleted, SnapshotCreated, etc.
//
// Caller identity is extracted from the D-Bus message via currentMessage()
// on every call (QDBusContext).  The UID is used for token ownership and
// recovery-trash namespacing.  The unique name is used for connection-death
// revocation.
//
// All slots return a{sv} result dicts with at minimum:
//   ok (bool), error (string), error_code (string).
class FsdService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Fsd1")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit FsdService(QObject *parent = nullptr);

    bool start(QString *error = nullptr);
    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    // ── Query ────────────────────────────────────────────────────────
    QVariantMap Ping() const;
    void IsProtected(const QString &path, bool &protectedOut, QString &scopeOut);
    QVariantList GetTokens();

    // ── Token lifecycle ──────────────────────────────────────────────
    QVariantMap ReleaseToken(const QString &tokenId);

    // ── Mutating verbs (all return a{sv} result dict) ────────────────
    QVariantMap RequestRename(const QString &tokenId,
                              const QString &source,
                              const QString &destination);

    QVariantMap RequestDelete(const QString &tokenId, const QString &path);
    QVariantMap RequestPurge(const QString &tokenId, const QString &path);

    QVariantMap RequestMove(const QString &tokenId,
                            const QString &source,
                            const QString &destination);

    QVariantMap RequestCopy(const QString &tokenId,
                            const QString &source,
                            const QString &destination);

    QVariantMap RequestWrite(const QString &tokenId,
                             const QString &path,
                             const QByteArray &content);

    QVariantMap RequestCreate(const QString &tokenId,
                              const QString &path,
                              bool isDir,
                              uint mode);

    QVariantMap RequestChmod(const QString &tokenId,
                             const QString &path,
                             uint mode);

    QVariantMap RequestChown(const QString &tokenId,
                             const QString &path,
                             uint uid,
                             uint gid);

    QVariantMap RequestMount(const QString &tokenId,
                             const QString &device,
                             const QString &mountpoint,
                             const QString &fstype,
                             const QString &options);

    QVariantMap RequestUnmount(const QString &tokenId, const QString &mountpoint);

signals:
    void serviceRegisteredChanged();
    void TokenIssued(const QVariantMap &token);
    void TokenExpired(const QString &tokenId);
    void TokenRevoked(const QString &tokenId, const QString &reason);
    void OperationCompleted(const QString &tokenId,
                            const QString &verb,
                            const QVariantMap &result);
    void SnapshotCreated(const QString &snapshotName,
                         const QString &sourcePath,
                         const QString &tokenId);

private:
    // Build a standard "not implemented" result map.
    static QVariantMap notImplemented();

    // Build a success or error result map.
    static QVariantMap makeResult(bool ok,
                                  const QString &errorCode = {},
                                  const QString &error = {},
                                  const QString &snapshot = {},
                                  const QString &tokenId = {});

    // Retrieve caller's UID from the current D-Bus message.
    // Returns (UINT_MAX) on failure.
    uint callerUid() const;

    // Retrieve caller's unique D-Bus name from the current D-Bus message.
    QString callerUniqueName() const;

    // Shared preamble for all mutating verbs:
    //   1. Validate + canonicalize path
    //   2. Check token scope (if tokenId is non-empty)
    //   3. Run polkit check
    //   4. Take a snapshot if the path is protected and op is destructive
    // Returns {} (empty) on success; a filled-in error result on failure.
    QVariantMap guardedPreamble(const QString &tokenId,
                                const QString &rawPath,
                                const QString &verb,
                                QString *canonPathOut,
                                QString *snapshotNameOut);

    // Connect token store signals to our D-Bus signals.
    void connectTokenSignals();

    bool          m_serviceRegistered = false;
    FsdTokenStore m_tokenStore;
    FsdPathPolicy m_pathPolicy;
    FsdPolkit     m_polkit;
    FsdSnapshot   m_snapshot;
    FsdRecoveryTrash m_recoveryTrash;
};
