#pragma once

#include "portalbridgeservice.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// XdgPortalsController — QML controller for the XDG Portals settings page.
//
// Exposes portal descriptors, per-interface handler preferences, and write
// access to ~/.config/xdg-desktop-portal/portals.conf.
//
// QML properties:
//   interfaces   QStringList — sorted list of known portal interface names
//   descriptors  QVariantList — raw descriptor data (id, dbusName, ifaces, useIn)
//   overrides    QVariantMap  — current [preferred] section key→value
//   defaultHandler QString   — value of [preferred] default= key
//   lastError    QString
//
class XdgPortalsController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList  interfaces     READ interfaces     NOTIFY interfacesChanged)
    Q_PROPERTY(QVariantList descriptors    READ descriptors    NOTIFY descriptorsChanged)
    Q_PROPERTY(QVariantMap  overrides      READ overrides      NOTIFY overridesChanged)
    Q_PROPERTY(QString      defaultHandler READ defaultHandler NOTIFY overridesChanged)
    Q_PROPERTY(QString      lastError      READ lastError      NOTIFY lastErrorChanged)

public:
    explicit XdgPortalsController(QObject *parent = nullptr);

    QStringList  interfaces()     const;
    QVariantList descriptors()    const;
    QVariantMap  overrides()      const;
    QString      defaultHandler() const;
    QString      lastError()      const;

    // Handlers (descriptor ids) that implement a given interface.
    Q_INVOKABLE QVariantList handlersFor(const QString &iface) const;

    // Current preferred handler string for a given interface.
    Q_INVOKABLE QString preferredHandler(const QString &iface) const;

    // Persist a preferred handler override.  Pass empty string to reset.
    Q_INVOKABLE bool setPreferredHandler(const QString &iface, const QString &handler);

    // Reset all [preferred] entries back to system defaults.
    Q_INVOKABLE bool resetAllHandlers();

    // Re-read descriptors and portals.conf from disk.
    Q_INVOKABLE void refresh();

signals:
    void interfacesChanged();
    void descriptorsChanged();
    void overridesChanged();
    void lastErrorChanged();

private:
    PortalBridgeService m_service;
    QString             m_lastError;
};
