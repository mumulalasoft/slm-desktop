#include "slmpamsession.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <unistd.h>

namespace Slm::Login {

namespace {

bool ensureRuntimeDir(const QString &path, uid_t uid, gid_t gid)
{
    const QByteArray bytes = path.toLocal8Bit();
    struct stat st{};
    if (::stat(bytes.constData(), &st) != 0) {
        if (errno != ENOENT) {
            qWarning("SLM-PAM: stat('%s') failed: %s", bytes.constData(), strerror(errno));
            return false;
        }
        if (::mkdir(bytes.constData(), 0700) != 0 && errno != EEXIST) {
            qWarning("SLM-PAM: mkdir('%s') failed: %s", bytes.constData(), strerror(errno));
            return false;
        }
        if (::stat(bytes.constData(), &st) != 0) {
            qWarning("SLM-PAM: stat('%s') after mkdir failed: %s", bytes.constData(), strerror(errno));
            return false;
        }
    }

    if (!S_ISDIR(st.st_mode)) {
        qWarning("SLM-PAM: XDG_RUNTIME_DIR path is not a directory: %s", bytes.constData());
        return false;
    }
    if ((st.st_uid != uid || st.st_gid != gid) && ::chown(bytes.constData(), uid, gid) != 0) {
        qWarning("SLM-PAM: chown('%s', %u, %u) failed: %s",
                 bytes.constData(), uid, gid, strerror(errno));
        return false;
    }
    if ((st.st_mode & 0777) != 0700 && ::chmod(bytes.constData(), 0700) != 0) {
        qWarning("SLM-PAM: chmod('%s', 0700) failed: %s", bytes.constData(), strerror(errno));
        return false;
    }
    return true;
}

} // namespace

SlmPamSession::SlmPamSession(QObject *parent) : QObject(parent) {}

SlmPamSession::~SlmPamSession()
{
    closeSession();
}

// ── PAM conversation (C callback) ─────────────────────────────────────────────

int SlmPamSession::pamConversation(int numMsg, const struct pam_message **msg,
                                    struct pam_response **resp, void *appdata)
{
    auto *self = static_cast<SlmPamSession *>(appdata);
    *resp = static_cast<struct pam_response *>(calloc(numMsg, sizeof(struct pam_response)));
    if (!*resp) return PAM_BUF_ERR;

    for (int i = 0; i < numMsg; ++i) {
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            (*resp)[i].resp = strdup(self->m_password.toLocal8Bit().constData());
            (*resp)[i].resp_retcode = PAM_SUCCESS;
            break;
        case PAM_ERROR_MSG:
            qWarning("SLM-PAM: pam error_msg: %s", msg[i]->msg ? msg[i]->msg : "");
            break;
        case PAM_TEXT_INFO:
            qInfo("SLM-PAM: pam text_info: %s", msg[i]->msg ? msg[i]->msg : "");
            break;
        default:
            break;
        }
    }
    return PAM_SUCCESS;
}

// ── VT helpers ────────────────────────────────────────────────────────────────

int SlmPamSession::detectCurrentVT() const
{
    // Try to read VT number from our stdin TTY path.
    const char *tty = ttyname(STDIN_FILENO);
    if (tty && strncmp(tty, "/dev/tty", 8) == 0) {
        const int n = atoi(tty + 8);
        if (n > 0) return n;
    }
    // Fallback: ask the kernel which VT is active.
    int fd = open("/dev/tty0", O_RDONLY | O_CLOEXEC);
    if (fd < 0) fd = open("/dev/console", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        struct vt_stat vts{};
        if (ioctl(fd, VT_GETSTATE, &vts) == 0) {
            close(fd);
            return static_cast<int>(vts.v_active);
        }
        close(fd);
    }
    return 1; // safe fallback
}

int SlmPamSession::findNextFreeVT() const
{
    int fd = open("/dev/tty0", O_RDONLY | O_CLOEXEC);
    if (fd < 0) fd = open("/dev/console", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        int nextVT = 0;
        if (ioctl(fd, VT_OPENQRY, &nextVT) == 0 && nextVT > 0) {
            close(fd);
            return nextVT;
        }
        close(fd);
    }
    return 2; // safe fallback
}

void SlmPamSession::switchToVT(int vt) const
{
    if (vt <= 0) return;
    int fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if (fd < 0) fd = open("/dev/console", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        qInfo("SLM-PAM: switching to VT%d", vt);
        ioctl(fd, VT_ACTIVATE, vt);
        ioctl(fd, VT_WAITACTIVE, vt);
        close(fd);
    }
}

// ── Phase 1: authenticate ─────────────────────────────────────────────────────

bool SlmPamSession::authenticate(const QString &username, const QString &password)
{
    qInfo("SLM-PAM: authenticate user='%s'", qPrintable(username));

    if (m_pamh) {
        pam_end(m_pamh, PAM_SUCCESS);
        m_pamh = nullptr;
        m_sessionOpen = false;
    }

    m_username  = username;
    m_password  = password;
    m_greeterVT = detectCurrentVT();
    m_sessionVT = findNextFreeVT();
    // If greeter and session would land on the same VT, push session one higher.
    if (m_sessionVT == m_greeterVT) m_sessionVT = m_greeterVT + 1;

    qInfo("SLM-PAM: greeter_vt=%d session_vt=%d", m_greeterVT, m_sessionVT);

    const struct pam_conv conv = { &SlmPamSession::pamConversation, this };
    int ret = pam_start("slm", username.toLocal8Bit().constData(), &conv, &m_pamh);
    if (ret != PAM_SUCCESS) {
        qWarning("SLM-PAM: pam_start failed: %s", pam_strerror(m_pamh, ret));
        m_pamh = nullptr;
        return false;
    }

    const QByteArray ttyName = "/dev/tty" + QByteArray::number(m_sessionVT);
    ret = pam_set_item(m_pamh, PAM_TTY, ttyName.constData());
    if (ret != PAM_SUCCESS) {
        qWarning("SLM-PAM: pam_set_item(PAM_TTY=%s) failed: %s",
                 ttyName.constData(), pam_strerror(m_pamh, ret));
    }

    // Set session vars BEFORE pam_open_session so pam_systemd.so picks them up.
    pam_putenv(m_pamh, "XDG_SESSION_TYPE=wayland");
    pam_putenv(m_pamh, "XDG_SESSION_CLASS=user");
    pam_putenv(m_pamh, "XDG_SESSION_DESKTOP=slm");
    pam_putenv(m_pamh, "DESKTOP_SESSION=slm");
    pam_putenv(m_pamh, "XDG_SEAT=seat0");
    {
        const QByteArray vtnrEnv = "XDG_VTNR=" + QByteArray::number(m_sessionVT);
        pam_putenv(m_pamh, vtnrEnv.constData());
    }

    ret = pam_authenticate(m_pamh, 0);
    // Zero password immediately — it has already been handed to PAM.
    m_password.fill(QChar(0));
    m_password.clear();

    if (ret != PAM_SUCCESS) {
        qWarning("SLM-PAM: pam_authenticate failed: %s", pam_strerror(m_pamh, ret));
        pam_end(m_pamh, ret);
        m_pamh = nullptr;
        return false;
    }

    ret = pam_acct_mgmt(m_pamh, 0);
    if (ret != PAM_SUCCESS) {
        qWarning("SLM-PAM: pam_acct_mgmt failed: %s", pam_strerror(m_pamh, ret));
        pam_end(m_pamh, ret);
        m_pamh = nullptr;
        return false;
    }

    ret = pam_setcred(m_pamh, PAM_ESTABLISH_CRED);
    if (ret != PAM_SUCCESS) {
        qWarning("SLM-PAM: pam_setcred(ESTABLISH_CRED) failed: %s", pam_strerror(m_pamh, ret));
        pam_end(m_pamh, ret);
        m_pamh = nullptr;
        return false;
    }

    qInfo("SLM-PAM: authentication complete for user='%s'", qPrintable(username));
    return true;
}

// ── Phase 2: openSession ──────────────────────────────────────────────────────

bool SlmPamSession::openSession()
{
    if (!m_pamh) {
        qWarning("SLM-PAM: openSession called without valid PAM handle");
        return false;
    }

    const struct passwd *pw = getpwnam(m_username.toLocal8Bit().constData());
    if (!pw) {
        qWarning("SLM-PAM: getpwnam('%s'): %s", qPrintable(m_username), strerror(errno));
        return false;
    }
    m_uid = pw->pw_uid;
    m_gid = pw->pw_gid;
    qInfo("SLM-PAM: user uid=%u gid=%u home='%s' shell='%s'",
          m_uid, m_gid, pw->pw_dir, pw->pw_shell);

    const int ret = pam_open_session(m_pamh, 0);
    if (ret != PAM_SUCCESS) {
        qWarning("SLM-PAM: pam_open_session failed: %s", pam_strerror(m_pamh, ret));
        pam_setcred(m_pamh, PAM_DELETE_CRED);
        pam_end(m_pamh, ret);
        m_pamh = nullptr;
        return false;
    }
    m_sessionOpen = true;
    qInfo("SLM-PAM: pam_open_session success");

    // Harvest env set by PAM modules (pam_systemd.so sets XDG_RUNTIME_DIR, etc.).
    m_pamEnv.clear();
    char **envList = pam_getenvlist(m_pamh);
    if (envList) {
        for (int i = 0; envList[i]; ++i) {
            const QByteArray kv(envList[i]);
            const int eq = kv.indexOf('=');
            if (eq > 0)
                m_pamEnv.insert(QString::fromLocal8Bit(kv.left(eq)),
                                QString::fromLocal8Bit(kv.mid(eq + 1)));
            free(envList[i]);
        }
        free(envList);
    }

    // Validation — warn but do not abort; session may still work.
    QString runtimeDir = m_pamEnv.value(QStringLiteral("XDG_RUNTIME_DIR"));
    const QString fallbackRuntimeDir = QStringLiteral("/run/user/") + QString::number(m_uid);
    if (runtimeDir.isEmpty()) {
        runtimeDir = fallbackRuntimeDir;
        if (ensureRuntimeDir(runtimeDir, m_uid, m_gid)) {
            m_pamEnv.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir);
            qWarning("SLM-PAM: pam_systemd.so did not export XDG_RUNTIME_DIR; "
                     "created fallback runtime dir '%s'", qPrintable(runtimeDir));
        }
    } else if (runtimeDir == fallbackRuntimeDir) {
        ensureRuntimeDir(runtimeDir, m_uid, m_gid);
    }

    const QString sessionId  = m_pamEnv.value(QStringLiteral("XDG_SESSION_ID"));
    qInfo("SLM-PAM: XDG_RUNTIME_DIR='%s' XDG_SESSION_ID='%s'",
          qPrintable(runtimeDir), qPrintable(sessionId));

    if (runtimeDir.isEmpty()) {
        qWarning("SLM-PAM: XDG_RUNTIME_DIR not set by pam_systemd.so — "
                 "check /etc/pam.d/slm contains 'session required pam_systemd.so'");
    } else {
        const QFileInfo fi(runtimeDir);
        if (!fi.exists() || !fi.isDir()) {
            qWarning("SLM-PAM: XDG_RUNTIME_DIR='%s' does not exist", qPrintable(runtimeDir));
        } else if (fi.ownerId() != m_uid) {
            qWarning("SLM-PAM: XDG_RUNTIME_DIR owner mismatch (owner=%u expected=%u)",
                     fi.ownerId(), m_uid);
        } else {
            qInfo("SLM-PAM: XDG_RUNTIME_DIR validated OK");
        }
    }

    return true;
}

// ── Phase 3: launchSession ────────────────────────────────────────────────────

qint64 SlmPamSession::launchSession(const QStringList &cmdOverride)
{
    if (!m_sessionOpen) {
        qWarning("SLM-PAM: launchSession called but session is not open");
        return -1;
    }

    const struct passwd *pw = getpwnam(m_username.toLocal8Bit().constData());
    if (!pw) {
        qWarning("SLM-PAM: launchSession: getpwnam failed");
        return -1;
    }

    // Build child process environment from PAM env + mandatory session vars.
    QProcessEnvironment childQEnv;
    for (auto it = m_pamEnv.constBegin(); it != m_pamEnv.constEnd(); ++it)
        childQEnv.insert(it.key(), it.value());

    auto setIfMissing = [&](const QString &key, const QString &val) {
        if (!childQEnv.contains(key)) childQEnv.insert(key, val);
    };

    const QString runtimeDir = m_pamEnv.value(QStringLiteral("XDG_RUNTIME_DIR"),
        QStringLiteral("/run/user/") + QString::number(m_uid));

    childQEnv.insert(QStringLiteral("USER"),              m_username);
    childQEnv.insert(QStringLiteral("LOGNAME"),           m_username);
    childQEnv.insert(QStringLiteral("HOME"),              QString::fromLocal8Bit(pw->pw_dir));
    childQEnv.insert(QStringLiteral("SHELL"),             QString::fromLocal8Bit(pw->pw_shell));
    childQEnv.insert(QStringLiteral("XDG_RUNTIME_DIR"),   runtimeDir);
    childQEnv.insert(QStringLiteral("XDG_SESSION_TYPE"),  QStringLiteral("wayland"));
    childQEnv.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("SLM"));
    childQEnv.insert(QStringLiteral("XDG_SESSION_DESKTOP"), QStringLiteral("slm"));
    childQEnv.insert(QStringLiteral("DESKTOP_SESSION"),   QStringLiteral("slm"));
    childQEnv.insert(QStringLiteral("SLM_OFFICIAL_SESSION"), QStringLiteral("1"));
    childQEnv.insert(QStringLiteral("LIBSEAT_BACKEND"),   QStringLiteral("logind"));
    setIfMissing(QStringLiteral("QT_QPA_PLATFORM"),       QStringLiteral("wayland"));
    setIfMissing(QStringLiteral("GDK_BACKEND"),           QStringLiteral("wayland,x11"));
    setIfMissing(QStringLiteral("MOZ_ENABLE_WAYLAND"),    QStringLiteral("1"));
    if (m_sessionVT > 0)
        childQEnv.insert(QStringLiteral("XDG_VTNR"), QString::number(m_sessionVT));

    // Default command: slm-session-broker --mode normal
    const QStringList fullCmd = cmdOverride.isEmpty()
        ? QStringList{QStringLiteral("slm-session-broker"), QStringLiteral("--mode"),
                      QStringLiteral("normal")}
        : cmdOverride;

    const uid_t uid = m_uid;
    const gid_t gid = m_gid;
    const int sessionVT = m_sessionVT;
    const std::string usernameStd = m_username.toStdString();
    const std::string homeDirStd  = std::string(pw->pw_dir);
    const std::string ttyPathStd = "/dev/tty" + std::to_string(m_sessionVT);

    m_proc = new QProcess(this);
    m_proc->setProgram(fullCmd.first());
    m_proc->setArguments(QStringList(fullCmd.constBegin() + 1, fullCmd.constEnd()));
    m_proc->setProcessEnvironment(childQEnv);
    m_proc->setProcessChannelMode(QProcess::ForwardedChannels);

    m_proc->setChildProcessModifier([uid, gid, sessionVT, usernameStd, homeDirStd, ttyPathStd]() {
        // Runs in child between fork() and exec() — only POSIX calls, no Qt.
        if (sessionVT > 0) {
            if (setsid() < 0) {
                const char msg[] = "SLM-PAM: setsid failed\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                _exit(126);
            }

            const int ttyFd = open(ttyPathStd.c_str(), O_RDWR | O_CLOEXEC);
            if (ttyFd < 0) {
                const char msg[] = "SLM-PAM: open session tty failed\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                _exit(126);
            }
            if (ioctl(ttyFd, TIOCSCTTY, 1) < 0) {
                const char msg[] = "SLM-PAM: TIOCSCTTY failed\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                close(ttyFd);
                _exit(126);
            }
            dup2(ttyFd, STDIN_FILENO);
            close(ttyFd);
        }

        if (initgroups(usernameStd.c_str(), gid) < 0) {
            const char msg[] = "SLM-PAM: initgroups failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            _exit(126);
        }
        if (setgid(gid) < 0) {
            const char msg[] = "SLM-PAM: setgid failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            _exit(126);
        }
        if (setuid(uid) < 0) {
            const char msg[] = "SLM-PAM: setuid failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            _exit(126);
        }
        if (!homeDirStd.empty()) chdir(homeDirStd.c_str());
    });

    connect(m_proc, &QProcess::finished,
            this, &SlmPamSession::onProcessFinished);

    qInfo("SLM-PAM: launching session: %s %s  (uid=%u gid=%u session_vt=%d)",
          qPrintable(fullCmd.first()),
          qPrintable(QStringList(fullCmd.constBegin() + 1, fullCmd.constEnd()).join(QLatin1Char(' '))),
          uid, gid, m_sessionVT);

    m_proc->start();
    if (!m_proc->waitForStarted(5000)) {
        qWarning("SLM-PAM: failed to start session: %s",
                 qPrintable(m_proc->errorString()));
        delete m_proc;
        m_proc = nullptr;
        return -1;
    }

    m_sessionPid = m_proc->processId();
    qInfo("SLM-PAM: session process started (pid=%lld session_vt=%d)",
          (long long)m_sessionPid, m_sessionVT);

    // Switch display to the session VT.
    if (m_sessionVT > 0 && m_sessionVT != m_greeterVT)
        switchToVT(m_sessionVT);

    return m_sessionPid;
}

// ── Phase 4: closeSession ─────────────────────────────────────────────────────

void SlmPamSession::closeSession()
{
    if (m_proc) {
        if (m_proc->state() == QProcess::Running) {
            m_proc->terminate();
            if (!m_proc->waitForFinished(5000))
                m_proc->kill();
        }
        delete m_proc;
        m_proc = nullptr;
    }
    m_sessionPid = -1;

    if (!m_pamh) return;

    if (m_sessionOpen) {
        const int ret = pam_close_session(m_pamh, 0);
        if (ret != PAM_SUCCESS)
            qWarning("SLM-PAM: pam_close_session: %s", pam_strerror(m_pamh, ret));
        else
            qInfo("SLM-PAM: pam_close_session success");
        m_sessionOpen = false;
    }

    pam_setcred(m_pamh, PAM_DELETE_CRED);
    pam_end(m_pamh, PAM_SUCCESS);
    m_pamh = nullptr;

    qInfo("SLM-PAM: PAM session closed for user='%s'", qPrintable(m_username));
}

// ── Slot ──────────────────────────────────────────────────────────────────────

void SlmPamSession::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool crashed = (status == QProcess::CrashExit);
    qInfo("SLM-PAM: session process finished (exit_code=%d crashed=%s)",
          exitCode, crashed ? "yes" : "no");

    closeSession();

    // Switch back to greeter VT so the login screen is visible again.
    if (m_greeterVT > 0)
        switchToVT(m_greeterVT);

    emit sessionFinished(exitCode);
}

} // namespace Slm::Login
