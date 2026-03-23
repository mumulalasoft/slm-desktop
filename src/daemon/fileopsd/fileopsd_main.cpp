#include <QCoreApplication>
#include <QDebug>

#include "../../filemanager/ops/fileoperationsmanager.h"
#include "../../filemanager/ops/fileoperationsservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM File Operations Daemon"));

    FileOperationsManager manager;
    FileOperationsService service(&manager);
    qInfo().noquote() << "[slm-fileopsd] serviceRegistered=" << service.serviceRegistered();

    return app.exec();
}
