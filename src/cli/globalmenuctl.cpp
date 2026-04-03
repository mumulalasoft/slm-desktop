#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QVariantMap>

namespace {
constexpr const char *kCanonicalRegistrarService = "com.canonical.AppMenu.Registrar";
constexpr const char *kCanonicalRegistrarPath = "/com/canonical/AppMenu/Registrar";
constexpr const char *kCanonicalRegistrarIface = "com.canonical.AppMenu.Registrar";
constexpr const char *kKdeRegistrarService = "org.kde.kappmenu";
constexpr const char *kKdeRegistrarPath = "/KAppMenu";
constexpr const char *kKdeRegistrarIface = "org.kde.kappmenu";
constexpr const char *kMenuIface = "com.canonical.dbusmenu";
constexpr const char *kSlmMenuService = "org.slm.Desktop.GlobalMenu";
constexpr const char *kSlmMenuPath = "/org/slm/Desktop/GlobalMenu";
constexpr const char *kSlmMenuIface = "org.slm.Desktop.GlobalMenu";

struct DbusMenuNode {
    int id = 0;
    QVariantMap properties;
    QList<DbusMenuNode> children;
};

const QDBusArgument &operator>>(const QDBusArgument &argument, DbusMenuNode &node)
{
    node = DbusMenuNode();
    argument.beginStructure();
    argument >> node.id;
    argument >> node.properties;
    argument.beginArray();
    while (!argument.atEnd()) {
        DbusMenuNode child;
        argument >> child;
        node.children.push_back(child);
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  globalmenuctl --help\n";
    out << "  globalmenuctl healthcheck [--pretty]\n";
    out << "  globalmenuctl dump [--pretty]\n";
}

bool hasArg(const QStringList &args, const QString &name)
{
    return args.contains(name);
}

bool serviceRegistered(const QString &name)
{
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    if (!iface) {
        return false;
    }
    QDBusReply<bool> reply = iface->isServiceRegistered(name);
    return reply.isValid() && reply.value();
}

quint32 detectActiveWindowId()
{
    const QString xpropPath = QStandardPaths::findExecutable(QStringLiteral("xprop"));
    if (xpropPath.isEmpty()) {
        return 0;
    }
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(xpropPath, QStringList{QStringLiteral("-root"), QStringLiteral("_NET_ACTIVE_WINDOW")});
    if (!proc.waitForFinished(220) || proc.exitCode() != 0) {
        return 0;
    }
    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QRegularExpression hexRe(QStringLiteral("0x([0-9a-fA-F]+)"));
    const QRegularExpressionMatch match = hexRe.match(output);
    if (!match.hasMatch()) {
        return 0;
    }
    bool ok = false;
    const quint32 id = match.captured(1).toUInt(&ok, 16);
    return ok ? id : 0;
}

bool resolveMenuForWindowViaRegistrar(const QString &registrarService,
                                      const QString &registrarPath,
                                      const QString &registrarIface,
                                      const QString &method,
                                      quint32 windowId,
                                      QString *service,
                                      QString *path)
{
    if (!service || !path || windowId == 0) {
        return false;
    }

    QDBusInterface registrar(registrarService,
                             registrarPath,
                             registrarIface,
                             QDBusConnection::sessionBus());
    if (!registrar.isValid()) {
        return false;
    }

    QDBusMessage reply = registrar.call(method, QVariant::fromValue(static_cast<uint>(windowId)));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return false;
    }
    const QList<QVariant> args = reply.arguments();
    if (args.size() < 2) {
        return false;
    }
    *service = args.at(0).toString().trimmed();
    const QVariant pathVar = args.at(1);
    if (pathVar.canConvert<QDBusObjectPath>()) {
        *path = pathVar.value<QDBusObjectPath>().path().trimmed();
    } else {
        *path = pathVar.toString().trimmed();
    }
    return !service->isEmpty() && !path->isEmpty();
}

QVariantList queryTopLevelMenus(const QString &service, const QString &path)
{
    QVariantList out;
    QDBusInterface menuIface(service,
                             path,
                             QString::fromLatin1(kMenuIface),
                             QDBusConnection::sessionBus());
    if (!menuIface.isValid()) {
        return out;
    }

    QDBusMessage reply = menuIface.call(QStringLiteral("GetLayout"),
                                        0,
                                        1,
                                        QStringList{QStringLiteral("label"),
                                                    QStringLiteral("visible"),
                                                    QStringLiteral("enabled")});
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return out;
    }
    const QList<QVariant> args = reply.arguments();
    if (args.size() < 2 || !args.at(1).canConvert<QDBusArgument>()) {
        return out;
    }
    DbusMenuNode root;
    const QDBusArgument dbusArg = args.at(1).value<QDBusArgument>();
    dbusArg >> root;
    for (const DbusMenuNode &child : std::as_const(root.children)) {
        const bool visible = child.properties.value(QStringLiteral("visible"), true).toBool();
        if (!visible) {
            continue;
        }
        QString label = child.properties.value(QStringLiteral("label")).toString().trimmed();
        label.remove(QLatin1Char('_'));
        label.remove(QLatin1Char('&'));
        if (label.isEmpty()) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), child.id);
        row.insert(QStringLiteral("label"), label);
        row.insert(QStringLiteral("enabled"),
                   child.properties.value(QStringLiteral("enabled"), true).toBool());
        out << row;
    }
    return out;
}

QVariantMap slmFallbackSnapshot()
{
    QVariantMap out;
    QDBusInterface iface(QString::fromLatin1(kSlmMenuService),
                         QString::fromLatin1(kSlmMenuPath),
                         QString::fromLatin1(kSlmMenuIface),
                         QDBusConnection::sessionBus());
    out.insert(QStringLiteral("registered"), iface.isValid());
    if (!iface.isValid()) {
        return out;
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("DiagnosticSnapshot"));
    if (reply.isValid()) {
        out.insert(QStringLiteral("diagnostic"), reply.value());
    }
    return out;
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

    const bool pretty = hasArg(args, QStringLiteral("--pretty"));
    const bool canonicalUp = serviceRegistered(QString::fromLatin1(kCanonicalRegistrarService));
    const bool kdeUp = serviceRegistered(QString::fromLatin1(kKdeRegistrarService));
    const quint32 activeWindow = detectActiveWindowId();

    QString menuService;
    QString menuPath;
    QString resolver;
    if (activeWindow != 0
        && resolveMenuForWindowViaRegistrar(QString::fromLatin1(kCanonicalRegistrarService),
                                            QString::fromLatin1(kCanonicalRegistrarPath),
                                            QString::fromLatin1(kCanonicalRegistrarIface),
                                            QStringLiteral("GetMenuForWindow"),
                                            activeWindow,
                                            &menuService,
                                            &menuPath)) {
        resolver = QStringLiteral("canonical");
    } else if (activeWindow != 0
               && resolveMenuForWindowViaRegistrar(QString::fromLatin1(kKdeRegistrarService),
                                                   QString::fromLatin1(kKdeRegistrarPath),
                                                   QString::fromLatin1(kKdeRegistrarIface),
                                                   QStringLiteral("getMenuForWindow"),
                                                   activeWindow,
                                                   &menuService,
                                                   &menuPath)) {
        resolver = QStringLiteral("kde-lowercase");
    } else if (activeWindow != 0
               && resolveMenuForWindowViaRegistrar(QString::fromLatin1(kKdeRegistrarService),
                                                   QString::fromLatin1(kKdeRegistrarPath),
                                                   QString::fromLatin1(kKdeRegistrarIface),
                                                   QStringLiteral("GetMenuForWindow"),
                                                   activeWindow,
                                                   &menuService,
                                                   &menuPath)) {
        resolver = QStringLiteral("kde");
    }

    QVariantList topLevel;
    if (!menuService.isEmpty() && !menuPath.isEmpty()) {
        topLevel = queryTopLevelMenus(menuService, menuPath);
    }
    const QVariantMap slmFallback = slmFallbackSnapshot();

    QJsonObject root;
    root.insert(QStringLiteral("registrars"),
                QJsonObject{
                    {QStringLiteral("canonical"), canonicalUp},
                    {QStringLiteral("kde"), kdeUp},
                });
    root.insert(QStringLiteral("activeWindowId"), static_cast<qint64>(activeWindow));
    root.insert(QStringLiteral("activeMenuBinding"),
                QJsonObject{
                    {QStringLiteral("resolver"), resolver},
                    {QStringLiteral("service"), menuService},
                    {QStringLiteral("path"), menuPath},
                    {QStringLiteral("valid"), !menuService.isEmpty() && !menuPath.isEmpty()},
                });
    root.insert(QStringLiteral("topLevelMenus"),
                QJsonArray::fromVariantList(topLevel));
    root.insert(QStringLiteral("slmFallback"),
                QJsonObject::fromVariantMap(slmFallback));

    if (cmd == "healthcheck") {
        root.insert(QStringLiteral("ok"),
                    canonicalUp || kdeUp || slmFallback.value(QStringLiteral("registered")).toBool());
    } else if (cmd != "dump") {
        err << "unknown command: " << cmd << '\n';
        printUsage();
        return 1;
    }

    const QJsonDocument doc(root);
    out << (pretty ? QString::fromUtf8(doc.toJson(QJsonDocument::Indented))
                   : QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    if (!pretty) {
        out << '\n';
    }

    if (cmd == "healthcheck") {
        return root.value(QStringLiteral("ok")).toBool() ? 0 : 2;
    }
    return 0;
}
