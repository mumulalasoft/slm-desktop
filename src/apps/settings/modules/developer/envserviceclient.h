#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QDBusInterface;

// EnvServiceClient — thin D-Bus proxy for org.slm.Environment1.
//
// When the service is not running, all mutating calls fail gracefully and
// serviceAvailable returns false.  The settings UI shows a "service offline"
// warning in this state and falls back to reading the JSON file directly via
// EnvVariableController (Phase 1 path).
//
class EnvServiceClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)

public:
    explicit EnvServiceClient(QObject *parent = nullptr);

    bool serviceAvailable() const;

    // ── User-persistent store ────────────────────────────────────────────────
    bool addUserVar(const QString &key, const QString &value,
                    const QString &comment, const QString &mergeMode);
    bool updateUserVar(const QString &key, const QString &value,
                       const QString &comment, const QString &mergeMode);
    bool deleteUserVar(const QString &key);
    bool setUserVarEnabled(const QString &key, bool enabled);
    QVariantList getUserVars();

    // ── Session scope ────────────────────────────────────────────────────────
    bool setSessionVar(const QString &key, const QString &value,
                       const QString &mergeMode = QStringLiteral("replace"));
    bool unsetSessionVar(const QString &key);
    QVariantList getSessionVars();

    // ── Per-app overrides ────────────────────────────────────────────────────
    bool         addAppVar(const QString &appId, const QString &key,
                           const QString &value, const QString &mergeMode);
    bool         removeAppVar(const QString &appId, const QString &key);
    QVariantList getAppVars(const QString &appId);
    QStringList  listAppsWithOverrides();

    // ── System scope (via slm-envd → slm-envd-helper) ────────────────────────
    bool         writeSystemVar(const QString &key, const QString &value,
                                const QString &comment, const QString &mergeMode,
                                bool enabled);
    bool         deleteSystemVar(const QString &key);
    QVariantList getSystemVars();

    // ── Resolver ─────────────────────────────────────────────────────────────
    QVariantMap  resolveEnv(const QString &appId = {});
    QStringList  resolveEnvList(const QString &appId = {});

    QString lastError() const;

signals:
    void serviceAvailableChanged();
    void userVarsChanged();
    void sessionVarsChanged();
    void appVarsChanged(const QString &appId);
    void systemVarsChanged();

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                            const QString &newOwner);

private:
    bool ensureIface();
    bool callOk(const QVariantMap &reply);

    QDBusInterface *m_iface   = nullptr;
    bool            m_available = false;
    QString         m_lastError;
};
