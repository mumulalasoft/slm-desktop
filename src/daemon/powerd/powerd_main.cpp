#include "../../services/power/powerbridge.h"
#include "../../services/power/powercontroller.h"
#include "../../services/power/schedulecontroller.h"
#include "../../services/power/sessioncontroller.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("slm-powerd"));

    PowerBridge powerBridge;
    SessionController sessionController;
    ScheduleController scheduleController;
    PowerController powerController(&powerBridge,
                                    &sessionController,
                                    &scheduleController,
                                    nullptr);

    QObject::connect(&scheduleController,
                     &ScheduleController::executeScheduledActionRequested,
                     &powerController,
                     [&powerController](const QString &action) {
        powerController.requestNow(action);
    });

    qInfo().noquote() << "[slm-powerd] started; pending schedule="
                      << scheduleController.hasPending()
                      << scheduleController.executeAt().toString(Qt::ISODate);
    return app.exec();
}
