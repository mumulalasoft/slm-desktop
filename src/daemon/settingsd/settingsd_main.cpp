#include "src/services/settingsd/settingsservice.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("slm-settingsd"));

    SettingsService service;
    QString error;
    if (!service.start(&error)) {
        qCritical().noquote() << "[slm-settingsd] failed to start:" << error;
        return 1;
    }

    qInfo().noquote() << "[slm-settingsd] serviceRegistered=" << service.serviceRegistered();
    return app.exec();
}

