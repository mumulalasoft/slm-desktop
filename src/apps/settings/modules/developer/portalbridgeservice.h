#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// PortalBridgeService — in-process reader for XDG Desktop Portal descriptors.
//
// Reads all *.portal files from /usr/share/xdg-desktop-portal/portals/ and
// the user's portals.conf from ~/.config/xdg-desktop-portal/portals.conf,
// exposing them through a QObject API consumed by XdgPortalsController.
//
// Portal descriptor format (INI):
//   [portal]
//   DBusName=org.freedesktop.impl.portal.desktop.gtk
//   Interfaces=org.freedesktop.impl.portal.FileChooser;...
//   UseIn=gnome;kde;...
//
// portals.conf format (INI):
//   [preferred]
//   default=impl1;impl2;
//   org.freedesktop.impl.portal.FileChooser=gtk;
//
class PortalBridgeService : public QObject
{
    Q_OBJECT

public:
    explicit PortalBridgeService(QObject *parent = nullptr);

    // ── Descriptors ──────────────────────────────────────────────────────────

    // All known portal implementations read from /usr/share/.../portals/*.portal.
    // Each entry is a QVariantMap with keys:
    //   id       QString  — stem of the .portal filename (e.g. "gtk")
    //   dbusName QString  — org.freedesktop.impl.portal.desktop.gtk
    //   ifaces   QStringList — list of implemented interface names
    //   useIn    QStringList — list of desktop environment names
    QVariantList descriptors() const { return m_descriptors; }

    // All unique interface names across all descriptors.
    QStringList interfaces() const { return m_interfaces; }

    // Descriptors that implement a given interface name.
    QVariantList handlersFor(const QString &iface) const;

    // ── portals.conf ─────────────────────────────────────────────────────────

    // Current preferred handler for a given interface (or "default" key).
    // Returns semicolon-separated fallback list, e.g. "gtk;gnome".
    QString preferredHandler(const QString &iface) const;

    // All non-default overrides in portals.conf: QVariantMap of iface → handler.
    QVariantMap handlerOverrides() const { return m_overrides; }

    // Default fallback list from portals.conf [preferred] default= key.
    QString defaultHandler() const { return m_defaultHandler; }

    // Write a preferred handler for an interface.
    // Pass an empty string to remove the override (reset to default).
    bool setPreferredHandler(const QString &iface, const QString &handler);

    // Reset all overrides — removes portals.conf (or clears [preferred]).
    bool resetAllHandlers();

    QString lastError() const { return m_lastError; }

    void reload();

signals:
    void descriptorsChanged();
    void configChanged();

private:
    void loadDescriptors();
    void loadConfig();
    bool saveConfig();

    QVariantList m_descriptors;
    QStringList  m_interfaces;
    QVariantMap  m_overrides;      // iface → handler string
    QString      m_defaultHandler; // value of [preferred] default=

    QFileSystemWatcher m_watcher;
    QString            m_configPath;
    QString            m_lastError;
};
