#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

// FsdPolkit wraps polkit authorization checks for slm-fsd verbs.
//
// We call org.freedesktop.PolicyKit1.Authority.CheckAuthorization() on the
// system bus directly, rather than linking libpolkit-qt6, to avoid a hard
// dependency on a library that may not be in the minimal system image.
//
// Each verb maps to a distinct polkit action ID under org.slm.desktop.fsd.*
//
// Authorization result:
//   "Authorized"        — proceed
//   "NotAuthorized"     — deny (user declined or no agent)
//   "ChallengeRequired" — should not occur in slm-fsd (agent is always present)
class FsdPolkit : public QObject
{
    Q_OBJECT

public:
    explicit FsdPolkit(QObject *parent = nullptr);

    // Map a verb name to its polkit action ID.
    // Returns empty string if the verb is unknown.
    static QString actionIdForVerb(const QString &verb);

    // Synchronously check authorization for the given action against the caller
    // identified by their D-Bus unique name on the system bus.
    //
    // callerUniqueName: e.g. ":1.42"
    // verb: e.g. "rename", "delete", "mount"
    //
    // Returns:
    //   "" (empty)            — authorized
    //   "PermissionDenied"    — denied by polkit
    //   "PolkitUnavailable"   — could not reach PolicyKit1
    //   "NotImplemented"      — stub; replace with real call in Phase 1
    QString checkAuthorization(const QString &callerUniqueName, const QString &verb);

private:
    // Real implementation (Phase 1): call PolicyKit1 D-Bus API.
    QString callPolicyKit(const QString &callerUniqueName, const QString &actionId);
};
