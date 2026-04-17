#pragma once

#include "helperservice.h"

#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// HelperDbusInterface — D-Bus adaptor for org.slm.EnvironmentHelper1.
//
// Registered on the system bus.  Every mutating method:
//   1. Identifies the caller's PID via the system bus.
//   2. Runs `pkcheck` to verify org.slm.environment.write-system.
//   3. Delegates to HelperService on success.
//   4. Appends an audit log entry.
//
// Service name : org.slm.EnvironmentHelper1
// Object path  : /org/slm/EnvironmentHelper
//
class HelperDbusInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.EnvironmentHelper1")

public:
    explicit HelperDbusInterface(HelperService *service, QObject *parent = nullptr);

    bool registerOn(QDBusConnection &bus);

public slots:
    // Write (add or update) a system-scope environment variable.
    QVariantMap WriteSystemEntry(const QString &key, const QString &value,
                                 const QString &comment, const QString &mergeMode,
                                 bool enabled);

    // Delete a system-scope environment variable.
    QVariantMap DeleteSystemEntry(const QString &key);

    // Return all current system-scope entries (read-only, no polkit required).
    QVariantList GetSystemEntries();

signals:
    void SystemEntriesChanged();

private:
    // Returns (authorized, callerUid).
    // Runs pkcheck for org.slm.environment.write-system against the caller's PID.
    bool checkAuthorization(quint32 &outUid);

    static QVariantMap ok();
    static QVariantMap err(const QString &message);

    HelperService *m_service;
};
