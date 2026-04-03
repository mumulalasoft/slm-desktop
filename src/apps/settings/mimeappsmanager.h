#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

/**
 * Reads and writes ~/.config/mimeapps.list (freedesktop MIME default apps spec)
 * and scans XDG data dirs for installed .desktop files.
 */
class MimeAppsManager : public QObject
{
    Q_OBJECT

public:
    explicit MimeAppsManager(QObject *parent = nullptr);

    /**
     * Returns a list of {id, name, icon} maps for apps that handle @p mimeType.
     * Scans ~/.local/share/applications/ and /usr/share/applications/.
     */
    Q_INVOKABLE QVariantList appsForMimeType(const QString &mimeType) const;

    /**
     * Returns the desktop-file ID (e.g. "firefox.desktop") currently registered
     * as the default handler for @p mimeType in mimeapps.list, or "" if none.
     */
    Q_INVOKABLE QString defaultAppForMimeType(const QString &mimeType) const;

    /**
     * Writes @p desktopFileId as the default handler for @p mimeType into
     * ~/.config/mimeapps.list, creating the file and section if needed.
     */
    Q_INVOKABLE void setDefaultApp(const QString &mimeType, const QString &desktopFileId);

signals:
    void defaultAppChanged(const QString &mimeType, const QString &desktopFileId);

private:
    struct DesktopEntry {
        QString id;          // e.g. "firefox.desktop"
        QString name;
        QString icon;
        QStringList mimeTypes;
    };

    QList<DesktopEntry> scanDesktopFiles() const;
    QString mimeAppsListPath() const;
};
