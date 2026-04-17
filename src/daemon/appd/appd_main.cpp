#include "../../services/appd/appservice.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("desktop-appd"));

    Slm::Appd::AppService service;
    QString error;
    if (!service.start(&error)) {
        qCritical().noquote() << "[desktop-appd] failed to start:" << error;
        return 1;
    }

    qInfo().noquote() << "[desktop-appd] serviceRegistered=" << service.serviceRegistered();
    return app.exec();
}
