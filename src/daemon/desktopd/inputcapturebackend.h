#pragma once

#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <memory>

class InputCaptureBackend
{
public:
    virtual ~InputCaptureBackend() = default;

    virtual QString backendName() const = 0;
    virtual QVariantMap capabilityReport() const = 0;

    virtual QVariantMap createSession(const QString &sessionPath,
                                      const QString &appId,
                                      const QVariantMap &options) = 0;
    virtual QVariantMap setPointerBarriers(const QString &sessionPath,
                                           const QVariantList &barriers,
                                           const QVariantMap &options) = 0;
    virtual QVariantMap enableSession(const QString &sessionPath, const QVariantMap &options) = 0;
    virtual QVariantMap disableSession(const QString &sessionPath, const QVariantMap &options) = 0;
    virtual QVariantMap releaseSession(const QString &sessionPath, const QString &reason) = 0;

    static std::unique_ptr<InputCaptureBackend> createDefault();
};

