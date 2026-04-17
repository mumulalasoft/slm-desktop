#include "../../services/context/contextservice.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("desktop-contextd"));

    Slm::Context::ContextService service;
    QString error;
    if (!service.start(&error)) {
        qCritical().noquote() << "[desktop-contextd] failed to start:" << error;
        return 1;
    }

    qInfo().noquote() << "[desktop-contextd] serviceRegistered=" << service.serviceRegistered();
    return app.exec();
}
