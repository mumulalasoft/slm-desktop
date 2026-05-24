#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class ISharingAdapter : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Available,
        Unavailable,
        Degraded,
    };
    Q_ENUM(Status)

    explicit ISharingAdapter(QObject *parent = nullptr) : QObject(parent) {}
    ~ISharingAdapter() override = default;

    virtual QString adapterId() const = 0;
    virtual QStringList capabilities() const = 0;

    virtual Status probe() = 0;
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;
    virtual bool recover() = 0;

    virtual QVariantMap statusDetail() const { return {}; }

signals:
    void statusChanged(ISharingAdapter::Status status);
    void capabilityEvent(const QString &event, const QVariantMap &payload);
};
