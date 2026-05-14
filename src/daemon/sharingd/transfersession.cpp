#include "transfersession.h"

#include <QFileInfo>

TransferSession::TransferSession(Direction direction,
                                  const QString &deviceId,
                                  const QString &filePath,
                                  QObject *parent)
    : QObject(parent)
    , m_transferId(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_direction(direction)
    , m_deviceId(deviceId)
    , m_filePath(filePath)
    , m_startedAt(QDateTime::currentDateTimeUtc())
{
    m_totalBytes = QFileInfo(filePath).size();
}

void TransferSession::setTotalBytes(qint64 total)
{
    m_totalBytes = total;
}

void TransferSession::updateProgress(qint64 transferred)
{
    m_bytesTransferred = transferred;
    m_state = State::Active;
    emit progressChanged(m_bytesTransferred, m_totalBytes);
}

void TransferSession::complete()
{
    m_bytesTransferred = m_totalBytes;
    m_state = State::Completed;
    emit stateChanged(m_state);
    emit completed(true, QString());
}

void TransferSession::fail(const QString &error)
{
    m_error = error;
    m_state = State::Failed;
    emit stateChanged(m_state);
    emit completed(false, error);
}

void TransferSession::cancel()
{
    m_state = State::Cancelled;
    emit stateChanged(m_state);
    emit completed(false, QStringLiteral("cancelled"));
}

void TransferSession::resume()
{
    if (m_state == State::Paused)
        m_state = State::Active;
}

QVariantMap TransferSession::toVariantMap() const
{
    static const QMetaEnum dirEnum = QMetaEnum::fromType<Direction>();
    static const QMetaEnum stateEnum = QMetaEnum::fromType<State>();

    return {
        {QStringLiteral("transferId"), m_transferId},
        {QStringLiteral("direction"), QString::fromLatin1(dirEnum.valueToKey(static_cast<int>(m_direction))).toLower()},
        {QStringLiteral("state"), QString::fromLatin1(stateEnum.valueToKey(static_cast<int>(m_state))).toLower()},
        {QStringLiteral("deviceId"), m_deviceId},
        {QStringLiteral("filePath"), m_filePath},
        {QStringLiteral("fileName"), QFileInfo(m_filePath).fileName()},
        {QStringLiteral("bytesTransferred"), m_bytesTransferred},
        {QStringLiteral("totalBytes"), m_totalBytes},
        {QStringLiteral("error"), m_error},
        {QStringLiteral("startedAt"), m_startedAt.toString(Qt::ISODate)},
    };
}
