#pragma once

#include "envservice.h"

#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// DbusInterface — hand-written D-Bus adaptor for org.slm.Environment1.
//
// Service name : org.slm.Environment1
// Object path  : /org/slm/Environment
//
// Public slots are exported as D-Bus methods.
// Signals are exported as D-Bus signals.
//
// All mutating methods return a (bool ok, QString error) struct encoded as a
// QVariantMap with keys "ok" (bool) and "error" (string) so callers can
// distinguish success from failure without throwing D-Bus errors.
//
class DbusInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Environment1")

public:
    explicit DbusInterface(EnvService *service, QObject *parent = nullptr);

    bool registerOn(QDBusConnection &bus);

public slots:
    // ── User-persistent store ────────────────────────────────────────────────
    QVariantMap AddUserVar(const QString &key, const QString &value,
                           const QString &comment, const QString &mergeMode);
    QVariantMap UpdateUserVar(const QString &key, const QString &value,
                              const QString &comment, const QString &mergeMode);
    QVariantMap DeleteUserVar(const QString &key);
    QVariantMap SetUserVarEnabled(const QString &key, bool enabled);
    QVariantList GetUserVars();

    // ── Session scope ────────────────────────────────────────────────────────
    QVariantMap SetSessionVar(const QString &key, const QString &value,
                              const QString &mergeMode);
    QVariantMap UnsetSessionVar(const QString &key);
    QVariantList GetSessionVars();

    // ── Per-app scope ────────────────────────────────────────────────────────
    QVariantMap AddAppVar(const QString &appId, const QString &key,
                          const QString &value, const QString &mergeMode);
    QVariantMap RemoveAppVar(const QString &appId, const QString &key);

    // ── Resolver ─────────────────────────────────────────────────────────────
    QVariantMap  ResolveEnv(const QString &appId);
    QStringList  ResolveEnvList(const QString &appId);

    // ── Per-app discovery ────────────────────────────────────────────────────
    QVariantList GetAppVars(const QString &appId);
    QStringList  ListAppsWithOverrides();

    // ── System scope ─────────────────────────────────────────────────────────
    QVariantMap  WriteSystemVar(const QString &key, const QString &value,
                                const QString &comment, const QString &mergeMode,
                                bool enabled);
    QVariantMap  DeleteSystemVar(const QString &key);
    QVariantList GetSystemVars();

    // ── Introspection ────────────────────────────────────────────────────────
    QString ServiceVersion();

signals:
    void UserVarsChanged();
    void SessionVarsChanged();
    void AppVarsChanged(const QString &appId);
    void SystemVarsChanged();

private:
    static QVariantMap ok();
    static QVariantMap err(const QString &message);

    // Returns true if the current D-Bus caller is running inside a sandbox
    // (Flatpak or Snap).  If the caller is Flatpak, *outAppId is set to the
    // value of FLATPAK_ID from the caller's environment (may be empty if
    // /proc is unreadable).
    bool callerIsSandboxed(QString *outAppId = nullptr) const;

    EnvService *m_service;
};
