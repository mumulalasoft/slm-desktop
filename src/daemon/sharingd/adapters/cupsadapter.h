#pragma once

#include "isharingadapter.h"

class CupsAdapter : public ISharingAdapter
{
    Q_OBJECT

public:
    explicit CupsAdapter(QObject *parent = nullptr);

    QString adapterId() const override { return QStringLiteral("cups"); }
    QStringList capabilities() const override { return {QStringLiteral("printer-sharing")}; }

    Status probe() override;
    bool activate() override;
    bool deactivate() override;
    bool recover() override;
    QVariantMap statusDetail() const override;

    QVariantList listLocalPrinters() const;
    bool sharePrinter(const QString &printerId, bool shared);

private:
    bool cupsctlAvailable() const;
    bool runCupsCtl(const QStringList &args) const;

    bool m_active = false;
};
