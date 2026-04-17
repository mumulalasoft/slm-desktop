#include "firewallservice.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("slm-firewalld"));

    FirewallService service;
    QString error;
    if (!service.start(&error)) {
        qCritical().noquote() << "[slm-firewalld] failed to start:" << error;
        return 1;
    }

    qInfo().noquote() << "[slm-firewalld] serviceRegistered=" << service.serviceRegistered();
    return app.exec();
}
