#include "src/services/secret/secretservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>

namespace {
constexpr const char kServiceName[] = "org.slm.Secret1";
constexpr const char kServicePath[] = "/org/slm/Secret1";
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("slm-secretd"));

    Slm::Secret::SecretService service;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        fprintf(stderr,
                "slm-secretd: failed to register service %s: %s\n",
                kServiceName,
                qPrintable(bus.lastError().message()));
        return 2;
    }
    if (!bus.registerObject(QString::fromLatin1(kServicePath),
                            &service,
                            QDBusConnection::ExportAllSlots)) {
        fprintf(stderr,
                "slm-secretd: failed to register object %s: %s\n",
                kServicePath,
                qPrintable(bus.lastError().message()));
        return 2;
    }
    return app.exec();
}

