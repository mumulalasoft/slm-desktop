#pragma once

#include "isharingadapter.h"

class SshAdapter : public ISharingAdapter
{
    Q_OBJECT

public:
    explicit SshAdapter(QObject *parent = nullptr);

    QString adapterId() const override { return QStringLiteral("ssh"); }
    QStringList capabilities() const override { return {QStringLiteral("remote-access")}; }

    Status probe() override;
    bool activate() override;
    bool deactivate() override;
    bool recover() override;
    QVariantMap statusDetail() const override;

    bool addAuthorizedKey(const QString &publicKey, const QString &deviceLabel);
    bool removeAuthorizedKey(const QString &deviceLabel);
    QVariantList listAuthorizedDevices() const;

private:
    bool sshdAvailable() const;
    bool sshdRunning() const;
    bool startSshd() const;
    bool stopSshd() const;
    QString authorizedKeysPath() const;
    QString commentForLabel(const QString &label) const;

    bool m_active = false;
};
