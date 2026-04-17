#include "recoveryservice.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("slm-recoveryd"));

    RecoveryService service;
    QString error;
    if (!service.start(&error)) {
        qCritical().noquote() << "[slm-recoveryd] failed to start:" << error;
        return 1;
    }

    qInfo().noquote() << "[slm-recoveryd] serviceRegistered=" << service.serviceRegistered();
    return app.exec();
}
