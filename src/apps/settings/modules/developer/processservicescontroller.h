#pragma once

#include "svcmanagerclient.h"

#include <QObject>
#include <QString>
#include <QVariantList>

// ProcessServicesController — QML-facing controller for the Process & Services page.
//
// Exposed to QML as the "ProcessServicesController" context property.
//
class ProcessServicesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool        serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(QVariantList components      READ components       NOTIFY componentsChanged)
    Q_PROPERTY(QString     lastError        READ lastError        NOTIFY lastErrorChanged)

public:
    explicit ProcessServicesController(SvcManagerClient *client, QObject *parent = nullptr);

    bool         serviceAvailable() const;
    QVariantList components()       const { return m_components; }
    QString      lastError()        const { return m_lastError;  }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool restart(const QString &name);
    Q_INVOKABLE bool stop(const QString &name);

signals:
    void serviceAvailableChanged();
    void componentsChanged();
    void lastErrorChanged();

private slots:
    void onServiceAvailableChanged();
    void onComponentStateChanged(const QString &name, const QString &newStatus);

private:
    void updateComponentStatus(const QString &name, const QString &status);

    SvcManagerClient *m_client;
    QVariantList      m_components;
    QString           m_lastError;
};
