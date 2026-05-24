#pragma once

#include "isharingadapter.h"
#include <QProcess>

class SambaAdapter : public ISharingAdapter
{
    Q_OBJECT

public:
    explicit SambaAdapter(QObject *parent = nullptr);

    QString adapterId() const override { return QStringLiteral("samba"); }
    QStringList capabilities() const override { return {QStringLiteral("file-sharing")}; }

    Status probe() override;
    bool activate() override;
    bool deactivate() override;
    bool recover() override;
    QVariantMap statusDetail() const override;

    bool configureShare(const QString &path, const QVariantMap &options);
    bool removeShare(const QString &path);
    QVariantList listShares() const;
    QVariantMap applyShare(const QString &shareName, const QString &path,
                           const QString &acl, bool guestOk);
    QVariantMap deleteShare(const QString &shareName);

private:
    bool runUsershareCommand(const QStringList &args) const;
    bool smbNetAvailable() const;
    bool usersharePermission() const;
    QString sanitizeName(const QString &path) const;

    bool m_active = false;
};
