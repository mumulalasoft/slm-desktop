#pragma once

#include <QObject>
#include <QString>

class QProcess;

class SettingsPolkitBridge : public QObject
{
    Q_OBJECT

public:
    explicit SettingsPolkitBridge(QObject *parent = nullptr);

    QString requestAuthorization(const QString &actionId,
                                 const QString &moduleId,
                                 const QString &settingId);

signals:
    void authorizationFinished(const QString &requestId,
                               const QString &moduleId,
                               const QString &settingId,
                               bool allowed,
                               const QString &reason);

private:
    qint64 m_counter = 0;
};

