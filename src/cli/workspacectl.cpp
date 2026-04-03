#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QVariantMap>

namespace {
constexpr const char *kService = "org.slm.WorkspaceManager";
constexpr const char *kPath = "/org/slm/WorkspaceManager";
constexpr const char *kIface = "org.slm.WorkspaceManager1";
constexpr const char *kLegacyService = "org.desktop_shell.WorkspaceManager";
constexpr const char *kLegacyPath = "/org/desktop_shell/WorkspaceManager";
constexpr const char *kLegacyIface = "org.desktop_shell.WorkspaceManager1";

void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  workspacectl --help\n";
    out << "  workspacectl healthcheck [--pretty]\n";
    out << "  workspacectl workspace [on|off|toggle]\n";
    out << "  workspacectl overview [on|off|toggle]   (legacy alias)\n";
    out << "  workspacectl appgrid\n";
    out << "  workspacectl desktop\n";
    out << "  workspacectl present <app_id>\n";
    out << "  workspacectl switch <index>\n";
    out << "  workspacectl switch-delta <delta>\n";
    out << "  workspacectl move <window_or_viewid_or_appid> <index>\n";
    out << "  workspacectl move-focused-delta <delta>\n";
    out << "  workspacectl ranked-apps [limit]\n";
}

bool hasArg(const QStringList &args, const QString &name)
{
    return args.contains(name);
}

bool serviceRegistered(const QString &name)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return false;
    }
    QDBusReply<bool> reply = iface->isServiceRegistered(name);
    return reply.isValid() && reply.value();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QStringList args = app.arguments();

    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const QString cmd = args.at(1).trimmed().toLower();
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        printUsage();
        return 0;
    }

    if (cmd == "healthcheck") {
        const bool pretty = hasArg(args, QStringLiteral("--pretty"));
        const bool primaryUp = serviceRegistered(QString::fromLatin1(kService));
        const bool legacyUp = serviceRegistered(QString::fromLatin1(kLegacyService));

        QVariantMap primaryDiag;
        QVariantMap legacyDiag;
        {
            QDBusInterface iface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIface),
                                 QDBusConnection::sessionBus());
            if (iface.isValid()) {
                QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("DiagnosticSnapshot"));
                if (reply.isValid()) {
                    primaryDiag = reply.value();
                }
            }
        }
        {
            QDBusInterface iface(QString::fromLatin1(kLegacyService),
                                 QString::fromLatin1(kLegacyPath),
                                 QString::fromLatin1(kLegacyIface),
                                 QDBusConnection::sessionBus());
            if (iface.isValid()) {
                QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("DiagnosticSnapshot"));
                if (reply.isValid()) {
                    legacyDiag = reply.value();
                }
            }
        }

        QJsonObject root;
        root.insert(QStringLiteral("primary"), QJsonObject{
                                                 {QStringLiteral("service"), QString::fromLatin1(kService)},
                                                 {QStringLiteral("path"), QString::fromLatin1(kPath)},
                                                 {QStringLiteral("interface"), QString::fromLatin1(kIface)},
                                                 {QStringLiteral("registered"), primaryUp},
                                                 {QStringLiteral("diagnostic"), QJsonObject::fromVariantMap(primaryDiag)},
                                             });
        root.insert(QStringLiteral("legacy"), QJsonObject{
                                                {QStringLiteral("service"), QString::fromLatin1(kLegacyService)},
                                                {QStringLiteral("path"), QString::fromLatin1(kLegacyPath)},
                                                {QStringLiteral("interface"), QString::fromLatin1(kLegacyIface)},
                                                {QStringLiteral("registered"), legacyUp},
                                                {QStringLiteral("diagnostic"), QJsonObject::fromVariantMap(legacyDiag)},
                                            });
        root.insert(QStringLiteral("ok"), primaryUp || legacyUp);

        const QJsonDocument doc(root);
        out << (pretty ? QString::fromUtf8(doc.toJson(QJsonDocument::Indented))
                       : QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        if (!pretty) {
            out << '\n';
        }
        return (primaryUp || legacyUp) ? 0 : 2;
    }

    const bool usingLegacy = !serviceRegistered(QString::fromLatin1(kService));
    const char *targetService = usingLegacy ? kLegacyService : kService;
    const char *targetPath = usingLegacy ? kLegacyPath : kPath;
    const char *targetIface = usingLegacy ? kLegacyIface : kIface;

    QDBusInterface iface(QString::fromLatin1(targetService),
                         QString::fromLatin1(targetPath),
                         QString::fromLatin1(targetIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        err << "Workspace service unavailable: " << kService
            << " (legacy fallback " << kLegacyService << ")\n";
        return 2;
    }

    if (cmd == "overview" || cmd == "workspace") {
        const QString mode = (args.size() >= 3)
                                 ? args.at(2).trimmed().toLower()
                                 : QStringLiteral("on");
        // Canonical path is workspace methods. Keep overview as command alias.
        const QString showMethod = QStringLiteral("ShowWorkspace");
        const QString hideMethod = QStringLiteral("HideWorkspace");
        const QString toggleMethod = QStringLiteral("ToggleWorkspace");
        QDBusReply<void> reply;
        if (mode.isEmpty() || mode == QStringLiteral("on") || mode == QStringLiteral("show")) {
            reply = iface.call(showMethod);
            if (!reply.isValid()) {
                reply = iface.call(QStringLiteral("ShowOverview"));
            }
        } else if (mode == QStringLiteral("off") || mode == QStringLiteral("hide")) {
            reply = iface.call(hideMethod);
            if (!reply.isValid()) {
                reply = iface.call(QStringLiteral("HideOverview"));
            }
        } else if (mode == QStringLiteral("toggle")) {
            reply = iface.call(toggleMethod);
            if (!reply.isValid()) {
                reply = iface.call(QStringLiteral("ToggleOverview"));
            }
        } else {
            err << "invalid " << cmd << " mode: " << mode << '\n';
            printUsage();
            return 1;
        }
        return reply.isValid() ? 0 : 3;
    }
    if (cmd == "appgrid") {
        QDBusReply<void> reply = iface.call(QStringLiteral("ShowAppGrid"));
        return reply.isValid() ? 0 : 3;
    }
    if (cmd == "desktop") {
        QDBusReply<void> reply = iface.call(QStringLiteral("ShowDesktop"));
        return reply.isValid() ? 0 : 3;
    }
    if (cmd == "present") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        QDBusReply<bool> reply = iface.call(QStringLiteral("PresentWindow"), args.at(2));
        if (!reply.isValid()) {
            return 3;
        }
        out << (reply.value() ? "ok" : "not-found") << '\n';
        return reply.value() ? 0 : 4;
    }
    if (cmd == "switch") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        bool ok = false;
        const int index = args.at(2).toInt(&ok);
        if (!ok) {
            err << "invalid index\n";
            return 1;
        }
        QDBusReply<void> reply = iface.call(QStringLiteral("SwitchWorkspace"), index);
        return reply.isValid() ? 0 : 3;
    }
    if (cmd == "switch-delta") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        bool ok = false;
        const int delta = args.at(2).toInt(&ok);
        if (!ok) {
            err << "invalid delta\n";
            return 1;
        }
        QDBusReply<void> reply = iface.call(QStringLiteral("SwitchWorkspaceByDelta"), delta);
        return reply.isValid() ? 0 : 3;
    }
    if (cmd == "move") {
        if (args.size() < 4) {
            printUsage();
            return 1;
        }
        bool ok = false;
        const int index = args.at(3).toInt(&ok);
        if (!ok) {
            err << "invalid index\n";
            return 1;
        }
        QDBusReply<bool> reply = iface.call(QStringLiteral("MoveWindowToWorkspace"),
                                            QVariant(args.at(2)), index);
        if (!reply.isValid()) {
            return 3;
        }
        out << (reply.value() ? "ok" : "failed") << '\n';
        return reply.value() ? 0 : 4;
    }
    if (cmd == "move-focused-delta") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        bool ok = false;
        const int delta = args.at(2).toInt(&ok);
        if (!ok) {
            err << "invalid delta\n";
            return 1;
        }
        QDBusReply<bool> reply = iface.call(QStringLiteral("MoveFocusedWindowByDelta"), delta);
        if (!reply.isValid()) {
            return 3;
        }
        out << (reply.value() ? "ok" : "failed") << '\n';
        return reply.value() ? 0 : 4;
    }
    if (cmd == "ranked-apps") {
        int limit = 24;
        if (args.size() >= 3) {
            bool ok = false;
            const int parsed = args.at(2).toInt(&ok);
            if (!ok || parsed <= 0) {
                err << "invalid limit\n";
                return 1;
            }
            limit = parsed;
        }
        QDBusReply<QVariantList> reply = iface.call(QStringLiteral("ListRankedApps"), limit);
        if (!reply.isValid()) {
            return 3;
        }
        QJsonArray arr;
        const QVariantList rows = reply.value();
        for (const QVariant &row : rows) {
            arr.push_back(QJsonObject::fromVariantMap(row.toMap()));
        }
        const QJsonDocument doc(arr);
        out << QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        return 0;
    }

    err << "unknown command: " << cmd;
    if (usingLegacy) {
        err << " (legacy iface)";
    }
    err << '\n';
    printUsage();
    return 1;
}
