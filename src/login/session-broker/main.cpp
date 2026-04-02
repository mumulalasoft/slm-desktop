#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include "sessionbroker.h"

using namespace Slm::Login;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("slm-session-broker"));
    app.setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "SLM Desktop session broker — validates state, selects startup mode, "
        "launches compositor and shell."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption modeOpt(
        QStringList{QStringLiteral("m"), QStringLiteral("mode")},
        QStringLiteral("Startup mode: normal|safe|recovery"),
        QStringLiteral("mode"),
        QStringLiteral("normal"));
    parser.addOption(modeOpt);
    parser.process(app);

    const StartupMode mode = startupModeFromString(parser.value(modeOpt));

    SessionBroker broker(mode);
    // run() is synchronous — it blocks until the compositor exits.
    // We do not call app.exec(); the event loop is not needed here.
    return broker.run();
}
