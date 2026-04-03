#pragma once

#include "componentinfo.h"

#include <QDBusInterface>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// SvcManagerService — monitors systemd user-session units for registered
// shell components and exposes restart/stop operations.
//
// Component list is hardcoded to shell-owned services only.
// Does NOT act as a general systemd manager.
//
class SvcManagerService : public QObject
{
    Q_OBJECT

public:
    explicit SvcManagerService(QObject *parent = nullptr);
    ~SvcManagerService() override;

    bool start();

    QVariantList listComponents() const;
    QVariantMap  getComponent(const QString &name) const;

    // Returns {ok: bool, error: string}
    QVariantMap restartComponent(const QString &name);
    QVariantMap stopComponent(const QString &name);

    QString lastError() const { return m_lastError; }

signals:
    void componentStateChanged(const QString &name, const QString &newStatus);
    void componentCrashed(const QString &name, int exitCode);

private slots:
    void pollAll();
    void onSystemdUnitPropertiesChanged(const QString &interface,
                                         const QVariantMap &changed,
                                         const QStringList &invalidated);

private:
    void initComponents();
    void updateComponent(ComponentInfo &info);
    QString systemdUnitState(const QString &unitName) const;
    qint64  systemdUnitPid(const QString &unitName) const;
    qint64  unitActiveSince(const QString &unitName) const;
    void    readProcStats(ComponentInfo &info) const;
    QDBusInterface *systemdManager() const;

    QList<ComponentInfo> m_components;
    QTimer               m_pollTimer;
    QString              m_lastError;
    mutable QDBusInterface *m_systemd = nullptr;
};
