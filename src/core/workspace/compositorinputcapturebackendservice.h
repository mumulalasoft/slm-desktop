#pragma once

#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

class CompositorInputCaptureBackendService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Compositor.InputCaptureBackend")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString activeSession READ activeSession NOTIFY activeSessionChanged)
    Q_PROPERTY(int enabledSessionCount READ enabledSessionCount NOTIFY enabledSessionCountChanged)

public:
    explicit CompositorInputCaptureBackendService(QObject *commandBridge,
                                                  QObject *parent = nullptr);
    ~CompositorInputCaptureBackendService() override;

    bool serviceRegistered() const;
    QString activeSession() const;
    int enabledSessionCount() const;

public slots:
    QVariantMap CreateSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options);
    QVariantMap SetPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options);
    QVariantMap EnableSession(const QString &sessionPath, const QVariantMap &options);
    QVariantMap DisableSession(const QString &sessionPath, const QVariantMap &options);
    QVariantMap ReleaseSession(const QString &sessionPath, const QString &reason);
    QVariantMap GetState() const;

signals:
    void serviceRegisteredChanged();
    void activeSessionChanged();
    void enabledSessionCountChanged();
    void CaptureActiveChanged(bool active, const QString &sessionHandle);
    void SessionStateChanged(const QString &sessionHandle, bool enabled, int barrierCount);

private:
    struct SessionRecord {
        QString sessionHandle;
        QString appId;
        QVariantList barriers;
        bool enabled = false;
    };

    void registerDbusService();
    QVariantMap ok(const QVariantMap &results) const;
    QVariantMap deny(const QString &reason, const QVariantMap &results = {}) const;
    bool validateBarriers(const QVariantList &barriers, QString *reasonOut) const;
    bool forwardCommand(const QString &command) const;
    bool debugEnabled() const;
    bool bridgeHasCapability(const QString &name) const;
    QVariantMap invokeStructuredPrimitiveOperation(const QString &method,
                                                   const QVariantList &args) const;
    QVariantMap invokePrimitiveOperation(const QString &method,
                                         const QVariantList &args) const;
    void setActiveSession(const QString &sessionHandle);
    void setEnabledSessionCount(int count);
    int recomputeEnabledSessionCount() const;
    QString firstEnabledSessionKey() const;

    QObject *m_commandBridge = nullptr;
    QHash<QString, SessionRecord> m_sessions;
    bool m_serviceRegistered = false;
    QString m_activeSession;
    int m_enabledSessionCount = 0;
};
