#include <QCoreApplication>
#include <QDebug>

#include "../../services/archive/archiveservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Archive Service"));

    Slm::Archive::ArchiveService service;
    const bool ok = service.registerService();
    qInfo().noquote() << "[slm-archived] serviceRegistered=" << ok;

    return app.exec();
}
