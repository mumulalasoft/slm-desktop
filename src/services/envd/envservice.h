#pragma once

#include "envtypes.h"
#include "envstore.h"
#include "mergeresolver.h"
#include "perappenvstore.h"
#include "sessionenvstore.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// EnvService — the central business-logic object for slm-envd.
//
// Owns all stores and the resolver.  DbusInterface delegates to this class.
// Does NOT know about D-Bus; tests can drive it directly.
//
class EnvService : public QObject
{
    Q_OBJECT

public:
    explicit EnvService(QObject *parent = nullptr);

    bool start();

    // ── User-persistent store operations (via Settings app) ─────────────────

    bool addUserVar(const QString &key, const QString &value,
                    const QString &comment, const QString &mergeMode);
    bool updateUserVar(const QString &key, const QString &value,
                       const QString &comment, const QString &mergeMode);
    bool deleteUserVar(const QString &key);
    bool setUserVarEnabled(const QString &key, bool enabled);

    // Returns all user-persistent entries as a list of QVariantMaps
    // (key, value, enabled, comment, mergeMode, modifiedAt, severity).
    QVariantList userVars() const;

    // ── Session-scope operations ─────────────────────────────────────────────

    bool setSessionVar(const QString &key, const QString &value,
                       const QString &mergeMode = QStringLiteral("replace"));
    bool unsetSessionVar(const QString &key);

    // ── Per-app operations ───────────────────────────────────────────────────

    bool addAppVar(const QString &appId, const QString &key,
                   const QString &value, const QString &mergeMode);
    bool removeAppVar(const QString &appId, const QString &key);

    // ── Per-app discovery ────────────────────────────────────────────────────
    QVariantList appVars(const QString &appId) const;
    QStringList  appsWithOverrides() const;

    // ── Resolver ─────────────────────────────────────────────────────────────

    // Returns the fully-merged environment for a given app ID.
    // Pass an empty appId to get the session-wide merged view.
    QVariantMap resolveEnv(const QString &appId = {}) const;

    // Returns the merged environment as a flat string list "KEY=VALUE"
    // suitable for passing to QProcess::setEnvironment().
    QStringList resolveEnvList(const QString &appId = {}) const;

    QString lastError() const;

signals:
    void userVarsChanged();
    void sessionVarsChanged();
    void appVarsChanged(const QString &appId);

private:
    void rebuildUserVarCache();

    EnvStore        m_userStore;
    SessionEnvStore m_sessionStore;
    PerAppEnvStore  m_perAppStore;

    QString m_lastError;
};
