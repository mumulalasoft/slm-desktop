#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// AppSandboxController — detect Flatpak/Bubblewrap availability and enumerate
// installed Flatpak apps with their sandbox permission summary.
//
// Reads metadata files from the Flatpak install paths directly; no per-app
// subprocess is needed.  One QProcess call is made to `flatpak list` to
// obtain app names and versions.
//
// Exposed to QML as the "AppSandbox" context property.
//
class AppSandboxController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool         flatpakAvailable    READ flatpakAvailable    NOTIFY flatpakAvailableChanged)
    Q_PROPERTY(bool         bubblewrapAvailable READ bubblewrapAvailable NOTIFY bubblewrapAvailableChanged)
    Q_PROPERTY(QVariantList apps                READ apps                NOTIFY appsChanged)
    Q_PROPERTY(bool         loading             READ loading             NOTIFY loadingChanged)
    Q_PROPERTY(QString      error               READ error               NOTIFY errorChanged)

public:
    explicit AppSandboxController(QObject *parent = nullptr);

    bool         flatpakAvailable()    const { return m_flatpakAvailable;    }
    bool         bubblewrapAvailable() const { return m_bubblewrapAvailable; }
    QVariantList apps()                const { return m_apps;                }
    bool         loading()             const { return m_loading;             }
    QString      error()               const { return m_error;               }

    Q_INVOKABLE void refresh();

signals:
    void flatpakAvailableChanged();
    void bubblewrapAvailableChanged();
    void appsChanged();
    void loadingChanged();
    void errorChanged();

private:
    void detectTools();
    void loadApps();

    // Parse ~/.local/share/flatpak/app/<id>/current/active/metadata (or the
    // system equivalent) and return a map with "permissions" and "sandboxLevel".
    QVariantMap parseMetadata(const QString &appId) const;

    // Derive a display-level sandbox strictness label from the permission sets.
    // Returns "full" | "partial" | "open".
    static QString sandboxLevel(const QStringList &filesystems,
                                const QStringList &devices);

    bool         m_flatpakAvailable    = false;
    bool         m_bubblewrapAvailable = false;
    QVariantList m_apps;
    bool         m_loading             = false;
    QString      m_error;
};
