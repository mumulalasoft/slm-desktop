#include <QCoreApplication>
#include "sessionwatchdog.h"

using namespace Slm::Login;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("slm-watchdog"));

    SessionWatchdog watchdog;
    return app.exec();
}
