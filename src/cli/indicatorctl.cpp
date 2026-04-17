#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QTextStream>
#include <QVariantList>

#include "../../resourcepaths.h"

namespace {
constexpr const char *kService = "org.slm.IndicatorService";
constexpr const char *kPath = "/org/slm/IndicatorRegistry";
constexpr const char *kIface = "org.slm.IndicatorRegistry";
constexpr const char *kLegacyService = "org.desktop_shell.IndicatorService";
constexpr const char *kLegacyPath = "/org/desktop_shell/IndicatorRegistry";
constexpr const char *kLegacyIface = "org.desktop_shell.IndicatorRegistry";

void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  indicatorctl --help\n";
    out << "  indicatorctl man\n";
    out << "  indicatorctl list\n";
    out << "  indicatorctl clear\n";
    out << "  indicatorctl unregister <id>\n";
    out << "  indicatorctl register <name> <source> [order] [visible] [enabled]\n";
}

void printMan()
{
    QTextStream out(stdout);
    out << "indicatorctl - manage SLM Desktop external indicators via DBus\n\n";
    out << "SYNOPSIS\n";
    out << "  indicatorctl list\n";
    out << "  indicatorctl clear\n";
    out << "  indicatorctl unregister <id>\n";
    out << "  indicatorctl register <name> <source> [order] [visible] [enabled]\n\n";
    out << "DESCRIPTION\n";
    out << "  Talks to org.slm.IndicatorService at\n";
    out << "  /org/slm/IndicatorRegistry (interface org.slm.IndicatorRegistry).\n\n";
    out << "COMMANDS\n";
    out << "  list\n";
    out << "      Print all registered external indicators.\n";
    out << "  clear\n";
    out << "      Remove all external indicators.\n";
    out << "  unregister <id>\n";
    out << "      Remove one indicator by id.\n";
    out << "  register <name> <source> [order] [visible] [enabled]\n";
    out << "      Register indicator using RegisterIndicatorSimple.\n\n";
    out << "EXAMPLES\n";
    out << "  indicatorctl register vpn " << ResourcePaths::Qml::indicatorExampleQml() << " 900 true true\n";
    out << "  indicatorctl list\n";
    out << "  indicatorctl unregister 1\n";
}

bool parseBool(const QString &raw, bool fallback)
{
    const QString v = raw.trimmed().toLower();
    if (v == "1" || v == "true" || v == "yes" || v == "on") {
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "off") {
        return false;
    }
    return fallback;
}

QDBusInterface makeIface()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *connIface = bus.interface();
    const bool primaryAvailable =
        connIface && connIface->isServiceRegistered(QString::fromLatin1(kService)).value();
    if (primaryAvailable) {
        return QDBusInterface(QString::fromLatin1(kService),
                              QString::fromLatin1(kPath),
                              QString::fromLatin1(kIface),
                              bus);
    }
    return QDBusInterface(QString::fromLatin1(kLegacyService),
                          QString::fromLatin1(kLegacyPath),
                          QString::fromLatin1(kLegacyIface),
                          bus);
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const QString cmd = args.at(1).trimmed().toLower();
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        printUsage();
        return 0;
    }
    if (cmd == "man") {
        printMan();
        return 0;
    }

    QDBusInterface iface = makeIface();
    if (!iface.isValid()) {
        err << "DBus service unavailable: " << kService
            << " (legacy fallback " << kLegacyService << ")\n";
        return 2;
    }

    if (cmd == "list") {
        QDBusReply<QVariantList> reply = iface.call(QStringLiteral("ListIndicators"));
        if (!reply.isValid()) {
            err << "list failed\n";
            return 3;
        }
        const QVariantList list = reply.value();
        for (const QVariant &itemVar : list) {
            const QVariantMap item = itemVar.toMap();
            out << "id=" << item.value("id").toString()
                << " name=" << item.value("name").toString()
                << " source=" << item.value("source").toString()
                << " order=" << item.value("order").toString()
                << " visible=" << item.value("visible").toString()
                << " enabled=" << item.value("enabled").toString()
                << '\n';
        }
        return 0;
    }

    if (cmd == "clear") {
        QDBusReply<void> reply = iface.call(QStringLiteral("ClearIndicators"));
        if (!reply.isValid()) {
            err << "clear failed\n";
            return 3;
        }
        out << "ok\n";
        return 0;
    }

    if (cmd == "unregister") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        bool ok = false;
        const int id = args.at(2).toInt(&ok);
        if (!ok) {
            err << "invalid id\n";
            return 1;
        }
        QDBusReply<bool> reply = iface.call(QStringLiteral("UnregisterIndicator"), id);
        if (!reply.isValid()) {
            err << "unregister failed\n";
            return 3;
        }
        out << (reply.value() ? "ok" : "not-found") << '\n';
        return 0;
    }

    if (cmd == "register") {
        if (args.size() < 4) {
            printUsage();
            return 1;
        }
        const QString name = args.at(2);
        const QString source = args.at(3);
        const int order = args.size() >= 5 ? args.at(4).toInt() : 1000;
        const bool visible = args.size() >= 6 ? parseBool(args.at(5), true) : true;
        const bool enabled = args.size() >= 7 ? parseBool(args.at(6), true) : true;

        QDBusReply<int> reply = iface.call(QStringLiteral("RegisterIndicatorSimple"),
                                           name, source, order, visible, enabled);
        if (!reply.isValid()) {
            err << "register failed\n";
            return 3;
        }
        out << reply.value() << '\n';
        return 0;
    }

    err << "unknown command: " << cmd << '\n';
    printUsage();
    return 1;
}
