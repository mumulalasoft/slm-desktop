#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class CompositorInputCapturePrimitiveService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Compositor.InputCapture")
    Q_PROPERTY(bool objectRegistered READ objectRegistered NOTIFY objectRegisteredChanged)

public:
    explicit CompositorInputCapturePrimitiveService(QObject *commandBridge,
                                                    QObject *parent = nullptr);
    ~CompositorInputCapturePrimitiveService() override;

    bool objectRegistered() const;

public slots:
    QVariantMap SetPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options);
    QVariantMap EnableSession(const QString &sessionPath, const QVariantMap &options);
    QVariantMap DisableSession(const QString &sessionPath, const QVariantMap &options);
    QVariantMap ReleaseSession(const QString &sessionPath, const QString &reason);
    QVariantMap GetState() const;

signals:
    void objectRegisteredChanged();

private:
    void registerDbusObject();
    QVariantMap ok(const QVariantMap &results = {}) const;
    QVariantMap fail(const QString &reason, const QVariantMap &results = {}) const;
    QVariantMap invokeStructured(const QString &method, const QVariantList &args) const;
    bool forwardCommand(const QString &command) const;
    bool validateBarriers(const QVariantList &barriers, QString *reasonOut) const;

    QObject *m_commandBridge = nullptr;
    bool m_objectRegistered = false;
};
