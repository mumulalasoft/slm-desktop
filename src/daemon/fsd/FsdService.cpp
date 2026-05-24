#include "FsdService.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDebug>

namespace {

constexpr const char kServiceName[] = "org.slm.Desktop.Fsd1";
constexpr const char kObjectPath[]  = "/org/slm/Desktop/Fsd";
constexpr const char kApiVersion[]  = "0.1.0";

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

FsdService::FsdService(QObject *parent)
    : QObject(parent)
    , m_tokenStore(this)
    , m_pathPolicy(this)
    , m_polkit(this)
    , m_snapshot(this)
    , m_recoveryTrash(this)
{
    connectTokenSignals();
    m_pathPolicy.loadConfig();
}

bool FsdService::start(QString *error)
{
    QDBusConnection bus = QDBusConnection::systemBus();

    if (!bus.isConnected()) {
        if (error)
            *error = QStringLiteral("Cannot connect to system D-Bus");
        return false;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (iface && iface->isServiceRegistered(QString::fromLatin1(kServiceName)).value()) {
        if (error)
            *error = QStringLiteral("Service already registered");
        return false;
    }

    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        if (error)
            *error = QStringLiteral("registerService failed: ") + bus.lastError().message();
        return false;
    }

    const bool ok = bus.registerObject(
        QString::fromLatin1(kObjectPath),
        this,
        QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllSignals
            | QDBusConnection::ExportAllProperties);

    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kServiceName));
        if (error)
            *error = QStringLiteral("registerObject failed: ") + bus.lastError().message();
        return false;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
    qInfo().noquote() << "[slm-fsd] registered service" << kServiceName
                      << "at" << kObjectPath;
    return true;
}

bool FsdService::serviceRegistered() const { return m_serviceRegistered; }
QString FsdService::apiVersion() const { return QString::fromLatin1(kApiVersion); }

// ── Caller identity ──────────────────────────────────────────────────────────

uint FsdService::callerUid() const
{
    if (!isDBusContext())
        return UINT_MAX;

    const QString callerName = connection().interface()
                                   ? currentMessage().service()
                                   : QString{};
    if (callerName.isEmpty())
        return UINT_MAX;

    bool ok = false;
    const uint uid = connection().interface()
                         ->serviceUid(callerName)
                         .value();
    Q_UNUSED(ok)
    return uid;
}

QString FsdService::callerUniqueName() const
{
    if (!isDBusContext())
        return {};
    return currentMessage().service();
}

// ── Query ────────────────────────────────────────────────────────────────────

QVariantMap FsdService::Ping() const
{
    QVariantMap r;
    r[QStringLiteral("ok")]         = true;
    r[QStringLiteral("service")]    = QString::fromLatin1(kServiceName);
    r[QStringLiteral("apiVersion")] = apiVersion();
    return r;
}

void FsdService::IsProtected(const QString &path, bool &protectedOut, QString &scopeOut)
{
    QString scope;
    QString errorCode;
    const QString canonical = m_pathPolicy.validateAndCanonicalize(path, &errorCode);

    if (canonical.isEmpty()) {
        // Path is invalid — treat as "not protected" rather than erroring out,
        // because the caller may be checking before an operation (harmless query).
        protectedOut = false;
        scopeOut     = {};
        return;
    }

    protectedOut = m_pathPolicy.isProtected(canonical, &scope);
    scopeOut     = scope;
}

QVariantList FsdService::GetTokens()
{
    const uint uid = callerUid();
    QVariantList result;
    if (uid == UINT_MAX)
        return result;

    const auto tokens = m_tokenStore.tokensForUid(uid);
    for (const auto &tok : tokens)
        result << tok.toVariantMap();
    return result;
}

// ── Token lifecycle ──────────────────────────────────────────────────────────

QVariantMap FsdService::ReleaseToken(const QString &tokenId)
{
    const uint callerUid_ = callerUid();
    // Verify token belongs to caller before releasing
    const FsdToken *tok = m_tokenStore.findToken(tokenId);
    if (!tok)
        return makeResult(false, QStringLiteral("TokenNotFound"),
                          QStringLiteral("Token not found or already expired"));
    if (tok->ownerUid != callerUid_)
        return makeResult(false, QStringLiteral("PermissionDenied"),
                          QStringLiteral("Token does not belong to caller"));

    m_tokenStore.releaseToken(tokenId);
    return makeResult(true);
}

// ── guardedPreamble ───────────────────────────────────────────────────────────

QVariantMap FsdService::guardedPreamble(const QString &tokenId,
                                         const QString &rawPath,
                                         const QString &verb,
                                         QString *canonPathOut,
                                         QString *snapshotNameOut)
{
    // 1. Validate + canonicalize
    QString errorCode;
    const QString canon = m_pathPolicy.validateAndCanonicalize(rawPath, &errorCode);
    if (canon.isEmpty()) {
        return makeResult(false, errorCode,
                          QStringLiteral("Path validation failed: ") + errorCode);
    }
    *canonPathOut = canon;

    const uint uid = callerUid();
    const QString uniqueName = callerUniqueName();

    // 2. Token scope check (if token provided)
    if (!tokenId.isEmpty()) {
        const QString tokenErr = m_tokenStore.validateToken(tokenId, canon, verb, uid);
        if (!tokenErr.isEmpty()) {
            return makeResult(false, tokenErr,
                              QStringLiteral("Token validation failed: ") + tokenErr);
        }
    }

    // 3. Polkit authorization
    //    Phase 0: checkAuthorization always returns "NotImplemented"
    //    Phase 1: returns "" (authorized) or "PermissionDenied"
    const QString polkitErr = m_polkit.checkAuthorization(uniqueName, verb);
    if (!polkitErr.isEmpty() && polkitErr != QStringLiteral("NotImplemented")) {
        return makeResult(false, polkitErr,
                          QStringLiteral("Authorization denied: ") + polkitErr);
    }

    // 4. Snapshot (stub — returns "" in Phase 0)
    QString snapshotName;
    if (m_pathPolicy.isProtected(canon, nullptr)) {
        snapshotName = m_snapshot.createSnapshot(canon, verb);
        if (!snapshotName.isEmpty())
            emit SnapshotCreated(snapshotName, canon, tokenId);
    }
    *snapshotNameOut = snapshotName;

    // 5. Issue token if not provided (on first call)
    //    Phase 0: issue a token so the UX can call GetTokens()
    if (tokenId.isEmpty()) {
        QString scope;
        m_pathPolicy.isProtected(canon, &scope);
        const FsdToken newTok = m_tokenStore.issueToken(
            canon, {verb}, uid, uniqueName, snapshotName);
        emit TokenIssued(newTok.toVariantMap());
        // We don't return here — we still continue with the verb stub below.
        // The token_id is included in the final result.
    }

    return {}; // empty = no error
}

// ── Mutating verb stubs ───────────────────────────────────────────────────────
// All verbs follow the same pattern:
//   1. guardedPreamble() — validate, polkit, snapshot
//   2. Return NotImplemented (Phase 0)
// Phase 1 replaces step 2 with actual filesystem operations.

QVariantMap FsdService::RequestRename(const QString &tokenId,
                                       const QString &source,
                                       const QString &destination)
{
    QString canonSource, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, source, QStringLiteral("rename"),
                                      &canonSource, &snapshotName);
    if (!err.isEmpty()) return err;

    // Phase 0 stub
    Q_UNUSED(destination)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("rename"), result);
    return result;
}

QVariantMap FsdService::RequestDelete(const QString &tokenId, const QString &path)
{
    QString canonPath, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, path, QStringLiteral("delete"),
                                      &canonPath, &snapshotName);
    if (!err.isEmpty()) return err;

    // Phase 0 stub — Phase 1: m_recoveryTrash.moveToTrash(...)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("delete"), result);
    return result;
}

QVariantMap FsdService::RequestPurge(const QString &tokenId, const QString &path)
{
    QString canonPath, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, path, QStringLiteral("purge"),
                                      &canonPath, &snapshotName);
    if (!err.isEmpty()) return err;

    // Phase 0 stub — Phase 1: m_recoveryTrash.purgeEntry(...)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("purge"), result);
    return result;
}

QVariantMap FsdService::RequestMove(const QString &tokenId,
                                     const QString &source,
                                     const QString &destination)
{
    QString canonSource, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, source, QStringLiteral("move"),
                                      &canonSource, &snapshotName);
    if (!err.isEmpty()) return err;

    Q_UNUSED(destination)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("move"), result);
    return result;
}

QVariantMap FsdService::RequestCopy(const QString &tokenId,
                                     const QString &source,
                                     const QString &destination)
{
    QString canonDest, snapshotName;
    // Validate destination (the protected path for copy is the destination)
    QVariantMap err = guardedPreamble(tokenId, destination, QStringLiteral("copy"),
                                      &canonDest, &snapshotName);
    if (!err.isEmpty()) return err;

    Q_UNUSED(source)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("copy"), result);
    return result;
}

QVariantMap FsdService::RequestWrite(const QString &tokenId,
                                      const QString &path,
                                      const QByteArray &content)
{
    QString canonPath, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, path, QStringLiteral("write"),
                                      &canonPath, &snapshotName);
    if (!err.isEmpty()) return err;

    Q_UNUSED(content)
    // Phase 1: atomic write via temp-file + rename
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("write"), result);
    return result;
}

QVariantMap FsdService::RequestCreate(const QString &tokenId,
                                       const QString &path,
                                       bool isDir,
                                       uint mode)
{
    QString canonPath, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, path, QStringLiteral("create"),
                                      &canonPath, &snapshotName);
    if (!err.isEmpty()) return err;

    Q_UNUSED(isDir)
    Q_UNUSED(mode)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("create"), result);
    return result;
}

QVariantMap FsdService::RequestChmod(const QString &tokenId,
                                      const QString &path,
                                      uint mode)
{
    QString canonPath, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, path, QStringLiteral("chmod"),
                                      &canonPath, &snapshotName);
    if (!err.isEmpty()) return err;

    Q_UNUSED(mode)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("chmod"), result);
    return result;
}

QVariantMap FsdService::RequestChown(const QString &tokenId,
                                      const QString &path,
                                      uint uid,
                                      uint gid)
{
    QString canonPath, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, path, QStringLiteral("chown"),
                                      &canonPath, &snapshotName);
    if (!err.isEmpty()) return err;

    Q_UNUSED(uid)
    Q_UNUSED(gid)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("chown"), result);
    return result;
}

QVariantMap FsdService::RequestMount(const QString &tokenId,
                                      const QString &device,
                                      const QString &mountpoint,
                                      const QString &fstype,
                                      const QString &options)
{
    // For mount, validate the mountpoint as the protected path
    QString canonMount, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, mountpoint, QStringLiteral("mount"),
                                      &canonMount, &snapshotName);
    if (!err.isEmpty()) return err;

    Q_UNUSED(device)
    Q_UNUSED(fstype)
    Q_UNUSED(options)
    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("mount"), result);
    return result;
}

QVariantMap FsdService::RequestUnmount(const QString &tokenId,
                                        const QString &mountpoint)
{
    QString canonMount, snapshotName;
    QVariantMap err = guardedPreamble(tokenId, mountpoint, QStringLiteral("unmount"),
                                      &canonMount, &snapshotName);
    if (!err.isEmpty()) return err;

    QVariantMap result = notImplemented();
    emit OperationCompleted(tokenId, QStringLiteral("unmount"), result);
    return result;
}

// ── Private helpers ───────────────────────────────────────────────────────────

QVariantMap FsdService::notImplemented()
{
    return makeResult(false,
                      QStringLiteral("NotImplemented"),
                      QStringLiteral("This verb is not yet implemented (Phase 0 scaffold)"));
}

QVariantMap FsdService::makeResult(bool ok,
                                    const QString &errorCode,
                                    const QString &error,
                                    const QString &snapshot,
                                    const QString &tokenId)
{
    QVariantMap r;
    r[QStringLiteral("ok")]         = ok;
    r[QStringLiteral("error")]      = error;
    r[QStringLiteral("error_code")] = errorCode;
    r[QStringLiteral("snapshot")]   = snapshot;
    r[QStringLiteral("token_id")]   = tokenId;
    return r;
}

void FsdService::connectTokenSignals()
{
    connect(&m_tokenStore, &FsdTokenStore::tokenExpired,
            this, [this](const QString &tokenId) {
                emit TokenExpired(tokenId);
            });

    connect(&m_tokenStore, &FsdTokenStore::tokenRevoked,
            this, [this](const QString &tokenId, const QString &reason) {
                emit TokenRevoked(tokenId, reason);
            });

    connect(&m_tokenStore, &FsdTokenStore::tokenIssued,
            this, [this](const FsdToken &token) {
                emit TokenIssued(token.toVariantMap());
            });
}
