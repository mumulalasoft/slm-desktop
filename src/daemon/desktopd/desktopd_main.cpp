#include <QCoreApplication>
#include <QDebug>
#include <memory>

#include "daemonhealthmonitor.h"
#include "../../../appmodel.h"
#include "desktopdaemonservice.h"
#include "../../../globalsearchservice.h"
#include "../../filemanager/ops/fileoperationsmanager.h"
#include "../../filemanager/ops/fileoperationsservice.h"
#include "../../core/workspace/spacesmanager.h"
#include "../../core/workspace/windowingbackendmanager.h"
#include "../../core/prefs/uipreferences.h"
#include "sessionstatecompatservice.h"
#include "sessionstatemanager.h"
#include "sessionstateservice.h"
#include "globalmenuservice.h"
#include "slmcapabilityservice.h"
#include "captureservice.h"
#include "screencastservice.h"
#include "inputcaptureservice.h"
#include "capturestreamingestor.h"
#include "foldersharingservice.h"
#include "../../core/workspace/workspacecompatservice.h"
#include "../../core/workspace/workspacemanager.h"
#include "../../core/actions/slmactionregistry.h"
#include "../../core/actions/framework/slmactionframework.h"
#include "../../core/execution/appexecutiongate.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Desktop Daemon"));

    WindowingBackendManager windowingBackendManager;
    windowingBackendManager.configureBackend(QStringLiteral("kwin-wayland"));
    UIPreferences uiPreferences;
    DesktopAppModel appModel;
    appModel.setUIPreferences(&uiPreferences);
    appModel.refresh();

    SpacesManager spacesManager;
    WorkspaceManager workspaceManager(&windowingBackendManager,
                                      &spacesManager,
                                      windowingBackendManager.compositorStateObject());
    DaemonHealthMonitor daemonHealthMonitor;
    DesktopDaemonService daemonService(&workspaceManager,
                                       &windowingBackendManager,
                                       &spacesManager,
                                       &appModel,
                                       &daemonHealthMonitor);
    GlobalSearchService globalSearchService(&appModel, &workspaceManager, &uiPreferences);
    WorkspaceCompatService legacyCompatService(&workspaceManager,
                                               &windowingBackendManager,
                                               &spacesManager);
    SessionStateManager sessionStateManager(&windowingBackendManager,
                                            windowingBackendManager.compositorStateObject());
    SessionStateService sessionStateService(&sessionStateManager);
    SessionStateCompatService legacySessionStateService(&sessionStateManager);
    GlobalMenuService globalMenuService;
    CaptureService captureService;
    ScreencastService screencastService;
    InputCaptureService inputCaptureService;
    FolderSharingService folderSharingService;
    CaptureStreamIngestor captureStreamIngestor(&captureService);
    Slm::Actions::ActionRegistry actionRegistry;
    AppExecutionGate appExecutionGate(nullptr, nullptr, &uiPreferences);
    actionRegistry.setExecutionGate(&appExecutionGate);
    actionRegistry.reload();
    Slm::Actions::Framework::SlmActionFramework actionFramework(&actionRegistry);
    actionFramework.start();
    SlmCapabilityService capabilityService(&actionFramework);
    const bool embedFileOps = qEnvironmentVariableIsEmpty("SLM_DESKTOPD_DISABLE_FILEOPS");

    std::unique_ptr<FileOperationsManager> fileOperationsManager;
    std::unique_ptr<FileOperationsService> fileOperationsService;
    if (embedFileOps) {
        fileOperationsManager = std::make_unique<FileOperationsManager>();
        fileOperationsService = std::make_unique<FileOperationsService>(fileOperationsManager.get());
    }
    qInfo().noquote() << "[desktopd] serviceRegistered=" << daemonService.serviceRegistered()
                      << "searchRegistered=" << globalSearchService.serviceRegistered()
                      << "legacyServiceRegistered=" << legacyCompatService.serviceRegistered()
                      << "sessionStateRegistered=" << sessionStateService.serviceRegistered()
                      << "legacySessionStateRegistered=" << legacySessionStateService.serviceRegistered()
                      << "globalMenuRegistered=" << globalMenuService.serviceRegistered()
                      << "captureRegistered=" << captureService.serviceRegistered()
                      << "screencastRegistered=" << screencastService.serviceRegistered()
                      << "inputCaptureRegistered=" << inputCaptureService.serviceRegistered()
                      << "folderSharingRegistered=" << folderSharingService.serviceRegistered()
                      << "slmCapabilitiesRegistered=" << capabilityService.serviceRegistered()
                      << "fileOperationsEmbedded=" << embedFileOps
                      << "fileOperationsRegistered="
                      << (fileOperationsService ? fileOperationsService->serviceRegistered() : false)
                      << "backend=" << daemonService.backend()
                      << "connected=" << daemonService.connected();

    return app.exec();
}
