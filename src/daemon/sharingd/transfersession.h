#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUuid>
#include <QVariantMap>

class TransferSession : public QObject
{
    Q_OBJECT

public:
    enum class Direction { Outgoing, Incoming };
    enum class State { Pending, Active, Paused, Completed, Failed, Cancelled };
    Q_ENUM(Direction)
    Q_ENUM(State)

    explicit TransferSession(Direction direction,
                             const QString &deviceId,
                             const QString &filePath,
                             QObject *parent = nullptr);

    QString transferId() const { return m_transferId; }
    Direction direction() const { return m_direction; }
    State state() const { return m_state; }
    QString deviceId() const { return m_deviceId; }
    QString filePath() const { return m_filePath; }
    qint64 bytesTransferred() const { return m_bytesTransferred; }
    qint64 totalBytes() const { return m_totalBytes; }
    QString errorString() const { return m_error; }
    QDateTime startedAt() const { return m_startedAt; }

    void setTotalBytes(qint64 total);
    void updateProgress(qint64 transferred);
    void complete();
    void fail(const QString &error);
    void cancel();
    void resume();

    QVariantMap toVariantMap() const;

signals:
    void stateChanged(State state);
    void progressChanged(qint64 bytesTransferred, qint64 totalBytes);
    void completed(bool success, const QString &error);

private:
    QString m_transferId;
    Direction m_direction;
    State m_state = State::Pending;
    QString m_deviceId;
    QString m_filePath;
    qint64 m_bytesTransferred = 0;
    qint64 m_totalBytes = 0;
    QString m_error;
    QDateTime m_startedAt;
};
