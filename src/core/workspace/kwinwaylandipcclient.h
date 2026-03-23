#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class QTimer;

class KWinWaylandIpcClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString socketPath READ socketPath CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QVariantMap lastEvent READ lastEvent NOTIFY lastEventChanged)

public:
    explicit KWinWaylandIpcClient(QObject *parent = nullptr);
    ~KWinWaylandIpcClient() override;

    QString socketPath() const;
    bool connected() const;
    QVariantMap lastEvent() const;

    Q_INVOKABLE void reconnect();
    Q_INVOKABLE bool sendCommand(const QString &command);
    Q_INVOKABLE bool supportsInputCaptureCommands() const;
    Q_INVOKABLE QVariantMap setInputCapturePointerBarriers(const QString &sessionPath,
                                                           const QVariantList &barriers,
                                                           const QVariantMap &options);
    Q_INVOKABLE QVariantMap enableInputCaptureSession(const QString &sessionPath,
                                                      const QVariantMap &options);
    Q_INVOKABLE QVariantMap disableInputCaptureSession(const QString &sessionPath,
                                                       const QVariantMap &options);
    Q_INVOKABLE QVariantMap releaseInputCaptureSession(const QString &sessionPath,
                                                       const QString &reason);

signals:
    void connectedChanged();
    void lastEventChanged();
    void eventReceived(const QString &event, const QVariantMap &payload);

private:
    void refreshConnected();
    void onServiceOwnerChanged(const QString &name,
                               const QString &oldOwner,
                               const QString &newOwner);
    bool invokeKWinShortcut(const QString &shortcutName, QString *errorOut);
    bool invokeAnyShortcut(const QStringList &shortcutNames, QString *errorOut);
    bool setIntProperty(const QString &path,
                        const QString &iface,
                        const QString &property,
                        int value,
                        QString *errorOut);
    bool activateWindowByPath(const QString &path, QString *errorOut);
    bool invokeInputCaptureBridge(const QString &operation,
                                  const QString &sessionOrPayload,
                                  QString *errorOut);
    QVariantMap callInputCaptureBridge(const QString &methodName,
                                       const QVariantList &args);
    QString inputCaptureBridgeService() const;
    QString inputCaptureBridgePath() const;
    QString inputCaptureBridgeInterface() const;
    void publishEvent(const QString &eventName,
                      const QString &command,
                      bool ok,
                      const QString &error = QString());

    bool m_connected = false;
    QVariantMap m_lastEvent;
    QTimer *m_probeTimer = nullptr;
};
