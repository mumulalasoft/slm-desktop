#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTextStream>

namespace {
constexpr const char kService[] = "org.slm.Actiond";
constexpr const char kPath[] = "/org/slm/Actiond";
constexpr const char kProviderIface[] = "org.slm.Actiond.Provider";
constexpr const char kFrontendIface[] = "org.slm.Actiond.Frontend";

void printJson(const QVariant &v)
{
    QTextStream out(stdout);
    out << QJsonDocument::fromVariant(v).toJson(QJsonDocument::Indented);
}

void usage()
{
    QTextStream out(stdout);
    out << "Usage: slm-actionctl <command>\n"
        << "  providers\n"
        << "  contexts\n"
        << "  active-context\n"
        << "  actions [context_id]\n"
        << "  menu [context_id]\n"
        << "  search <query>\n"
        << "  quick-actions <app_id>\n"
        << "  invoke <action_id> [context_id] [--gesture token]\n";
}

class WatchPrinter : public QObject
{
    Q_OBJECT
public slots:
    void onActiveContextChanged(const QVariantMap &context)
    {
        QTextStream(stdout) << "ActiveContextChanged\n";
        printJson(context);
    }

    void onActionsChanged(const QString &contextId, const QVariantList &actions)
    {
        QTextStream(stdout) << "ActionsChanged: " << contextId << "\n";
        printJson(actions);
    }

    void onMenuModelChanged(const QString &contextId, const QVariantList &menu)
    {
        QTextStream(stdout) << "MenuModelChanged: " << contextId << "\n";
        printJson(menu);
    }
};
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2) {
        usage();
        return 1;
    }

    QDBusInterface frontend(QString::fromLatin1(kService),
                            QString::fromLatin1(kPath),
                            QString::fromLatin1(kFrontendIface),
                            QDBusConnection::sessionBus());

    const QString cmd = args.at(1);
    if (cmd == QLatin1String("active-context")) {
        printJson(frontend.call(QStringLiteral("GetActiveContext")).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("contexts")) {
        printJson(frontend.call(QStringLiteral("GetContexts")).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("providers")) {
        printJson(frontend.call(QStringLiteral("GetProviders")).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("actions")) {
        const QString contextId = args.size() > 2 ? args.at(2) : QString();
        printJson(frontend.call(QStringLiteral("GetActions"), contextId).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("menu")) {
        const QString contextId = args.size() > 2 ? args.at(2) : QString();
        printJson(frontend.call(QStringLiteral("GetMenuModel"), contextId).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("search") && args.size() > 2) {
        printJson(frontend.call(QStringLiteral("SearchActions"), args.at(2), QVariantMap{}).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("quick-actions") && args.size() > 2) {
        printJson(frontend.call(QStringLiteral("GetQuickActions"), args.at(2)).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("invoke") && args.size() > 2) {
        const QString actionId = args.at(2);
        const QString contextId = (args.size() > 3 && !args.at(3).startsWith(QLatin1String("--"))) ? args.at(3) : QString();
        QVariantMap params;
        const int gestureIx = args.indexOf(QStringLiteral("--gesture"));
        if (gestureIx >= 0 && gestureIx + 1 < args.size()) {
            params.insert(QStringLiteral("user_gesture_token"), args.at(gestureIx + 1));
        }
        printJson(frontend.call(QStringLiteral("InvokeAction"), actionId, contextId, params).arguments().value(0));
        return 0;
    }
    if (cmd == QLatin1String("watch")) {
        WatchPrinter printer;
        auto bus = QDBusConnection::sessionBus();
        bus.connect(QString::fromLatin1(kService),
                    QString::fromLatin1(kPath),
                    QString::fromLatin1(kFrontendIface),
                    QStringLiteral("ActiveContextChanged"),
                    &printer,
                    SLOT(onActiveContextChanged(QVariantMap)));
        bus.connect(QString::fromLatin1(kService),
                    QString::fromLatin1(kPath),
                    QString::fromLatin1(kFrontendIface),
                    QStringLiteral("ActionsChanged"),
                    &printer,
                    SLOT(onActionsChanged(QString,QVariantList)));
        bus.connect(QString::fromLatin1(kService),
                    QString::fromLatin1(kPath),
                    QString::fromLatin1(kFrontendIface),
                    QStringLiteral("MenuModelChanged"),
                    &printer,
                    SLOT(onMenuModelChanged(QString,QVariantList)));
        return app.exec();
    }

    usage();
    return 1;
}

#include "slm-actionctl.moc"
