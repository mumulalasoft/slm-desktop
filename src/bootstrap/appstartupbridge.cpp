#include "appstartupbridge.h"
#include "../services/power/powerbridge.h"

#include <QQmlContext>
#include <QTimer>
#include <array>
#include <utility>

#include "../services/network/networkmanager.h"
#include "../services/bluetooth/bluetoothmanager.h"
#include "../services/sound/soundmanager.h"
#include "../services/media/mediasessionmanager.h"
#include "../services/notifications/notificationmanager.h"
#include "../services/power/batterymanager.h"
#include "../../externalindicatorregistry.h"
#include "../../globalmenumanager.h"
#include "../services/indicator/statusnotifierhost.h"
#include "../services/portal/screencastprivacymodel.h"
#include "../services/portal/inputcaptureprivacymodel.h"
#include "../../appmodel.h"
#include "../../shortcutmodel.h"
#include "../../dockmodel.h"
#include "../core/workspace/spacesmanager.h"
#include "../core/execution/appexecutiongate.h"
#include "../core/execution/appcommandrouter.h"
#include "../../cursorcontroller.h"
#include "../core/icons/themeiconcontroller.h"
#include "../core/prefs/uipreferences.h"
#include "../core/workspace/windowingbackendmanager.h"
#include "../core/workspace/workspacemanager.h"
#include "../core/workspace/workspacestripmodel.h"
#include "../core/workspace/multitaskingcontroller.h"
#include "../core/workspace/windowthumbnaillayoutengine.h"
#include "../core/workspace/workspacepreviewmanager.h"
#include "../../screenshotmanager.h"
#include "../../portalchooserlogichelper.h"
#include "../../screenshotsavehelper.h"
#include "../../portaluibridge.h"
#include "../../src/apps/filemanager/include/filemanagerapi.h"
#include "../../src/apps/filemanager/include/filemanagermodel.h"
#include "../../src/apps/filemanager/include/filemanagermodelfactory.h"
#include "../apps/filemanager/ops/globalprogresscenter.h"
#include "../../tothespotservice.h"
#include "../../tothespotcontextmenuhelper.h"
#include "../../tothespottexthighlighter.h"
#include "../../metadataindexserver.h"
#include "../services/clipboard/ClipboardServiceClient.h"
#include "../core/motion/slmmotioncontroller.h"
#include "../core/shell/shellstatecontroller.h"
#include "../core/shell/shellinputrouter.h"
#include "../core/shell/shelllayerwatchdog.h"

namespace {
template <std::size_t N>
void setContextObjects(QQmlContext *context,
                       const std::array<std::pair<const char *, QObject *>, N> &entries)
{
    for (const auto &[name, object] : entries) {
        context->setContextProperty(QString::fromLatin1(name), object);
    }
}

void hideBatchProgress(GlobalProgressCenter *globalProgressCenter,
                       WindowingBackendManager *windowingBackendManager,
                       QTimer *globalProgressHideTimer,
                       QTimer *compositorProgressHideTimer)
{
    if (globalProgressHideTimer) {
        globalProgressHideTimer->stop();
    }
    if (compositorProgressHideTimer) {
        compositorProgressHideTimer->stop();
    }
    if (globalProgressCenter) {
        globalProgressCenter->hideProgress(QStringLiteral("filemanager.batch"));
    }
    if (windowingBackendManager) {
        windowingBackendManager->sendCommand(QStringLiteral("progress hide"));
    }
}
}

namespace AppStartupBridge {
void registerTopBarIndicatorContext(QQmlContext *context,
                                    NetworkManager *networkManager,
                                    BluetoothManager *bluetoothManager,
                                    SoundManager *soundManager,
                                    MediaSessionManager *mediaSessionManager,
                                    NotificationManager *notificationManager,
                                    BatteryManager *batteryManager,
                                    ExternalIndicatorRegistry *externalIndicatorRegistry,
                                    GlobalMenuManager *globalMenuManager,
                                    StatusNotifierHost *statusNotifierHost,
                                    ScreencastPrivacyModel *screencastPrivacyModel,
                                    InputCapturePrivacyModel *inputCapturePrivacyModel)
{
    const std::array<std::pair<const char *, QObject *>, 11> entries{{
        {"NetworkManager", networkManager},
        {"BluetoothManager", bluetoothManager},
        {"SoundManager", soundManager},
        {"MediaSessionManager", mediaSessionManager},
        {"NotificationManager", notificationManager},
        {"BatteryManager", batteryManager},
        {"ExternalIndicatorRegistry", externalIndicatorRegistry},
        {"GlobalMenuManager", globalMenuManager},
        {"StatusNotifierHost", statusNotifierHost},
        {"ScreencastPrivacyModel", screencastPrivacyModel},
        {"InputCapturePrivacyModel", inputCapturePrivacyModel},
    }};
    setContextObjects(context, entries);
}

void registerCoreContext(QQmlContext *context,
                         DesktopAppModel *appModel,
                         ShortcutModel *shortcutModel,
                         DockModel *dockModel,
                         SpacesManager *spacesManager,
                         AppExecutionGate *appExecutionGate,
                         AppCommandRouter *appCommandRouter,
                         CursorController *cursorController,
                         ThemeIconController *themeIconController,
                         QObject *desktopSettings,
                         UIPreferences *uiPreferences,
                         WindowingBackendManager *windowingBackendManager,
                         WorkspaceManager *workspaceManager,
                         WorkspaceStripModel *workspaceStripModel,
                         MultitaskingController *multitaskingController,
                         WindowThumbnailLayoutEngine *windowThumbnailLayoutEngine,
                         WorkspacePreviewManager *workspacePreviewManager,
                         ScreenshotManager *screenshotManager,
                         PortalChooserLogicHelper *portalChooserLogicHelper,
                         ScreenshotSaveHelper *screenshotSaveHelper,
                         PortalUiBridge *portalUiBridge,
                         FileManagerApi *fileManagerApi,
                         FileManagerModel *fileManagerModel,
                         FileManagerModelFactory *fileManagerModelFactory,
                         GlobalProgressCenter *globalProgressCenter,
                         TothespotService *tothespotService,
                         TothespotContextMenuHelper *tothespotContextMenuHelper,
                         TothespotTextHighlighter *tothespotTextHighlighter,
                         MetadataIndexServer *metadataIndexServer,
                         Slm::Clipboard::ClipboardServiceClient *clipboardServiceClient,
                         Slm::Motion::MotionController *motionController,
                         ShellStateController *shellStateController,
                         ShellInputRouter *shellInputRouter,
                         ShellLayerWatchdog *shellLayerWatchdog,
                         PowerBridge *powerBridge)
{
    const std::array<std::pair<const char *, QObject *>, 37> entries{{
        {"AppModel", appModel},
        {"AppManager", appModel},
        {"ShortcutModel", shortcutModel},
        {"DockModel", dockModel},
        {"SpacesManager", spacesManager},
        {"AppExecutionGate", appExecutionGate},
        {"AppCommandRouter", appCommandRouter},
        {"CursorController", cursorController},
        {"ThemeIconController", themeIconController},
        {"DesktopSettings", desktopSettings},
        {"UIPreferences", uiPreferences},
        {"WindowingBackend", windowingBackendManager},
        {"CompositorStateModel",
         windowingBackendManager ? windowingBackendManager->compositorStateObject() : nullptr},
        {"WorkspaceManager", workspaceManager},
        {"WorkspaceStripModel", workspaceStripModel},
        {"MultitaskingController", multitaskingController},
        {"WindowThumbnailLayoutEngine", windowThumbnailLayoutEngine},
        {"WorkspacePreviewManager", workspacePreviewManager},
        {"OverviewPreviewManager", workspacePreviewManager},
        {"ScreenshotManager", screenshotManager},
        {"PortalChooserLogicHelper", portalChooserLogicHelper},
        {"ScreenshotSaveHelper", screenshotSaveHelper},
        {"PortalUiBridge", portalUiBridge},
        {"FileManagerApi", fileManagerApi},
        {"FileManagerModel", fileManagerModel},
        {"FileManagerModelFactory", fileManagerModelFactory},
        {"GlobalProgressCenter", globalProgressCenter},
        {"TothespotService", tothespotService},
        {"TothespotContextMenuHelper", tothespotContextMenuHelper},
        {"TothespotTextHighlighter", tothespotTextHighlighter},
        {"MetadataIndexServer", metadataIndexServer},
        {"ClipboardServiceClient", clipboardServiceClient},
        {"MotionController", motionController},
        {"ShellStateController", shellStateController},
        {"ShellInputRouter", shellInputRouter},
        {"ShellLayerWatchdog", shellLayerWatchdog},
        {"PowerBridge", powerBridge},
    }};
    setContextObjects(context, entries);
}

void setStartupWindowContext(QQmlContext *context,
                             bool appStartWindowed,
                             int appStartWindowWidth,
                             int appStartWindowHeight)
{
    context->setContextProperty(QStringLiteral("AppStartWindowed"), appStartWindowed);
    context->setContextProperty(QStringLiteral("AppStartWindowWidth"), appStartWindowWidth);
    context->setContextProperty(QStringLiteral("AppStartWindowHeight"), appStartWindowHeight);
}

void wireGlobalBatchProgress(QObject *owner,
                             FileManagerApi *fileManagerApi,
                             WindowingBackendManager *windowingBackendManager,
                             GlobalProgressCenter *globalProgressCenter)
{
    auto *compositorProgressHideTimer = new QTimer(owner);
    compositorProgressHideTimer->setSingleShot(true);
    auto *globalProgressHideTimer = new QTimer(owner);
    globalProgressHideTimer->setSingleShot(true);

    QObject::connect(compositorProgressHideTimer, &QTimer::timeout, owner, [=]() {
        if (fileManagerApi && fileManagerApi->batchOperationActive()) {
            return;
        }
        if (windowingBackendManager) {
            windowingBackendManager->sendCommand(QStringLiteral("progress hide"));
        }
    });

    QObject::connect(globalProgressHideTimer, &QTimer::timeout, owner, [=]() {
        if (fileManagerApi && fileManagerApi->batchOperationActive()) {
            return;
        }
        if (globalProgressCenter) {
            globalProgressCenter->hideProgress(QStringLiteral("filemanager.batch"));
        }
    });

    QObject::connect(fileManagerApi, &FileManagerApi::batchOperationStateChanged, owner, [=]() {
        hideBatchProgress(globalProgressCenter,
                          windowingBackendManager,
                          globalProgressHideTimer,
                          compositorProgressHideTimer);
    });

    QObject::connect(windowingBackendManager,
                     &WindowingBackendManager::connectedChanged,
                     owner,
                     [=]() {
        hideBatchProgress(globalProgressCenter,
                          windowingBackendManager,
                          globalProgressHideTimer,
                          compositorProgressHideTimer);
    });

    QObject::connect(globalProgressCenter,
                     &GlobalProgressCenter::pauseRequested,
                     owner,
                     [=](const QString &source) {
        if (source == QStringLiteral("filemanager.batch") && fileManagerApi) {
            fileManagerApi->pauseActiveBatchOperation();
        }
    });

    QObject::connect(globalProgressCenter,
                     &GlobalProgressCenter::resumeRequested,
                     owner,
                     [=](const QString &source) {
        if (source == QStringLiteral("filemanager.batch") && fileManagerApi) {
            fileManagerApi->resumeActiveBatchOperation();
        }
    });

    QObject::connect(globalProgressCenter,
                     &GlobalProgressCenter::cancelRequested,
                     owner,
                     [=](const QString &source) {
        if (source == QStringLiteral("filemanager.batch") && fileManagerApi) {
            fileManagerApi->cancelActiveBatchOperation(QStringLiteral("keep-completed"));
        }
    });

    hideBatchProgress(globalProgressCenter,
                      windowingBackendManager,
                      globalProgressHideTimer,
                      compositorProgressHideTimer);
}
}
