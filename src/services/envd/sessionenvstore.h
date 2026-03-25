#pragma once

#include "../../apps/settings/modules/developer/enventry.h"

#include <QList>
#include <QObject>
#include <QString>

// SessionEnvStore — in-memory, session-scoped environment overrides.
//
// These entries live only for the lifetime of the slm-envd process.
// They sit at EnvLayer::Session priority and are not persisted to disk.
//
class SessionEnvStore : public QObject
{
    Q_OBJECT

public:
    explicit SessionEnvStore(QObject *parent = nullptr);

    QList<EnvEntry> entries() const;

    bool setVar(const QString &key, const QString &value,
                const QString &mergeMode = QStringLiteral("replace"));
    bool unsetVar(const QString &key);
    bool setEnabled(const QString &key, bool enabled);

    void clear();

signals:
    void entriesChanged();

private:
    QList<EnvEntry> m_entries;
};
