#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Slm::Login {

class ConfigManager
{
public:
    ConfigManager();

    // ~/.config/slm-desktop/
    static QString configDir();

    static QString activePath();    // config.json
    static QString prevPath();      // config.prev.json
    static QString safePath();      // config.safe.json
    static QString snapshotDir();   // snapshots/
    static QString snapshotPath(const QString &id);

    // Load active config from disk (silent on missing file).
    bool load();

    // Atomic: back up active → prev, write new as active.
    bool save(const QJsonObject &config, QString *error = nullptr);

    // Atomic: copy prev → active.
    bool rollbackToPrevious(QString *error = nullptr);

    // Atomic: copy safe → active.
    bool rollbackToSafe(QString *error = nullptr);

    // Snapshot active config; returns snapshot id (timestamp-based).
    QString snapshot(QString *error = nullptr);

    // Restore snapshot by id → active (atomic).
    bool restoreSnapshot(const QString &id, QString *error = nullptr);

    // Promote active → safe (mark current as known-good baseline).
    bool promoteToLastGood(QString *error = nullptr);

    bool hasPreviousConfig() const;
    bool hasSafeConfig() const;

    // Factory reset: remove all config files (active, prev, safe, snapshots)
    // and write a minimal default config as the new active config.
    bool factoryReset(QString *error = nullptr);

    QJsonObject currentConfig() const;

    // Convenience accessors with defaults.
    QString      compositor() const;
    QStringList  compositorArgs() const;
    QString      shell() const;
    QStringList  shellArgs() const;

private:
    static QJsonObject defaultConfig();
    static bool validateConfig(const QJsonObject &config,
                               QString *error = nullptr,
                               QJsonObject *normalized = nullptr);
    static bool atomicWriteJson(const QString &path,
                                const QJsonObject &obj,
                                QString *error = nullptr);
    static bool atomicCopy(const QString &src,
                           const QString &dst,
                           QString *error = nullptr);
    static QJsonObject loadFile(const QString &path);

    QJsonObject m_config;
};

} // namespace Slm::Login
