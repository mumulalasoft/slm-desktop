#pragma once

#include "../../apps/settings/modules/developer/enventry.h"

#include <QList>
#include <QObject>
#include <QString>

// PerAppEnvStore — per-application environment overrides.
//
// Each app has its own JSON file at:
//   ~/.config/slm/environment.d/apps/<appid>.json
//
// The format is identical to the user-persistent store so the same
// EnvEntry::toJson/fromJson round-trip can be used.
//
class PerAppEnvStore : public QObject
{
    Q_OBJECT

public:
    explicit PerAppEnvStore(QObject *parent = nullptr);

    // Load entries for the given application ID.
    bool load(const QString &appId);
    bool save(const QString &appId);

    QList<EnvEntry> entries(const QString &appId) const;

    bool addEntry(const QString &appId, const EnvEntry &entry);
    bool updateEntry(const QString &appId, const QString &key, const EnvEntry &updated);
    bool removeEntry(const QString &appId, const QString &key);

    QString lastError() const;

signals:
    void appEntriesChanged(const QString &appId);

private:
    static QString filePath(const QString &appId);

    // appId → list of entries
    QHash<QString, QList<EnvEntry>> m_store;
    QString m_lastError;
};
