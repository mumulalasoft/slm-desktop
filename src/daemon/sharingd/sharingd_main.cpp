#include "sharingmanager.h"
#include "sharingservice.h"
#include "nearbyservice.h"
#include "trustservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>

namespace {
constexpr const char kServiceName[]   = "org.slm.Sharing";
constexpr const char kRootPath[]      = "/org/slm/Sharing";
constexpr const char kNearbyPath[]    = "/org/slm/Sharing/Nearby";
constexpr const char kTrustPath[]     = "/org/slm/Sharing/Trust";
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("slm-sharingd"));
    QCoreApplication::setOrganizationName(QStringLiteral("slm"));

    SharingManager manager;
    if (!manager.initialize()) {
        fprintf(stderr, "slm-sharingd: initialization failed\n");
        return 1;
    }

    SharingService sharingService(&manager);
    NearbyService  nearbyService(&manager);
    TrustService   trustService(&manager);

    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        fprintf(stderr,
                "slm-sharingd: failed to register service %s: %s\n",
                kServiceName,
                qPrintable(bus.lastError().message()));
        return 2;
    }

    const auto registerObj = [&](const char *path, QObject *obj) {
        if (!bus.registerObject(QString::fromLatin1(path), obj,
                                QDBusConnection::ExportAllSlots
                                    | QDBusConnection::ExportAllSignals
                                    | QDBusConnection::ExportAllProperties)) {
            fprintf(stderr,
                    "slm-sharingd: failed to register object %s: %s\n",
                    path,
                    qPrintable(bus.lastError().message()));
            return false;
        }
        return true;
    };

    if (!registerObj(kRootPath,   &sharingService)) return 2;
    if (!registerObj(kNearbyPath, &nearbyService))  return 2;
    if (!registerObj(kTrustPath,  &trustService))   return 2;

    return app.exec();
}
