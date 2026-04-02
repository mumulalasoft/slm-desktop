#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QVariantMap>

namespace {
constexpr const char *kService = "org.slm.Desktop.Devices";
constexpr const char *kPath = "/org/slm/Desktop/Devices";
constexpr const char *kIface = "org.slm.Desktop.Devices";
constexpr const char *kLegacyService = "org.desktop_shell.Desktop.Devices";
constexpr const char *kLegacyPath = "/org/desktop_shell/Desktop/Devices";
constexpr const char *kLegacyIface = "org.desktop_shell.Desktop.Devices";

void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  devicectl --help\n";
    out << "  devicectl mount <target>\n";
    out << "  devicectl eject <target>\n";
    out << "  devicectl unlock <target> <passphrase>\n";
    out << "  devicectl format <target> [filesystem]\n";
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

void printResult(const QVariantMap &result, bool pretty)
{
    const QJsonObject obj = QJsonObject::fromVariantMap(result);
    const QJsonDocument doc(obj);
    QTextStream out(stdout);
    out << QString::fromUtf8(doc.toJson(pretty ? QJsonDocument::Indented
                                               : QJsonDocument::Compact));
    if (!pretty) {
        out << '\n';
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
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
    const bool pretty = args.contains(QStringLiteral("--pretty"));
    const bool useLegacy = !serviceRegistered(QString::fromLatin1(kService));
    const QString service = useLegacy ? QString::fromLatin1(kLegacyService)
                                      : QString::fromLatin1(kService);
    const QString path = useLegacy ? QString::fromLatin1(kLegacyPath)
                                   : QString::fromLatin1(kPath);
    const QString ifaceName = useLegacy ? QString::fromLatin1(kLegacyIface)
                                        : QString::fromLatin1(kIface);

    QDBusInterface iface(service, path, ifaceName, QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        err << "Devices service unavailable: " << kService
            << " (legacy fallback " << kLegacyService << ")\n";
        return 2;
    }

    if (cmd == "mount" || cmd == "eject") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        QDBusReply<QVariantMap> reply = iface.call(cmd == "mount"
                                                   ? QStringLiteral("Mount")
                                                   : QStringLiteral("Eject"),
                                                   args.at(2));
        if (!reply.isValid()) {
            return 3;
        }
        printResult(reply.value(), pretty);
        return reply.value().value(QStringLiteral("ok")).toBool() ? 0 : 4;
    }

    if (cmd == "unlock") {
        if (args.size() < 4) {
            printUsage();
            return 1;
        }
        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Unlock"), args.at(2), args.at(3));
        if (!reply.isValid()) {
            return 3;
        }
        printResult(reply.value(), pretty);
        return reply.value().value(QStringLiteral("ok")).toBool() ? 0 : 4;
    }

    if (cmd == "format") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }
        const QString fs = (args.size() >= 4 && !args.at(3).startsWith("--"))
                ? args.at(3) : QString();
        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Format"), args.at(2), fs);
        if (!reply.isValid()) {
            return 3;
        }
        printResult(reply.value(), pretty);
        return reply.value().value(QStringLiteral("ok")).toBool() ? 0 : 4;
    }

    err << "unknown command: " << cmd << '\n';
    printUsage();
    return 1;
}

