#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class KWinWaylandIpcClient;
class KWinWaylandStateModel;

class WindowingBackendManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(int protocolVersion READ protocolVersion NOTIFY protocolVersionChanged)
    Q_PROPERTY(int eventSchemaVersion READ eventSchemaVersion NOTIFY eventSchemaVersionChanged)

public:
    static QVariantMap baseCapabilitiesForBackend(const QString &backend);

    explicit WindowingBackendManager(QObject *parent = nullptr);
    ~WindowingBackendManager() override;

    QString backend() const;
    bool connected() const;
    QVariantMap capabilities() const;
    int protocolVersion() const;
    int eventSchemaVersion() const;

    Q_INVOKABLE QStringList availableBackends() const;
    Q_INVOKABLE QString canonicalizeBackend(const QString &value) const;
    Q_INVOKABLE bool configureBackend(const QString &requestedBackend);
    Q_INVOKABLE bool sendCommand(const QString &command);
    Q_INVOKABLE bool hasCapability(const QString &name) const;
    Q_INVOKABLE QVariantMap setInputCapturePointerBarriers(const QString &sessionPath,
                                                           const QVariantList &barriers,
                                                           const QVariantMap &options);
    Q_INVOKABLE QVariantMap enableInputCaptureSession(const QString &sessionPath,
                                                      const QVariantMap &options);
    Q_INVOKABLE QVariantMap disableInputCaptureSession(const QString &sessionPath,
                                                       const QVariantMap &options);
    Q_INVOKABLE QVariantMap releaseInputCaptureSession(const QString &sessionPath,
                                                       const QString &reason);

    QObject *compositorStateObject() const;

signals:
    void backendChanged();
    void connectedChanged();
    void capabilitiesChanged();
    void protocolVersionChanged();
    void eventSchemaVersionChanged();
    void eventReceived(const QString &event, const QVariantMap &payload);

private:
    void wireSignals();

    QString m_backend = QStringLiteral("kwin-wayland");
    KWinWaylandIpcClient *m_kwinIpc = nullptr;
    KWinWaylandStateModel *m_kwinState = nullptr;
};
