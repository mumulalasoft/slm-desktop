#include <QGuiApplication>
#include <QDebug>

#include "../../services/clipboard/ClipboardService.h"
#include "../../services/clipboard/DBusInterface.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Clipboard Service"));

    Slm::Clipboard::ClipboardService service;
    Slm::Clipboard::DBusInterface dbus(&service);
    const bool ok = service.isReady() && dbus.registerService();
    qInfo().noquote() << "[clipboardd] ready=" << service.isReady()
                      << "dbusRegistered=" << ok;

    return app.exec();
}
