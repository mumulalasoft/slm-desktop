#pragma once
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <security/pam_appl.h>
#include <sys/types.h>

namespace Slm::Login {

// SlmPamSession handles the full PAM lifecycle without greetd:
//   authenticate() → openSession() → launchSession() → [session runs] → closeSession()
//
// VT management: the user session is assigned the next free VT so the greeter
// compositor (cage) can keep running on its own VT. On session exit the greeter
// VT is restored.
class SlmPamSession : public QObject
{
    Q_OBJECT
public:
    explicit SlmPamSession(QObject *parent = nullptr);
    ~SlmPamSession() override;

    // Phase 1 — PAM authentication. Returns false on any PAM failure.
    bool authenticate(const QString &username, const QString &password);

    // Phase 2 — Open PAM session (creates logind session on the target VT).
    // Must be called after authenticate() succeeds.
    bool openSession();

    // Phase 3 — Fork+exec the user session with privilege drop.
    // cmd is the full command list; if empty defaults to slm-session-broker --mode normal.
    // Returns the process PID, or -1 on failure.
    qint64 launchSession(const QStringList &cmd = {});

    // Phase 4 — Close PAM session (call when session process has exited).
    void closeSession();

    // Environment variables exported by PAM after openSession()
    // (e.g. XDG_RUNTIME_DIR, XDG_SESSION_ID set by pam_systemd.so).
    QMap<QString, QString> pamEnvironment() const { return m_pamEnv; }

    qint64 sessionPid() const { return m_sessionPid; }

signals:
    void sessionFinished(int exitCode);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    static int pamConversation(int numMsg, const struct pam_message **msg,
                               struct pam_response **resp, void *appdata);
    int  detectCurrentVT() const;
    int  findNextFreeVT() const;
    void switchToVT(int vt) const;

    pam_handle_t           *m_pamh        = nullptr;
    bool                    m_sessionOpen = false;
    QMap<QString, QString>  m_pamEnv;
    QString                 m_username;
    QString                 m_password;     // zeroed immediately after pam_authenticate
    uid_t                   m_uid         = 0;
    gid_t                   m_gid         = 0;
    int                     m_greeterVT   = 0;
    int                     m_sessionVT   = 0;
    qint64                  m_sessionPid  = -1;
    QProcess               *m_proc        = nullptr;
};

} // namespace Slm::Login
