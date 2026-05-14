#pragma once

#include "isharingadapter.h"
#include <QProcess>
#include <QTimer>

class AvahiAdapter : public ISharingAdapter
{
    Q_OBJECT

public:
    explicit AvahiAdapter(QObject *parent = nullptr);
    ~AvahiAdapter() override;

    QString adapterId() const override { return QStringLiteral("avahi"); }
    QStringList capabilities() const override { return {QStringLiteral("nearby-discovery"), QStringLiteral("mdns-announce")}; }

    Status probe() override;
    bool activate() override;
    bool deactivate() override;
    bool recover() override;
    QVariantMap statusDetail() const override;

    bool announceService(const QString &serviceType, const QString &name, quint16 port, const QVariantMap &txtRecord);
    bool withdrawService(const QString &serviceType, const QString &name);
    void startBrowse(const QString &serviceType);
    void stopBrowse();

private slots:
    void onBrowseOutput();
    void onBrowseError();

private:
    bool avahiPublishAvailable() const;
    bool avahiBrowseAvailable() const;
    void parseBrowseLine(const QString &line);

    QProcess *m_browseProcess = nullptr;
    QTimer m_restartTimer;
    bool m_active = false;
};
