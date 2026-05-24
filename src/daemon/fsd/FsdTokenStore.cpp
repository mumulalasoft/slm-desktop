#include "FsdTokenStore.h"

#include <QDebug>

// ── FsdToken ────────────────────────────────────────────────────────────────

bool FsdToken::isExpired() const
{
    return QDateTime::currentDateTimeUtc() >= expiresAt;
}

bool FsdToken::allowsOp(const QString &op) const
{
    return opSet.contains(op);
}

bool FsdToken::coversScope(const QString &canonPath, const QString &op) const
{
    if (isExpired())
        return false;
    if (!allowsOp(op))
        return false;
    // canonPath must equal pathPrefix or be strictly under it
    if (canonPath != pathPrefix && !canonPath.startsWith(pathPrefix + QLatin1Char('/')))
        return false;
    return true;
}

QVariantMap FsdToken::toVariantMap() const
{
    QVariantMap m;
    m[QStringLiteral("token_id")]   = id;
    m[QStringLiteral("path")]       = pathPrefix;
    m[QStringLiteral("ops")]        = opSet;
    m[QStringLiteral("expires_at")] = static_cast<qulonglong>(expiresAt.toSecsSinceEpoch());
    m[QStringLiteral("uid")]        = ownerUid;
    m[QStringLiteral("snapshot")]   = snapshotName;
    return m;
}

// ── FsdTokenStore ────────────────────────────────────────────────────────────

FsdTokenStore::FsdTokenStore(QObject *parent)
    : QObject(parent)
{
}

FsdTokenStore::~FsdTokenStore()
{
    // QTimer children will be deleted automatically via QObject parent chain.
}

FsdToken FsdTokenStore::issueToken(const QString &pathPrefix,
                                   const QStringList &opSet,
                                   uint ownerUid,
                                   const QString &callerUniqueName,
                                   const QString &snapshotName)
{
    FsdToken tok;
    tok.id               = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tok.pathPrefix       = pathPrefix;
    tok.opSet            = opSet;
    tok.expiresAt        = QDateTime::currentDateTimeUtc().addSecs(kDefaultTtlSeconds);
    tok.ownerUid         = ownerUid;
    tok.callerUniqueName = callerUniqueName;
    tok.snapshotName     = snapshotName;

    m_tokens.insert(tok.id, tok);
    scheduleExpiry(tok);

    qInfo().noquote() << "[slm-fsd] token issued id=" << tok.id
                      << "uid=" << ownerUid
                      << "prefix=" << pathPrefix
                      << "ops=" << opSet.join(QLatin1Char(','))
                      << "expires=" << tok.expiresAt.toString(Qt::ISODate);

    emit tokenIssued(tok);
    return tok;
}

const FsdToken *FsdTokenStore::findToken(const QString &tokenId) const
{
    auto it = m_tokens.constFind(tokenId);
    if (it == m_tokens.constEnd())
        return nullptr;
    if (it->isExpired())
        return nullptr;
    return &it.value();
}

bool FsdTokenStore::releaseToken(const QString &tokenId)
{
    if (!m_tokens.contains(tokenId))
        return false;

    // Cancel the expiry timer
    if (auto *timer = m_timers.take(tokenId)) {
        timer->stop();
        timer->deleteLater();
    }
    m_tokens.remove(tokenId);

    emit tokenRevoked(tokenId, QStringLiteral("Released"));
    return true;
}

void FsdTokenStore::revokeByConnection(const QString &callerUniqueName, const QString &reason)
{
    QStringList toRevoke;
    for (auto it = m_tokens.constBegin(); it != m_tokens.constEnd(); ++it) {
        if (it->callerUniqueName == callerUniqueName)
            toRevoke << it->id;
    }
    for (const QString &id : std::as_const(toRevoke)) {
        if (auto *timer = m_timers.take(id)) {
            timer->stop();
            timer->deleteLater();
        }
        m_tokens.remove(id);
        qInfo().noquote() << "[slm-fsd] token revoked id=" << id << "reason=" << reason;
        emit tokenRevoked(id, reason);
    }
}

QList<FsdToken> FsdTokenStore::tokensForUid(uint uid) const
{
    QList<FsdToken> result;
    for (auto it = m_tokens.constBegin(); it != m_tokens.constEnd(); ++it) {
        if (it->ownerUid == uid && !it->isExpired())
            result << it.value();
    }
    return result;
}

QString FsdTokenStore::validateToken(const QString &tokenId,
                                     const QString &canonPath,
                                     const QString &op,
                                     uint callerUid) const
{
    if (tokenId.isEmpty())
        return QStringLiteral("TokenMissing");

    auto it = m_tokens.constFind(tokenId);
    if (it == m_tokens.constEnd())
        return QStringLiteral("TokenNotFound");

    const FsdToken &tok = it.value();

    if (tok.isExpired())
        return QStringLiteral("TokenExpired");

    if (tok.ownerUid != callerUid)
        return QStringLiteral("TokenOwnerMismatch");

    if (!tok.coversScope(canonPath, op))
        return QStringLiteral("TokenScopeViolation");

    return {}; // success
}

void FsdTokenStore::scheduleExpiry(const FsdToken &token)
{
    const qint64 msLeft = QDateTime::currentDateTimeUtc().msecsTo(token.expiresAt);
    if (msLeft <= 0) {
        // Already expired — remove immediately on next event loop tick
        QTimer::singleShot(0, this, &FsdTokenStore::onTokenTimerFired);
        return;
    }

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setProperty("tokenId", token.id);
    connect(timer, &QTimer::timeout, this, &FsdTokenStore::onTokenTimerFired);
    timer->start(static_cast<int>(qMin(msLeft, static_cast<qint64>(INT_MAX))));
    m_timers.insert(token.id, timer);
}

void FsdTokenStore::onTokenTimerFired()
{
    auto *timer = qobject_cast<QTimer *>(sender());
    if (!timer)
        return;

    const QString tokenId = timer->property("tokenId").toString();
    m_timers.remove(tokenId);
    timer->deleteLater();

    if (m_tokens.contains(tokenId)) {
        m_tokens.remove(tokenId);
        qInfo().noquote() << "[slm-fsd] token expired id=" << tokenId;
        emit tokenExpired(tokenId);
    }
}
