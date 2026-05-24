#include "shellliveness.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QTimer>

namespace {
constexpr const char kSessionStateService[]   = "org.slm.SessionState";
constexpr const char kSessionStatePath[]      = "/org/slm/SessionState";
constexpr const char kSessionStateInterface[] = "org.slm.SessionState1";
constexpr const char kDefaultShellName[]      = "org.slm.Desktop";
}

// slm-lockd — minimal lockscreen supervisor.
//
// Watches the shell process via its well-known DBus name. When the shell
// disappears, calls org.slm.SessionState.ForceLocked so desktopd's
// authoritative lock state remains Locked even though no UI is visible.
// This guarantees that the screen-reentry path on shell respawn requires
// a fresh PAM unlock; the daemon never silently goes back to Active just
// because the UI vanished.
//
// What this binary does NOT do (yet): host its own emergency LayerShell
// lock UI. That is a follow-up — see docs/SESSION_STATE.md. The current
// failsafe is state-only: shell crash => Locked stays Locked.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("slm-lockd"));
    app.setOrganizationName(QStringLiteral("SLM"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("SLM lockscreen supervisor — forces Locked state when the shell disappears."));
    parser.addHelpOption();
    QCommandLineOption shellNameOpt(
        QStringLiteral("shell-name"),
        QStringLiteral("Well-known DBus name of the shell process to watch."),
        QStringLiteral("name"),
        QString::fromLatin1(kDefaultShellName));
    parser.addOption(shellNameOpt);
    parser.process(app);

    const QString shellName = parser.value(shellNameOpt);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "[LOCKD] [SUPERVISOR] cannot connect to DBus session bus";
        return 1;
    }

    qInfo().noquote() << "[LOCKD] [SUPERVISOR] starting watch shell-name=" << shellName;

    Slm::Lockd::ShellLiveness liveness(shellName);

    auto callForceLocked = [shellName](const QString &reason) {
        QDBusInterface iface(QString::fromLatin1(kSessionStateService),
                             QString::fromLatin1(kSessionStatePath),
                             QString::fromLatin1(kSessionStateInterface),
                             QDBusConnection::sessionBus());
        if (!iface.isValid()) {
            qWarning().noquote() << "[LOCKD] [SUPERVISOR] cannot reach"
                                 << kSessionStateService
                                 << "to ForceLocked reason=" << reason;
            return;
        }
        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("ForceLocked"), reason);
        if (!reply.isValid()) {
            qWarning().noquote() << "[LOCKD] [SUPERVISOR] ForceLocked call failed:"
                                 << reply.error().message();
            return;
        }
        const QVariantMap result = reply.value();
        qInfo().noquote() << "[LOCKD] [SUPERVISOR] ForceLocked applied reason="
                          << reason
                          << "lock_state=" << result.value(QStringLiteral("lock_state")).toString()
                          << "noop=" << result.value(QStringLiteral("noop")).toBool();
    };

    QObject::connect(&liveness, &Slm::Lockd::ShellLiveness::shellLost,
                     &app, callForceLocked);

    QObject::connect(&liveness, &Slm::Lockd::ShellLiveness::shellReturned,
                     &app, []() {
        qInfo().noquote() << "[LOCKD] [SUPERVISOR] shell returned; daemon lock state preserved"
                          << "(unlock requires PAM via SessionState.Unlock)";
    });

    qInfo().noquote() << "[LOCKD] [SUPERVISOR] running shell-alive="
                      << liveness.shellAlive()
                      << "started_at=" << QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    return app.exec();
}
