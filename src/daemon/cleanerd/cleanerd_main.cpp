#include "src/services/cleaner/cleanerservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>

namespace {
constexpr const char kServiceName[] = "org.slm.Desktop.Cleaner";
constexpr const char kServicePath[] = "/org/slm/Desktop/Cleaner";
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("slm-cleanerd"));

    Slm::Cleaner::CleanerService service;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        fprintf(stderr,
                "slm-cleanerd: failed to register service %s: %s\n",
                kServiceName,
                qPrintable(bus.lastError().message()));
        return 2;
    }
    if (!bus.registerObject(QString::fromLatin1(kServicePath),
                            &service,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        fprintf(stderr,
                "slm-cleanerd: failed to register object %s: %s\n",
                kServicePath,
                qPrintable(bus.lastError().message()));
        return 2;
    }
    return app.exec();
}
