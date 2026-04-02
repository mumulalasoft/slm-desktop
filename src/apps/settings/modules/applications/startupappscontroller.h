#pragma once
#include <QObject>
#include <QVariantList>
#include <QMap>
#include <QString>

// StartupAppsController — manages XDG autostart entries in ~/.config/autostart.
//
// Each QML-facing entry is a QVariantMap with keys:
//   id       QString  — desktop file stem (e.g. "firefox")
//   name     QString
//   icon     QString  — icon name or path
//   comment  QString
//   exec     QString
//   enabled  bool     — X-GNOME-Autostart-enabled (default true)

class StartupAppsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)

public:
    explicit StartupAppsController(QObject *parent = nullptr);

    QVariantList apps() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void removeApp(const QString &id);
    Q_INVOKABLE void addDesktopFile(const QString &sourcePath);
    Q_INVOKABLE void addEntry(const QString &name,
                              const QString &exec,
                              const QString &icon = {},
                              const QString &comment = {});

    // Returns list of installed applications from XDG data dirs.
    // Each entry: { name, exec, icon, comment, id }
    Q_INVOKABLE QVariantList installedApps() const;

signals:
    void appsChanged();

private:
    QVariantList m_apps;

    QString userDir() const;
    QString userPath(const QString &id) const;

    QMap<QString, QString> parseDesktopFile(const QString &filePath) const;
    void writeDesktopFile(const QString &filePath,
                          const QMap<QString, QString> &keys) const;
    void updateKey(const QString &filePath,
                   const QString &key,
                   const QString &value) const;
    bool ensureUserDir() const;
    QVariantMap buildEntry(const QString &id,
                           const QMap<QString, QString> &keys) const;
};
