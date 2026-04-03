#pragma once

#include "svcmanagerservice.h"

#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// SvcManagerDbusInterface — D-Bus adaptor for org.slm.ServiceManager1.
//
// Service name : org.slm.ServiceManager1
// Object path  : /org/slm/ServiceManager
//
class SvcManagerDbusInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.ServiceManager1")

public:
    explicit SvcManagerDbusInterface(SvcManagerService *service, QObject *parent = nullptr);

    bool registerOn(QDBusConnection &bus);

public slots:
    QVariantList ListComponents();
    QVariantMap  GetComponent(const QString &name);
    QVariantMap  RestartComponent(const QString &name);
    QVariantMap  StopComponent(const QString &name);
    QString      ServiceVersion();

signals:
    void ComponentStateChanged(const QString &name, const QString &newStatus);
    void ComponentCrashed(const QString &name, int exitCode);

private:
    SvcManagerService *m_service;
};
